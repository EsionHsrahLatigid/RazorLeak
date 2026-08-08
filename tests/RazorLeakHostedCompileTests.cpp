#include "razorleak/RazorLeakEngine.h"

#include <cassert>
#include <iostream>

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
#error "Hosted compile test must not define YUP_AUDIO_PLUGIN_ENABLE_STANDALONE"
#endif

int main()
{
    razorleak::RazorLeakEngine engine;
    engine.prepare (48000.0);

    razorleak::RazorLeakParameters parameters;
    parameters.leak = 1.0f;
    parameters.mix = 1.0f;
    engine.setParameters (parameters);

    const auto frame = engine.processSample (0.0f, 0.0f);
    assert (frame.left == 0.0f);
    assert (frame.right == 0.0f);

    std::cout << "RazorLeakHostedCompileTests passed\n";
    return 0;
}
