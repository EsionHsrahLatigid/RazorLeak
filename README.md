# RazorLeak

RazorLeak v0.1.0 is an independent YUP stereo effect by 2bit. It is a short bounded circular delay with time-sheared read offsets and an edge/highpass detector feeding a clipped feedback leak. The sound target is glitch smear: razor cuts and leaking scanlines, not reverb.

## Identity

- App ID: `audio.2bit.razorleak`
- Plugin ID: `audio.2bit.RazorLeak`
- AU subtype: `RzLk`
- State magic/version: `RzL1` / `1`
- Host parameters: `Slice`, `Leak`, `Time`, `Bias`, `Mix`, `Output`

## Parameters

- `Slice`: increases shear width and clip hardness.
- `Leak`: raises edge-triggered feedback amount. Internal feedback is clamped strictly below 1 before the safety clip.
- `Time`: moves the short delay window from tight comb cuts to wider scanline smears.
- `Bias`: offsets left/right read positions in opposite directions.
- `Mix`: dry/wet blend.
- `Output`: final gain before the peak safety limiter.

## Standalone Audition

Standalone builds expose runtime-only audition controls and input/output meters. Audition enable/type are guarded by `YUP_AUDIO_PLUGIN_ENABLE_STANDALONE`, live in atomics, and are neither automated nor serialized. Hosted VST3/AU paths compile without the generator branch and remain strictly input to effect to output, preserving silence.

## Build And Test

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug --parallel
ctest --preset engine-debug --output-on-failure
```

Plugin bundles use the `plugin-release` preset, but the local verification path for v0.1.0 is engine-debug only.

```sh
cmake --preset plugin-release
cmake --build --preset plugin-release --parallel
ctest --preset plugin-release --output-on-failure
```

## CI And Releases

- Pull requests and `main` pushes run a classifier first. Documentation-only changes skip the paid macOS and Windows build jobs while the stable `CI Summary` check still reports the required result.
- Heavy CI builds and tests macOS arm64 and Windows x64 bundles once, packages checksum-protected ZIP files, and retains the artifacts for 14 days.
- A version tag never rebuilds the plugin. The release workflow resolves the tag to its commit, verifies that the tag and CMake versions match, and promotes only the two unexpired artifacts from the unique successful `main` CI run for that exact commit SHA.
- ZIP checksums and archive structure are verified before a draft release is published. Existing published releases fail closed instead of being overwritten.
