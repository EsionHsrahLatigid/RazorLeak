#pragma once

#include "razorleak/RazorLeakEngine.h"

#include <yup_audio_processors/yup_audio_processors.h>
#include <yup_gui/yup_gui.h>

#include <atomic>
#include <memory>
#include <vector>

namespace razorleak::plugin
{

} // namespace razorleak::plugin

namespace ehl::ui
{
class CommandButton;
class StripMeter;
}

namespace razorleak::plugin
{

class ParameterGridEditor final
    : public yup::AudioProcessorEditor
    , private yup::Timer
{
public:
    ParameterGridEditor (yup::AudioProcessor& processor,
                         yup::StringRef title,
                         std::atomic<float>& inputMeterLeft,
                         std::atomic<float>& inputMeterRight,
                         std::atomic<float>& outputMeterLeft,
                         std::atomic<float>& outputMeterRight
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
                         , std::atomic<bool>& auditionEnabled
                         , std::atomic<int>& auditionType
#endif
                         );
    ~ParameterGridEditor() override;

    bool isResizable() const override;
    bool shouldPreserveAspectRatio() const override;
    yup::Size<int> getPreferredSize() const override;
    void paint (yup::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshAuditionControls();

    yup::String title;
    std::atomic<float>& inputMeterLeft;
    std::atomic<float>& inputMeterRight;
    std::atomic<float>& outputMeterLeft;
    std::atomic<float>& outputMeterRight;
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    std::atomic<bool>& auditionEnabled;
    std::atomic<int>& auditionType;
    std::unique_ptr<ehl::ui::CommandButton> auditionToggle;
    std::unique_ptr<ehl::ui::CommandButton> auditionSelector;
#endif
    std::unique_ptr<yup::Label> titleLabel;
    std::unique_ptr<yup::Label> inputMeterLabel;
    std::unique_ptr<yup::Label> outputMeterLabel;
    std::unique_ptr<ehl::ui::StripMeter> inputMeter;
    std::unique_ptr<ehl::ui::StripMeter> outputMeter;
    std::vector<yup::AudioParameter::Ptr> parameters;
    std::vector<std::unique_ptr<yup::Label>> labels;
    std::vector<std::unique_ptr<yup::Slider>> sliders;
    std::vector<std::unique_ptr<yup::Label>> valueLabels;
};

} // namespace razorleak::plugin
