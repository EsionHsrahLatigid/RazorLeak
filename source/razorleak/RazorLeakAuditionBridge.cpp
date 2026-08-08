#include "razorleak/RazorLeakEngine.h"

#include <algorithm>
#include <cmath>

#if ! defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
#error "RazorLeakAuditionBridge.cpp is standalone-only"
#endif

namespace razorleak
{

void RazorLeakAuditionBridge::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1000.0 ? newSampleRate : 44100.0;
    reset();
}

void RazorLeakAuditionBridge::reset() noexcept
{
    sampleCounter = 0;
    noise.reset (0x7a1f1201u);
}

StereoFrame RazorLeakAuditionBridge::nextInput() noexcept
{
    if (! enabled)
        return {};

    const auto rate = static_cast<int> (sampleRate);
    auto value = 0.0f;
    if (type == Type::Impulse)
    {
        value = (sampleCounter % std::max (64, rate / 3)) == 0 ? 0.95f : 0.0f;
    }
    else if (type == Type::EdgePulse)
    {
        const auto phase = sampleCounter % std::max (16, rate / 80);
        value = phase < 4 ? 0.55f : (phase < 8 ? -0.55f : 0.0f);
    }
    else
    {
        const auto phase = sampleCounter % 37;
        value = (phase < 18 ? 0.18f : -0.18f) + noise.nextSigned() * 0.022f;
    }

    ++sampleCounter;
    return { value, -0.78f * value };
}

} // namespace razorleak
