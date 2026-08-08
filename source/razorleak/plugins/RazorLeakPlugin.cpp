#include "razorleak/plugins/RazorLeakPlugin.h"

#include "razorleak/ParameterGridEditor.h"
#include "razorleak/ProductState.h"

#include <algorithm>
#include <array>

namespace razorleak::plugin
{
namespace
{

constexpr std::size_t presetParameterCount = 6;

constexpr std::array<std::array<float, presetParameterCount>, 4> presetValues {{
    {{ 0.34f, 0.24f, 0.26f, 0.00f, 0.62f, -1.0f }},
    {{ 0.78f, 0.72f, 0.42f, 0.16f, 0.86f, -2.0f }},
    {{ 0.92f, 0.64f, 0.58f, -0.72f, 0.93f, -3.0f }},
    {{ 0.68f, 0.96f, 0.34f, 0.48f, 1.00f, -4.0f }}
}};

yup::AudioParameter::Ptr makeParameter (const char* id,
                                        const char* name,
                                        int hostID,
                                        float minValue,
                                        float maxValue,
                                        float defaultValue,
                                        yup::AudioParameter::ParameterUnit unit,
                                        float smoothingMs)
{
    return yup::AudioParameterBuilder()
        .withID (id)
        .withName (name)
        .withHostID (static_cast<yup::uint32> (hostID))
        .withRange (yup::NormalisableRange<float> (minValue, maxValue))
        .withDefault (defaultValue)
        .withSmoothing (smoothingMs)
        .withModulatable (true)
        .withUnit (unit)
        .build();
}

} // namespace

RazorLeakPlugin::RazorLeakPlugin()
    : yup::AudioProcessor ("RazorLeak",
                           yup::AudioBusLayout ({
                                                    yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Input, 2),
                                                },
                                                {
                                                    yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Output, 2),
                                                }))
{
    parameters[slice] = makeParameter ("slice", "Slice", slice, 0.0f, 1.0f, 0.42f, yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[leak] = makeParameter ("leak", "Leak", leak, 0.0f, 1.0f, 0.36f, yup::AudioParameter::ParameterUnit::Percent, 24.0f);
    parameters[time] = makeParameter ("time", "Time", time, 0.0f, 1.0f, 0.50f, yup::AudioParameter::ParameterUnit::Percent, 30.0f);
    parameters[bias] = makeParameter ("bias", "Bias", bias, -1.0f, 1.0f, 0.0f, yup::AudioParameter::ParameterUnit::Generic, 22.0f);
    parameters[mix] = makeParameter ("mix", "Mix", mix, 0.0f, 1.0f, 0.72f, yup::AudioParameter::ParameterUnit::Percent, 20.0f);
    parameters[output] = makeParameter ("output", "Output", output, -18.0f, 6.0f, 0.0f, yup::AudioParameter::ParameterUnit::Decibels, 12.0f);

    for (const auto& parameter : parameters)
        addParameter (parameter);

    syncParameterValuesFromParameters();
    updateEngineParameters();
}

void RazorLeakPlugin::prepareToPlay (const yup::AudioSpec& spec)
{
    engine.prepare (spec.sampleRate);
    engine.reset();
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    audition.prepare (spec.sampleRate);
#endif

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i] = yup::AudioParameterHandle (*parameters[i], spec.sampleRate);

    syncParameterValuesFromParameters();
    updateEngineParameters();
}

void RazorLeakPlugin::releaseResources()
{
}

void RazorLeakPlugin::processBlock (yup::AudioProcessContext<float>& context)
{
    auto& audio = context.audio;
    const auto numSamples = audio.getNumSamples();
    const auto numChannels = audio.getNumChannels();

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i].prepareBlock (context.params, parameters[i]->getIndexInContainer());

    auto* left = numChannels > 0 ? audio.getWritePointer (0) : nullptr;
    auto* right = numChannels > 1 ? audio.getWritePointer (1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (auto& handle : parameterHandles)
            handle.advanceToSample (sample);

        for (std::size_t i = 0; i < parameterHandles.size(); ++i)
            currentParameterValues[i] = parameterHandles[i].getNextValue();

        if ((sample % parameterUpdateCadenceSamples) == 0)
            updateEngineParameters();

        if (left != nullptr && right != nullptr)
        {
            auto inputLeft = left[sample];
            auto inputRight = right[sample];
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
            audition.setEnabled (auditionEnabled.load (std::memory_order_relaxed));
            audition.setType (static_cast<RazorLeakAuditionBridge::Type> (std::clamp (auditionType.load (std::memory_order_relaxed), 0, 2)));
            if (audition.isEnabled())
            {
                const auto generated = audition.nextInput();
                inputLeft = generated.left;
                inputRight = generated.right;
            }
#endif
            const auto frame = engine.processSample (inputLeft, inputRight);
            left[sample] = frame.left;
            right[sample] = frame.right;
        }
        else if (left != nullptr)
        {
            const auto frame = engine.processSample (left[sample], left[sample]);
            left[sample] = frame.left;
        }

        for (int channel = 2; channel < numChannels; ++channel)
            audio.getWritePointer (channel)[sample] = 0.0f;

        if ((sample % meterPublishCadenceSamples) == 0)
            publishMeters();
    }

    publishMeters();
    context.midi.clear();
}

void RazorLeakPlugin::flush()
{
    engine.reset();
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    audition.reset();
#endif
}

bool RazorLeakPlugin::acceptsMidi() const noexcept
{
    return false;
}

bool RazorLeakPlugin::producesMidi() const noexcept
{
    return false;
}

int RazorLeakPlugin::getCurrentPreset() const noexcept
{
    return currentPreset.load (std::memory_order_relaxed);
}

void RazorLeakPlugin::setCurrentPreset (int index) noexcept
{
    if (! yup::isPositiveAndBelow (index, static_cast<int> (presetValues.size())))
        return;

    currentPreset.store (index, std::memory_order_relaxed);
    for (std::size_t i = 0; i < parameters.size(); ++i)
        parameters[i]->setValue (presetValues[static_cast<std::size_t> (index)][i]);
}

int RazorLeakPlugin::getNumPresets() const
{
    return static_cast<int> (presetNames.size());
}

yup::String RazorLeakPlugin::getPresetName (int index) const
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        return presetNames[static_cast<std::size_t> (index)];
    return "Invalid Preset";
}

void RazorLeakPlugin::setPresetName (int index, yup::StringRef newName)
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        presetNames[static_cast<std::size_t> (index)] = newName;
}

yup::Result RazorLeakPlugin::loadStateFromMemory (const yup::MemoryBlock& data)
{
    int loadedPreset = 0;
    const auto result = loadProductState (*this, data, razorLeakStateMagic, razorLeakStateVersion, getNumPresets(), loadedPreset);
    if (result.failed())
        return result;

    currentPreset.store (loadedPreset, std::memory_order_relaxed);
    return yup::Result::ok();
}

yup::Result RazorLeakPlugin::saveStateIntoMemory (yup::MemoryBlock& data)
{
    return saveProductState (*this, data, razorLeakStateMagic, razorLeakStateVersion, currentPreset.load (std::memory_order_relaxed));
}

bool RazorLeakPlugin::hasEditor() const
{
    return true;
}

yup::AudioProcessorEditor* RazorLeakPlugin::createEditor()
{
    return new ParameterGridEditor (*this,
                                    "RazorLeak",
                                    inputMeterLeft,
                                    inputMeterRight,
                                    outputMeterLeft,
                                    outputMeterRight
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
                                    , auditionEnabled
                                    , auditionType
#endif
                                    );
}

void RazorLeakPlugin::syncParameterValuesFromParameters() noexcept
{
    for (std::size_t i = 0; i < parameters.size(); ++i)
        currentParameterValues[i] = parameters[i]->getValue();
}

void RazorLeakPlugin::updateEngineParameters()
{
    RazorLeakParameters engineParameters;
    engineParameters.slice = currentParameterValues[slice];
    engineParameters.leak = currentParameterValues[leak];
    engineParameters.time = currentParameterValues[time];
    engineParameters.bias = currentParameterValues[bias];
    engineParameters.mix = currentParameterValues[mix];
    engineParameters.output = currentParameterValues[output];

    engine.setParameters (engineParameters);
}

void RazorLeakPlugin::publishMeters() noexcept
{
    const auto snapshot = engine.getAndClearMeters();
    inputMeterLeft.store (snapshot.inputPeakLeft, std::memory_order_relaxed);
    inputMeterRight.store (snapshot.inputPeakRight, std::memory_order_relaxed);
    outputMeterLeft.store (snapshot.outputPeakLeft, std::memory_order_relaxed);
    outputMeterRight.store (snapshot.outputPeakRight, std::memory_order_relaxed);
}

} // namespace razorleak::plugin

extern "C" yup::AudioProcessor* createPluginProcessor()
{
    return new razorleak::plugin::RazorLeakPlugin();
}
