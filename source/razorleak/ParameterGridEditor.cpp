#include "razorleak/ParameterGridEditor.h"

#include <ehl/yup_plugin_ui/EhlPluginTheme.h>

#include <algorithm>

namespace razorleak::plugin
{

ParameterGridEditor::ParameterGridEditor (yup::AudioProcessor& processor,
                                          yup::StringRef newTitle,
                                          std::atomic<float>& newInputMeterLeft,
                                          std::atomic<float>& newInputMeterRight,
                                          std::atomic<float>& newOutputMeterLeft,
                                          std::atomic<float>& newOutputMeterRight
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
                                          , std::atomic<bool>& newAuditionEnabled
                                          , std::atomic<int>& newAuditionType
#endif
                                          )
    : title (newTitle)
    , inputMeterLeft (newInputMeterLeft)
    , inputMeterRight (newInputMeterRight)
    , outputMeterLeft (newOutputMeterLeft)
    , outputMeterRight (newOutputMeterRight)
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    , auditionEnabled (newAuditionEnabled)
    , auditionType (newAuditionType)
#endif
{
    const auto processorParameters = processor.getParameters();
    parameters.assign (processorParameters.begin(), processorParameters.end());

    titleLabel = std::make_unique<yup::Label>();
    titleLabel->setText (title, yup::dontSendNotification);
    titleLabel->setJustification (yup::Justification::centerLeft);
    ehl::ui::styleLabel (*titleLabel, ehl::ui::TextRole::primary);
    addAndMakeVisible (*titleLabel);

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionToggle = std::make_unique<ehl::ui::CommandButton> ("Audition");
    auditionToggle->setMouseCursor (yup::MouseCursor::Hand);
    auditionToggle->setClickingGrabFocus (false);
    auditionToggle->onClick = [this]
    {
        auditionEnabled.store (! auditionEnabled.load (std::memory_order_relaxed), std::memory_order_relaxed);
        refreshAuditionControls();
    };
    addAndMakeVisible (*auditionToggle);

    auditionSelector = std::make_unique<ehl::ui::CommandButton> ("Impulse");
    auditionSelector->setMouseCursor (yup::MouseCursor::Hand);
    auditionSelector->setClickingGrabFocus (false);
    auditionSelector->onClick = [this]
    {
        auditionType.store ((auditionType.load (std::memory_order_relaxed) + 1) % 3, std::memory_order_relaxed);
        refreshAuditionControls();
    };
    addAndMakeVisible (*auditionSelector);
#endif

    inputMeterLabel = std::make_unique<yup::Label>();
    inputMeterLabel->setText ("In", yup::dontSendNotification);
    inputMeterLabel->setJustification (yup::Justification::centerLeft);
    ehl::ui::styleLabel (*inputMeterLabel, ehl::ui::TextRole::secondary);
    addAndMakeVisible (*inputMeterLabel);

    outputMeterLabel = std::make_unique<yup::Label>();
    outputMeterLabel->setText ("Out", yup::dontSendNotification);
    outputMeterLabel->setJustification (yup::Justification::centerLeft);
    ehl::ui::styleLabel (*outputMeterLabel, ehl::ui::TextRole::secondary);
    addAndMakeVisible (*outputMeterLabel);

    inputMeter = std::make_unique<ehl::ui::StripMeter> (ehl::ui::mid);
    outputMeter = std::make_unique<ehl::ui::StripMeter> (ehl::ui::paper);
    addAndMakeVisible (*inputMeter);
    addAndMakeVisible (*outputMeter);

    labels.reserve (parameters.size());
    sliders.reserve (parameters.size());
    valueLabels.reserve (parameters.size());

    for (const auto& parameter : parameters)
    {
        auto label = std::make_unique<yup::Label>();
        label->setText (parameter->getName(), yup::dontSendNotification);
        label->setJustification (yup::Justification::center);
        ehl::ui::styleLabel (*label, ehl::ui::TextRole::secondary);
        addAndMakeVisible (*label);
        labels.push_back (std::move (label));

        auto slider = std::make_unique<ehl::ui::PixelSlider> (yup::Slider::RotaryVerticalDrag);
        slider->setRange (parameter->getMinimumValue(),
                          parameter->getMaximumValue(),
                          parameter->isStepped() ? 1.0 : 0.0);
        slider->setDefaultValue (parameter->getDefaultValue());
        slider->setValue (parameter->getValue(), yup::dontSendNotification);
        slider->setTextBoxStyle (yup::Slider::NoTextBox);
        slider->setPopupDisplayEnabled (false);
        slider->setMouseCursor (yup::MouseCursor::Hand);
        slider->onDragStart = [this, parameter] (const yup::MouseEvent&)
        {
            takeKeyboardFocus();
            parameter->beginChangeGesture();
        };
        slider->onValueChanged = [parameter] (double value)
        {
            parameter->setValueNotifyingHost (static_cast<float> (value));
        };
        slider->onDragEnd = [this, parameter] (const yup::MouseEvent&)
        {
            takeKeyboardFocus();
            parameter->endChangeGesture();
        };
        addAndMakeVisible (*slider);
        sliders.push_back (std::move (slider));

        auto valueLabel = std::make_unique<yup::Label>();
        valueLabel->setText (parameter->toString(), yup::dontSendNotification);
        valueLabel->setJustification (yup::Justification::center);
        ehl::ui::styleLabel (*valueLabel, ehl::ui::TextRole::primary);
        addAndMakeVisible (*valueLabel);
        valueLabels.push_back (std::move (valueLabel));
    }

    setSize (getPreferredSize().to<float>());
    refreshAuditionControls();
    startTimerHz (30);
}

ParameterGridEditor::~ParameterGridEditor() = default;

bool ParameterGridEditor::isResizable() const
{
    return true;
}

bool ParameterGridEditor::shouldPreserveAspectRatio() const
{
    return true;
}

yup::Size<int> ParameterGridEditor::getPreferredSize() const
{
    return ehl::ui::preferredSize;
}

void ParameterGridEditor::paint (yup::Graphics& graphics)
{
    ehl::ui::paintEditorBackground (graphics, getWidth(), getHeight());
}

void ParameterGridEditor::resized()
{
    constexpr int columns = 7;
    constexpr float margin = 16.0f;
    constexpr float top = 128.0f;
    constexpr float gap = 8.0f;
    constexpr float labelHeight = 24.0f;
    constexpr float valueHeight = 24.0f;
    constexpr float controlSize = 72.0f;

    const auto bounds = getLocalBounds();
    const auto cellWidth = (bounds.getWidth() - 2.0f * margin - gap * (columns - 1)) / columns;
    const auto rows = std::max (1, static_cast<int> ((sliders.size() + columns - 1) / columns));
    const auto availableHeight = bounds.getHeight() - top - margin;
    const auto cellHeight = (availableHeight - gap * (rows - 1)) / rows;

    titleLabel->setBounds (20.0f, 8.0f, bounds.getWidth() - 40.0f, 28.0f);
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionToggle->setBounds (margin, 72.0f, 104.0f, 28.0f);
    auditionSelector->setBounds (margin + 112.0f, 72.0f, 88.0f, 28.0f);
#endif
    const auto meterX =
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
        margin + 216.0f;
#else
        margin;
#endif
    const auto meterWidth = std::max (90.0f, (bounds.getWidth() - margin - meterX - gap) * 0.5f);
    inputMeterLabel->setBounds (meterX, 68.0f, 28.0f, 16.0f);
    inputMeter->setBounds (meterX + 28.0f, 76.0f, meterWidth - 28.0f, 12.0f);
    outputMeterLabel->setBounds (meterX + meterWidth + gap, 68.0f, 32.0f, 16.0f);
    outputMeter->setBounds (meterX + meterWidth + gap + 32.0f, 76.0f, meterWidth - 32.0f, 12.0f);

    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        const auto column = static_cast<int> (i) % columns;
        const auto row = static_cast<int> (i) / columns;
        const auto x = margin + column * (cellWidth + gap);
        const auto y = top + row * (cellHeight + gap);
        const auto inset = rows > 1 ? 4.0f : 12.0f;
        const auto labelY = y + inset;
        const auto valueY = y + cellHeight - valueHeight - inset;
        const auto controlTop = labelY + labelHeight;
        const auto controlBottom = valueY;
        const auto fittedControlSize = std::min ({ controlSize,
                                                   cellWidth - 8.0f,
                                                   std::max (20.0f, controlBottom - controlTop) });
        const auto controlX = x + 0.5f * (cellWidth - fittedControlSize);
        const auto controlY = controlTop + 0.5f * (controlBottom - controlTop - fittedControlSize);

        labels[i]->setBounds (x + 2.0f, labelY, cellWidth - 4.0f, labelHeight);
        sliders[i]->setBounds (controlX, controlY, fittedControlSize, fittedControlSize);
        valueLabels[i]->setBounds (x + 2.0f, valueY, cellWidth - 4.0f, valueHeight);
    }
}

void ParameterGridEditor::timerCallback()
{
    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        if (! sliders[i]->isCurrentlyBeingDragged())
            sliders[i]->setValue (parameters[i]->getValue(), yup::dontSendNotification);
        valueLabels[i]->setText (parameters[i]->toString(), yup::dontSendNotification);
    }

    const auto inPeak = std::max (inputMeterLeft.load (std::memory_order_relaxed),
                                  inputMeterRight.load (std::memory_order_relaxed));
    const auto outPeak = std::max (outputMeterLeft.load (std::memory_order_relaxed),
                                   outputMeterRight.load (std::memory_order_relaxed));
    inputMeter->setLevel (inPeak);
    outputMeter->setLevel (outPeak);
    refreshAuditionControls();
}

void ParameterGridEditor::refreshAuditionControls()
{
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    if (auditionToggle == nullptr || auditionSelector == nullptr)
        return;

    const auto enabled = auditionEnabled.load (std::memory_order_relaxed);
    const auto type = std::clamp (auditionType.load (std::memory_order_relaxed), 0, 2);
    auditionToggle->setButtonText (enabled ? "Audition On" : "Audition Off");
    auditionSelector->setButtonText (type == 0 ? "Impulse" : (type == 1 ? "Edge" : "Scanline"));
    auditionToggle->setSelected (enabled);
    auditionSelector->setSelected (type != 0);
#endif
}

} // namespace razorleak::plugin
