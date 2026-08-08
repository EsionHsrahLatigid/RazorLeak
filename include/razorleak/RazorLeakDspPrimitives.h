#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace razorleak
{

struct StereoFrame
{
    float left = 0.0f;
    float right = 0.0f;
};

inline float clampFinite (float value, float low, float high, float fallback) noexcept
{
    if (! std::isfinite (value))
        value = fallback;
    return std::clamp (value, low, high);
}

inline float sanitizeAudio (float value) noexcept
{
    return clampFinite (value, -16.0f, 16.0f, 0.0f);
}

inline float flushDenormal (float value) noexcept
{
    return std::fabs (value) < 1.0e-20f ? 0.0f : value;
}

inline float softClip (float input, float drive = 1.0f) noexcept
{
    const auto safeInput = sanitizeAudio (input);
    const auto safeDrive = clampFinite (drive, 0.0f, 32.0f, 1.0f);
    return std::tanh (safeInput * safeDrive);
}

inline float decibelsToGain (float decibels) noexcept
{
    return std::pow (10.0f, clampFinite (decibels, -60.0f, 12.0f, 0.0f) / 20.0f);
}

class Biquad
{
public:
    void reset() noexcept
    {
        z1 = 0.0f;
        z2 = 0.0f;
    }

    void setHighPass (double sampleRate, float frequency, float quality = 0.70710678f) noexcept
    {
        const auto safeRate = std::isfinite (sampleRate) && sampleRate > 1000.0 ? sampleRate : 44100.0;
        const auto nyquist = static_cast<float> (safeRate * 0.5);
        const auto safeFrequency = clampFinite (frequency, 20.0f, nyquist * 0.92f, 1800.0f);
        const auto safeQuality = clampFinite (quality, 0.1f, 8.0f, 0.70710678f);
        const auto omega = 2.0f * std::numbers::pi_v<float> * safeFrequency / static_cast<float> (safeRate);
        const auto sine = std::sin (omega);
        const auto cosine = std::cos (omega);
        const auto alpha = sine / (2.0f * safeQuality);
        const auto inverseA0 = 1.0f / (1.0f + alpha);

        b0 = 0.5f * (1.0f + cosine) * inverseA0;
        b1 = -(1.0f + cosine) * inverseA0;
        b2 = b0;
        a1 = -2.0f * cosine * inverseA0;
        a2 = (1.0f - alpha) * inverseA0;
    }

    [[nodiscard]] float process (float input) noexcept
    {
        const auto safeInput = sanitizeAudio (input);
        const auto output = b0 * safeInput + z1;
        z1 = flushDenormal (b1 * safeInput - a1 * output + z2);
        z2 = flushDenormal (b2 * safeInput - a2 * output);
        return std::isfinite (output) ? output : 0.0f;
    }

private:
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float z1 = 0.0f;
    float z2 = 0.0f;
};

class DeterministicNoise
{
public:
    void reset (std::uint32_t seed) noexcept
    {
        state = seed != 0u ? seed : 0x52a11eafu;
    }

    [[nodiscard]] std::uint32_t nextWord() noexcept
    {
        auto value = state;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        state = value != 0u ? value : 0x52a11eafu;
        return state;
    }

    [[nodiscard]] float nextSigned() noexcept
    {
        constexpr auto scale = 1.0 / 2147483648.0;
        return static_cast<float> (static_cast<double> (nextWord()) * scale - 1.0);
    }

private:
    std::uint32_t state = 0x52a11eafu;
};

} // namespace razorleak
