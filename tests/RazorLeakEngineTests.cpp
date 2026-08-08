#include "razorleak/RazorLeakEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <vector>

using razorleak::RazorLeakEngine;
using razorleak::RazorLeakParameters;
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
using razorleak::RazorLeakAuditionBridge;
#endif

namespace
{

constexpr int sampleRate = 48000;

struct Rendered
{
    std::vector<float> left;
    std::vector<float> right;
};

float peakOf (const std::vector<float>& samples, int start = 0) noexcept
{
    auto peak = 0.0f;
    for (std::size_t i = static_cast<std::size_t> (std::max (0, start)); i < samples.size(); ++i)
        peak = std::max (peak, std::fabs (samples[i]));
    return peak;
}

float rmsOf (const std::vector<float>& samples, int start = 0) noexcept
{
    auto sum = 0.0;
    auto count = 0;
    for (std::size_t i = static_cast<std::size_t> (std::max (0, start)); i < samples.size(); ++i)
    {
        sum += static_cast<double> (samples[i]) * static_cast<double> (samples[i]);
        ++count;
    }
    return count > 0 ? static_cast<float> (std::sqrt (sum / static_cast<double> (count))) : 0.0f;
}

Rendered render (const std::vector<float>& inputLeft,
                 const std::vector<float>& inputRight,
                 const RazorLeakParameters& params,
                 int tail = 4096)
{
    assert (inputLeft.size() == inputRight.size());

    RazorLeakEngine engine;
    engine.prepare (sampleRate);
    engine.setParameters (params);

    Rendered output;
    output.left.reserve (inputLeft.size() + static_cast<std::size_t> (tail));
    output.right.reserve (inputRight.size() + static_cast<std::size_t> (tail));

    for (std::size_t i = 0; i < inputLeft.size(); ++i)
    {
        const auto frame = engine.processSample (inputLeft[i], inputRight[i]);
        output.left.push_back (frame.left);
        output.right.push_back (frame.right);
    }

    for (int i = 0; i < tail; ++i)
    {
        const auto frame = engine.processSample (0.0f, 0.0f);
        output.left.push_back (frame.left);
        output.right.push_back (frame.right);
    }

    return output;
}

void testImpulseDelayDistributionShowsTimeShear()
{
    std::vector<float> left (256, 0.0f);
    std::vector<float> right (256, 0.0f);
    left[0] = 1.0f;
    right[0] = -1.0f;

    RazorLeakParameters params;
    params.slice = 0.90f;
    params.leak = 0.70f;
    params.time = 0.40f;
    params.bias = 0.65f;
    params.mix = 1.0f;

    RazorLeakEngine probe;
    probe.prepare (sampleRate);
    probe.setParameters (params);
    const auto baseDelay = probe.getBaseDelaySamples();

    const auto output = render (left, right, params);
    auto hits = 0;
    for (int i = std::max (0, baseDelay - 90); i < baseDelay + 260; ++i)
    {
        if (std::fabs (output.left[static_cast<std::size_t> (i)]) > 0.015f)
            ++hits;
    }

    assert (hits >= 3);
    assert (peakOf (output.left, baseDelay - 90) > 0.04f);
}

void testLeakRespondsToEdges()
{
    std::vector<float> left (4096, 0.0f);
    std::vector<float> right (4096, 0.0f);
    for (std::size_t i = 128; i < left.size(); i += 256)
    {
        left[i] = 0.8f;
        right[i] = -0.8f;
    }

    RazorLeakParameters dryLeak;
    dryLeak.leak = 0.0f;
    dryLeak.slice = 0.80f;
    dryLeak.time = 0.30f;
    dryLeak.mix = 1.0f;

    auto wetLeak = dryLeak;
    wetLeak.leak = 1.0f;

    const auto quiet = render (left, right, dryLeak);
    const auto leaking = render (left, right, wetLeak);
    assert (rmsOf (leaking.left, 256) > rmsOf (quiet.left, 256) * 1.25f);
}

void testFeedbackIsStrictlyBoundedBeforeSafetyClip()
{
    RazorLeakEngine engine;
    engine.prepare (sampleRate);

    RazorLeakParameters params;
    params.leak = std::numeric_limits<float>::infinity();
    params.slice = std::numeric_limits<float>::infinity();
    params.time = 1.0f;
    params.bias = -2.0f;
    params.output = 12.0f;
    engine.setParameters (params);

    assert (engine.getFeedbackCoefficient() < 1.0f);

    auto peak = 0.0f;
    for (int i = 0; i < 32768; ++i)
    {
        const auto input = (i % 97) == 0 ? 8.0f : ((i % 11) == 0 ? -3.0f : 0.0f);
        const auto frame = engine.processSample (input, -input);
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        peak = std::max ({ peak, std::fabs (frame.left), std::fabs (frame.right) });
    }

    assert (peak <= 0.98001f);
}

void testHostedSilencePreservation()
{
    std::vector<float> left (4096, 0.0f);
    std::vector<float> right (4096, 0.0f);

    RazorLeakParameters params;
    params.slice = 1.0f;
    params.leak = 1.0f;
    params.mix = 1.0f;
    params.output = 6.0f;

    const auto output = render (left, right, params);
    assert (peakOf (output.left) <= 1.0e-7f);
    assert (peakOf (output.right) <= 1.0e-7f);
}

void testDeterministicWithinTolerance()
{
    std::vector<float> left (2048, 0.0f);
    std::vector<float> right (2048, 0.0f);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        left[i] = std::sin (static_cast<float> (i) * 0.091f) * 0.31f;
        right[i] = std::cos (static_cast<float> (i) * 0.037f) * 0.27f;
    }

    RazorLeakParameters params;
    params.slice = 0.73f;
    params.leak = 0.59f;
    params.time = 0.44f;
    params.bias = -0.42f;
    params.mix = 0.81f;

    const auto a = render (left, right, params);
    const auto b = render (left, right, params);
    assert (a.left.size() == b.left.size());
    for (std::size_t i = 0; i < a.left.size(); ++i)
    {
        assert (std::fabs (a.left[i] - b.left[i]) <= 1.0e-6f);
        assert (std::fabs (a.right[i] - b.right[i]) <= 1.0e-6f);
    }
}

void testStateIdentityConstantsAreUnique()
{
    static_assert (razorleak::razorLeakStateMagic[0] == 'R');
    static_assert (razorleak::razorLeakStateMagic[1] == 'z');
    static_assert (razorleak::razorLeakStateMagic[2] == 'L');
    static_assert (razorleak::razorLeakStateMagic[3] == '1');
    static_assert (razorleak::razorLeakStateVersion == 1);
}

void testMetersAndStandaloneAuditionBridge()
{
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE) && defined(RAZORLEAK_HEADLESS_BRIDGE_TEST)
    RazorLeakAuditionBridge bridge;
    bridge.prepare (sampleRate);
    bridge.setEnabled (true);
    bridge.setType (RazorLeakAuditionBridge::Type::Scanline);

    RazorLeakEngine engine;
    engine.prepare (sampleRate);

    RazorLeakParameters params;
    params.slice = 0.84f;
    params.leak = 0.77f;
    params.mix = 1.0f;
    engine.setParameters (params);

    std::vector<float> audition;
    audition.reserve (4096);
    for (int i = 0; i < 4096; ++i)
    {
        const auto input = bridge.nextInput();
        const auto frame = engine.processSample (input.left, input.right);
        audition.push_back (frame.left);
    }

    const auto meters = engine.getAndClearMeters();
    assert (rmsOf (audition) >= 1.0e-4f);
    assert (meters.inputPeakLeft > 0.0f);
    assert (meters.outputPeakLeft > 0.0f);
    const auto cleared = engine.getAndClearMeters();
    assert (cleared.inputPeakLeft == 0.0f);
#else
    assert (false && "standalone audition bridge test requires explicit standalone macro");
#endif
}

} // namespace

int main()
{
    testImpulseDelayDistributionShowsTimeShear();
    testLeakRespondsToEdges();
    testFeedbackIsStrictlyBoundedBeforeSafetyClip();
    testHostedSilencePreservation();
    testDeterministicWithinTolerance();
    testStateIdentityConstantsAreUnique();
    testMetersAndStandaloneAuditionBridge();

    std::cout << "RazorLeakEngineTests passed\n";
    return 0;
}
