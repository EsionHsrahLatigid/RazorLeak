#include "razorleak/RazorLeakEngine.h"

#include <algorithm>
#include <cmath>

namespace razorleak
{

namespace
{

[[nodiscard]] float limitPeak (float value) noexcept
{
    return clampFinite (softClip (value, 1.15f), -0.98f, 0.98f, 0.0f);
}

[[nodiscard]] float onePole (float previous, float target, float coefficient) noexcept
{
    return flushDenormal (target + coefficient * (previous - target));
}

} // namespace

RazorLeakEngine::RazorLeakEngine()
{
    prepare (44100.0);
}

void RazorLeakEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1000.0 ? newSampleRate : 44100.0;
    for (auto& filter : edgeHighpass)
        filter.setHighPass (sampleRate, 1900.0f, 0.72f);
    updateDerivedParameters();
    reset();
}

void RazorLeakEngine::reset() noexcept
{
    for (auto& channel : delay)
        channel.fill (0.0f);
    for (auto& filter : edgeHighpass)
        filter.reset();
    previousInput.fill (0.0f);
    leakState.fill (0.0f);
    writeIndex = 0;
    meters = {};
}

void RazorLeakEngine::setParameters (const RazorLeakParameters& parameters) noexcept
{
    params.slice = clampFinite (parameters.slice, 0.0f, 1.0f, 0.42f);
    params.leak = clampFinite (parameters.leak, 0.0f, 1.0f, 0.36f);
    params.time = clampFinite (parameters.time, 0.0f, 1.0f, 0.50f);
    params.bias = clampFinite (parameters.bias, -1.0f, 1.0f, 0.0f);
    params.mix = clampFinite (parameters.mix, 0.0f, 1.0f, 0.72f);
    params.output = clampFinite (parameters.output, -18.0f, 6.0f, 0.0f);
    updateDerivedParameters();
}

StereoFrame RazorLeakEngine::processSample (float inputLeft, float inputRight) noexcept
{
    const StereoFrame input { sanitizeAudio (inputLeft), sanitizeAudio (inputRight) };
    const auto mono = 0.5f * (input.left + input.right);
    const auto edgeLeft = std::fabs (edgeHighpass[0].process (input.left - previousInput[0]));
    const auto edgeRight = std::fabs (edgeHighpass[1].process (input.right - previousInput[1]));
    const auto edge = std::min (1.0f, std::max (edgeLeft, edgeRight) + 0.35f * std::fabs (mono));

    const StereoFrame wet {
        processChannel (input.left, 0, edge),
        processChannel (input.right, 1, edge)
    };

    previousInput[0] = input.left;
    previousInput[1] = input.right;

    const auto dry = 1.0f - params.mix;
    StereoFrame output {
        limitPeak ((input.left * dry + wet.left * params.mix) * outputGain),
        limitPeak ((input.right * dry + wet.right * params.mix) * outputGain)
    };

    updateMeters (input, output);
    writeIndex = (writeIndex + 1u) % delaySize;
    return output;
}

void RazorLeakEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto frame = processSample (left[i], right[i]);
        left[i] = frame.left;
        right[i] = frame.right;
    }
}

RazorLeakMeters RazorLeakEngine::getAndClearMeters() noexcept
{
    const auto snapshot = meters;
    meters = {};
    return snapshot;
}

float RazorLeakEngine::readDelay (std::size_t channel, float delaySamples) const noexcept
{
    const auto safeChannel = std::min (channel, numChannels - 1);
    const auto safeDelay = clampFinite (delaySamples, 1.0f, static_cast<float> (maxDelaySamples), 1.0f);
    auto readPosition = static_cast<float> (writeIndex) - safeDelay;
    while (readPosition < 0.0f)
        readPosition += static_cast<float> (delaySize);

    const auto index0 = static_cast<std::size_t> (readPosition) % delaySize;
    const auto index1 = (index0 + 1u) % delaySize;
    const auto fraction = readPosition - std::floor (readPosition);
    return delay[safeChannel][index0] * (1.0f - fraction) + delay[safeChannel][index1] * fraction;
}

float RazorLeakEngine::processChannel (float input, std::size_t channel, float edge) noexcept
{
    const auto biasSign = channel == 0 ? 1.0f : -1.0f;
    const auto shearBias = params.bias * biasSign;
    const auto shortTap = readDelay (channel, static_cast<float> (std::max (1, baseDelaySamples - shearSamples)));
    const auto mainTap = readDelay (channel, static_cast<float> (baseDelaySamples) + shearBias * static_cast<float> (shearSamples));
    const auto longTap = readDelay (channel, static_cast<float> (baseDelaySamples + shearSamples * 2));

    const auto smear = shortTap * (0.22f + 0.18f * params.bias * biasSign)
                     + mainTap * 0.58f
                     - longTap * (0.20f - 0.12f * params.bias * biasSign);
    const auto detector = std::min (1.0f, edge * (0.35f + params.leak * 2.2f));
    leakState[channel] = onePole (leakState[channel], detector, 0.88f - params.slice * 0.42f);

    const auto clippedLeak = softClip (smear * (1.0f + 3.0f * params.leak) + input * leakState[channel] * 0.42f,
                                       1.0f + params.slice * 5.0f);
    const auto feedback = clippedLeak * feedbackCoefficient;
    delay[channel][writeIndex] = flushDenormal (softClip (input + feedback, 1.0f + params.leak * 1.6f));

    return limitPeak (input * (0.18f + 0.16f * (1.0f - params.slice)) + clippedLeak * (0.82f + params.slice * 0.24f));
}

void RazorLeakEngine::updateDerivedParameters() noexcept
{
    const auto shortest = static_cast<int> (sampleRate * 0.0015);
    const auto longest = static_cast<int> (sampleRate * 0.045);
    const auto shapedTime = params.time * params.time;
    baseDelaySamples = std::clamp (shortest + static_cast<int> ((longest - shortest) * shapedTime),
                                   2,
                                   maxDelaySamples - 16);
    shearSamples = std::clamp (2 + static_cast<int> (params.slice * params.slice * 96.0f),
                               2,
                               std::max (2, baseDelaySamples / 2));
    feedbackCoefficient = std::min (0.915f, params.leak * (0.28f + params.slice * 0.58f));
    outputGain = decibelsToGain (params.output);
    writeBlend = params.slice;
}

void RazorLeakEngine::updateMeters (StereoFrame input, StereoFrame output) noexcept
{
    meters.inputPeakLeft = std::max (meters.inputPeakLeft, std::fabs (input.left));
    meters.inputPeakRight = std::max (meters.inputPeakRight, std::fabs (input.right));
    meters.outputPeakLeft = std::max (meters.outputPeakLeft, std::fabs (output.left));
    meters.outputPeakRight = std::max (meters.outputPeakRight, std::fabs (output.right));
}

} // namespace razorleak
