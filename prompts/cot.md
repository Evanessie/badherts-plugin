# Chain-of-thought template for DSP changes

1. **Restate the audible goal** in one sentence ("make the high band wobble like a 32 kbps MP3").
2. **Name the knob** that will drive it, and the unit it is expressed in (Hz, seconds, %).
3. **Map it** through `mapParameters()` so every mode keeps one shared control surface.
4. **Pick the cheapest primitive** that produces the artifact: gain envelope > delay
   modulation > filtering > FFT. Only reach for the STFT when the artifact is spectral.
5. **Cache everything** that does not change per sample: rates, periods, depths, coefficients.
6. **Check the realtime rules** (`prompts/system_rules.md`): no alloc, no lock, no `rand()`.
7. **Bound it**: unit gain at 0, no NaN/Inf, peak under control, silence stays silent.
8. **Verify offline** with `tools/audiotest_bitrot.cpp` before touching the UI.
