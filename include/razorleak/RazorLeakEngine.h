#pragma once

#include "razorleak/RazorLeakDspPrimitives.h"

#include <array>
#include <cstddef>

namespace razorleak
{

inline constexpr std::array<char, 4> razorLeakStateMagic { 'R', 'z', 'L', '1' };
inline constexpr int razorLeakStateVersion = 1;

struct RazorLeakParameters
{
    float slice = 0.42f;
    float leak = 0.36f;
    float time = 0.50f;
    float bias = 0.0f;
    float mix = 0.72f;
    float output = 0.0f;
};

struct RazorLeakMeters
{
    float inputPeakLeft = 0.0f;
    float inputPeakRight = 0.0f;
    float outputPeakLeft = 0.0f;
    float outputPeakRight = 0.0f;
};

class RazorLeakEngine
{
public:
    RazorLeakEngine();

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setParameters (const RazorLeakParameters& parameters) noexcept;

    [[nodiscard]] StereoFrame processSample (float inputLeft, float inputRight) noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] RazorLeakMeters getAndClearMeters() noexcept;
    [[nodiscard]] float getFeedbackCoefficient() const noexcept { return feedbackCoefficient; }
    [[nodiscard]] int getBaseDelaySamples() const noexcept { return baseDelaySamples; }

private:
    static constexpr std::size_t numChannels = 2;
    static constexpr std::size_t delaySize = 4096;
    static constexpr int maxDelaySamples = static_cast<int> (delaySize) - 8;

    struct ClampedParameters
    {
        float slice = 0.42f;
        float leak = 0.36f;
        float time = 0.50f;
        float bias = 0.0f;
        float mix = 0.72f;
        float output = 0.0f;
    };

    [[nodiscard]] float readDelay (std::size_t channel, float delaySamples) const noexcept;
    [[nodiscard]] float processChannel (float input, std::size_t channel, float edge) noexcept;
    void updateDerivedParameters() noexcept;
    void updateMeters (StereoFrame input, StereoFrame output) noexcept;

    ClampedParameters params;
    double sampleRate = 44100.0;
    float feedbackCoefficient = 0.0f;
    float outputGain = 1.0f;
    int baseDelaySamples = 96;
    int shearSamples = 8;
    float writeBlend = 0.0f;

    std::array<std::array<float, delaySize>, numChannels> delay {};
    std::array<Biquad, numChannels> edgeHighpass {};
    std::array<float, numChannels> previousInput {};
    std::array<float, numChannels> leakState {};
    std::size_t writeIndex = 0;
    RazorLeakMeters meters {};
};

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
class RazorLeakAuditionBridge
{
public:
    enum class Type
    {
        Impulse,
        EdgePulse,
        Scanline
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setEnabled (bool shouldEnable) noexcept { enabled = shouldEnable; }
    void setType (Type newType) noexcept { type = newType; }

    [[nodiscard]] bool isEnabled() const noexcept { return enabled; }
    [[nodiscard]] Type getType() const noexcept { return type; }
    [[nodiscard]] StereoFrame nextInput() noexcept;

private:
    double sampleRate = 44100.0;
    bool enabled = false;
    Type type = Type::EdgePulse;
    int sampleCounter = 0;
    DeterministicNoise noise;
};
#endif

} // namespace razorleak
