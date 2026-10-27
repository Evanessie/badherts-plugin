# Rules injected into every DSP task in this project

## CAN
- Use `juce::Random` (a small deterministic LCG) for every random process.
- Cache filter coefficients per block; recompute only when a parameter changes.
- Preallocate all buffers in `prepareToPlay` / `prepare` and reuse them forever.
- Reinterpret the same knobs differently per mode through one mapping function.
- Keep the whole chain within three stages: pre-filter -> engine -> mix/limiter.

## SHOULD
- Keep the per-sample work branch-light: hoist mode checks and rates out of the loop.
- Report a constant latency (1023 samples) no matter which FFT size is active.
- Smooth every user-visible gain with `SmoothedValue` (20 ms) to avoid zipper noise.
- Round-trip state through `AudioProcessorValueTreeState` only.

## MUST NOT
- MUST NOT allocate, free, lock, log, open files or build strings inside `processBlock`.
- MUST NOT call `rand()`, `std::random_device`, `new`, `std::vector::push_back` or `juce::String` in the audio thread.
- MUST NOT implement a real encoder/decoder: no entropy coding, no psychoacoustic model, no bit reservoir, no Huffman, no Huffman tables.
- MUST NOT give each mode its own separate DSP chain; modes share one parameter mapping layer.
- MUST NOT recompute `sin`/`exp`/`pow` per sample for filter coefficients.

## SHOULD NOT
- SHOULD NOT add reverb / delay style spatial effects to the core chain.
- SHOULD NOT let any single artifact push peaks above +2 dBFS; the safety knee catches the rest.
- SHOULD NOT change the UI palette without updating the palette tokens in `PixelWidgets.h`.
