#pragma once

#include "razorleak/RazorLeakEngine.h"

#include <yup_audio_processors/yup_audio_processors.h>
#include <yup_gui/yup_gui.h>

#include <atomic>
#include <memory>
#include <vector>

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

    bool isResizable() const override;
    bool shouldPreserveAspectRatio() const override;
    yup::Size<int> getPreferredSize() const override;
    void paint (yup::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;

    yup::String title;
    std::atomic<float>& inputMeterLeft;
    std::atomic<float>& inputMeterRight;
    std::atomic<float>& outputMeterLeft;
    std::atomic<float>& outputMeterRight;
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    std::atomic<bool>& auditionEnabled;
    std::atomic<int>& auditionType;
    std::unique_ptr<yup::ToggleButton> auditionToggle;
    std::unique_ptr<yup::ComboBox> auditionSelector;
#endif
    std::unique_ptr<yup::Label> titleLabel;
    std::vector<yup::AudioParameter::Ptr> parameters;
    std::vector<std::unique_ptr<yup::Label>> labels;
    std::vector<std::unique_ptr<yup::Slider>> sliders;
    std::vector<std::unique_ptr<yup::Label>> valueLabels;
};

} // namespace razorleak::plugin
