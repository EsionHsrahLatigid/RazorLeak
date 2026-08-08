#include "razorleak/ParameterGridEditor.h"

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
    addAndMakeVisible (*titleLabel);

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionToggle = std::make_unique<yup::ToggleButton> ("Audition");
    auditionToggle->setButtonText ("Audition");
    auditionToggle->setToggleState (auditionEnabled.load (std::memory_order_relaxed), yup::dontSendNotification);
    auditionToggle->onClick = [this]
    {
        auditionEnabled.store (auditionToggle->getToggleState(), std::memory_order_relaxed);
    };
    addAndMakeVisible (*auditionToggle);

    auditionSelector = std::make_unique<yup::ComboBox>();
    auditionSelector->addItem ("Impulse", 1);
    auditionSelector->addItem ("Edge", 2);
    auditionSelector->addItem ("Scanline", 3);
    auditionSelector->setSelectedId (auditionType.load (std::memory_order_relaxed) + 1, yup::dontSendNotification);
    auditionSelector->onSelectedItemChanged = [this]
    {
        auditionType.store (std::max (0, auditionSelector->getSelectedId() - 1), std::memory_order_relaxed);
    };
    addAndMakeVisible (*auditionSelector);
#endif

    labels.reserve (parameters.size());
    sliders.reserve (parameters.size());
    valueLabels.reserve (parameters.size());

    for (const auto& parameter : parameters)
    {
        auto label = std::make_unique<yup::Label>();
        label->setText (parameter->getName(), yup::dontSendNotification);
        label->setJustification (yup::Justification::center);
        addAndMakeVisible (*label);
        labels.push_back (std::move (label));

        auto slider = std::make_unique<yup::Slider> (yup::Slider::RotaryVerticalDrag);
        slider->setRange (parameter->getMinimumValue(),
                          parameter->getMaximumValue(),
                          parameter->isStepped() ? 1.0 : 0.0);
        slider->setDefaultValue (parameter->getDefaultValue());
        slider->setValue (parameter->getValue(), yup::dontSendNotification);
        slider->setTextBoxStyle (yup::Slider::NoTextBox);
        slider->setPopupDisplayEnabled (false);
        slider->setMouseCursor (yup::MouseCursor::Hand);
        slider->onDragStart = [parameter] (const yup::MouseEvent&) { parameter->beginChangeGesture(); };
        slider->onValueChanged = [parameter] (double value)
        {
            parameter->setValueNotifyingHost (static_cast<float> (value));
        };
        slider->onDragEnd = [parameter] (const yup::MouseEvent&) { parameter->endChangeGesture(); };
        addAndMakeVisible (*slider);
        sliders.push_back (std::move (slider));

        auto valueLabel = std::make_unique<yup::Label>();
        valueLabel->setText (parameter->toString(), yup::dontSendNotification);
        valueLabel->setJustification (yup::Justification::center);
        addAndMakeVisible (*valueLabel);
        valueLabels.push_back (std::move (valueLabel));
    }

    setSize (getPreferredSize().to<float>());
    startTimerHz (30);
}

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
    return { 920, 520 };
}

void ParameterGridEditor::paint (yup::Graphics& graphics)
{
    graphics.setFillColor (0xff050608u);
    graphics.fillAll();

    graphics.setFillColor (0xffe8eef2u);
    for (int x = 18; x < getWidth(); x += 42)
        graphics.fillRect (static_cast<float> (x), 0.0f, 1.0f, static_cast<float> (getHeight()));

    graphics.setFillColor (0xffff1844u);
    for (int y = 54; y < getHeight(); y += 37)
        graphics.fillRect (0.0f, static_cast<float> (y), static_cast<float> (getWidth()), 2.0f);

    const auto inPeak = std::max (inputMeterLeft.load (std::memory_order_relaxed),
                                  inputMeterRight.load (std::memory_order_relaxed));
    const auto outPeak = std::max (outputMeterLeft.load (std::memory_order_relaxed),
                                   outputMeterRight.load (std::memory_order_relaxed));
    graphics.setFillColor (0xff6fffe9u);
    graphics.fillRect (24.0f, 48.0f, 180.0f * std::clamp (inPeak, 0.0f, 1.0f), 5.0f);
    graphics.setFillColor (0xffffd166u);
    graphics.fillRect (24.0f, 58.0f, 180.0f * std::clamp (outPeak, 0.0f, 1.0f), 5.0f);
}

void ParameterGridEditor::resized()
{
    constexpr int columns = 6;
    constexpr float margin = 22.0f;
    constexpr float top = 96.0f;
    constexpr float gap = 12.0f;
    constexpr float labelHeight = 24.0f;
    constexpr float valueHeight = 24.0f;
    constexpr float controlGap = 4.0f;

    const auto bounds = getLocalBounds();
    const auto cellWidth = (bounds.getWidth() - 2.0f * margin - gap * (columns - 1)) / columns;
    const auto rows = std::max (1, static_cast<int> ((sliders.size() + columns - 1) / columns));
    const auto availableHeight = bounds.getHeight() - top - margin;
    const auto cellHeight = (availableHeight - gap * (rows - 1)) / rows;

    titleLabel->setBounds (24.0f, 12.0f, bounds.getWidth() - 48.0f, 30.0f);
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionToggle->setBounds (bounds.getWidth() - 230.0f, 46.0f, 96.0f, 24.0f);
    auditionSelector->setBounds (bounds.getWidth() - 126.0f, 44.0f, 102.0f, 28.0f);
#endif

    for (std::size_t i = 0; i < sliders.size(); ++i)
    {
        const auto column = static_cast<int> (i) % columns;
        const auto row = static_cast<int> (i) / columns;
        const auto x = margin + column * (cellWidth + gap);
        const auto y = top + row * (cellHeight + gap);
        const auto controlHeight = cellHeight - labelHeight - valueHeight - 2.0f * controlGap;
        const auto controlSize = std::max (20.0f, std::min (cellWidth - 8.0f, controlHeight));
        const auto controlX = x + 0.5f * (cellWidth - controlSize);
        const auto controlY = y + labelHeight + controlGap;

        labels[i]->setBounds (x, y, cellWidth, labelHeight);
        sliders[i]->setBounds (controlX, controlY, controlSize, controlSize);
        valueLabels[i]->setBounds (x, y + cellHeight - valueHeight, cellWidth, valueHeight);
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
    repaint();
}

} // namespace razorleak::plugin
