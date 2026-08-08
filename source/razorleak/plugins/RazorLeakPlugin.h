#pragma once

#include "razorleak/RazorLeakEngine.h"

#include <yup_audio_processors/yup_audio_processors.h>

#include <array>
#include <atomic>

namespace razorleak::plugin
{

class RazorLeakPlugin final : public yup::AudioProcessor
{
public:
    RazorLeakPlugin();

    void prepareToPlay (const yup::AudioSpec& spec) override;
    void releaseResources() override;
    void processBlock (yup::AudioProcessContext<float>& context) override;
    void flush() override;

    bool acceptsMidi() const noexcept override;
    bool producesMidi() const noexcept override;

    int getCurrentPreset() const noexcept override;
    void setCurrentPreset (int index) noexcept override;
    int getNumPresets() const override;
    yup::String getPresetName (int index) const override;
    void setPresetName (int index, yup::StringRef newName) override;

    yup::Result loadStateFromMemory (const yup::MemoryBlock& data) override;
    yup::Result saveStateIntoMemory (yup::MemoryBlock& data) override;

    bool hasEditor() const override;
    yup::AudioProcessorEditor* createEditor() override;

private:
    enum ParameterIndex
    {
        slice,
        leak,
        time,
        bias,
        mix,
        output,
        parameterCount
    };

    static constexpr int parameterUpdateCadenceSamples = 16;
    static constexpr int meterPublishCadenceSamples = 32;

    void syncParameterValuesFromParameters() noexcept;
    void updateEngineParameters();
    void publishMeters() noexcept;

    std::array<yup::AudioParameter::Ptr, parameterCount> parameters;
    std::array<yup::AudioParameterHandle, parameterCount> parameterHandles;
    std::array<float, parameterCount> currentParameterValues {};
    RazorLeakEngine engine;
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    RazorLeakAuditionBridge audition;
    std::atomic<bool> auditionEnabled { false };
    std::atomic<int> auditionType { 1 };
#endif

    std::atomic<float> inputMeterLeft { 0.0f };
    std::atomic<float> inputMeterRight { 0.0f };
    std::atomic<float> outputMeterLeft { 0.0f };
    std::atomic<float> outputMeterRight { 0.0f };
    std::atomic<int> currentPreset { 0 };
    std::array<yup::String, 4> presetNames {
        "Clean Cuts",
        "Edge Leak",
        "Biased Shred",
        "Scanline Spill"
    };
};

} // namespace razorleak::plugin
