# RazorLeak Design Checklist

## Product

- [x] Independent YUP stereo effect named RazorLeak v0.1.0.
- [x] DSP identity is a short bounded circular delay with time-shear read offsets.
- [x] Edge/highpass detector feeds a clipped feedback leak.
- [x] Result is glitch smear, not reverb.
- [x] IDs are unique: app `audio.2bit.razorleak`, plugin `audio.2bit.RazorLeak`, AU subtype `RzLk`.
- [x] State magic/version are unique: `RzL1` / `1`.

## Parameters

- [x] Host automation exposes exactly `Slice`, `Leak`, `Time`, `Bias`, `Mix`, and `Output`.
- [x] Standalone audition enable/type are runtime-only controls, not host parameters.
- [x] Standalone audition state is not serialized.

## DSP Safety

- [x] Hosted processing is input to effect to output.
- [x] Silence preservation target: hosted silence peak <= `1e-7`.
- [x] Peak safety target: output peak <= `0.98`.
- [x] Feedback coefficient is strictly less than 1 before safety clipping.
- [x] Non-finite inputs and parameters are clamped.
- [x] Denormal values are flushed on feedback/filter state.
- [x] Realtime callback avoids allocation, locks, I/O, logging, and UI access.

## Standalone

- [x] Standalone wrapper is detected through `YUP_AUDIO_PLUGIN_ENABLE_STANDALONE`.
- [x] Fail-closed behavior: if the macro is absent, audition members and UI controls are not compiled.
- [x] Input and output meters are published from lock-free atomics.
- [x] Audition bridge proves RMS >= `1e-4` under headless standalone macro test.

## Visual

- [x] High contrast native YUP grid.
- [x] Razor-cut vertical lines.
- [x] Leaking scanline bands.
- [x] No external visual assets.
- [x] No new dependencies.

## Verification

- [x] Tests cover impulse delay distribution and time shear.
- [x] Tests cover Leak response to edge-heavy material.
- [x] Tests cover feedback bounds and peak safety.
- [x] Tests cover silence preservation.
- [x] Tests cover deterministic output within `1e-6`.
- [x] Tests cover state identity constants.
- [x] Tests cover meters and standalone audition bridge.

## CI And Release Constraints

- [x] `main`, pull requests, and manual dispatch are the only CI build entry points; tags do not rebuild bundles.
- [x] Documentation-only changes keep a stable required summary while skipping paid macOS and Windows build jobs.
- [x] macOS arm64 and Windows x64 artifacts include strict SHA-256 manifests and use 14-day retention.
- [x] Release tags are resolved to commit SHAs and must match the CMake project version.
- [x] A release promotes exactly two unexpired artifacts from the unique successful `main` CI run whose `head_sha` matches the tag commit.
- [x] Downloaded archives are checksum- and ZIP-verified before draft assets are replaced and published.
- [x] Existing published releases fail closed instead of being overwritten.
- [x] Workflow actions are pinned to immutable commit SHAs.
