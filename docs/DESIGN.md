# BADHERTS Losser999 — lossy engine design

## Signal flow

```
 in ─┐
     │
     ▼
 ┌───────────────────────────────────────────────────────────────┐
 │ 1. PRE FILTER      HP @ preLow (20 Hz – 2 kHz)                 │
 │                    LP @ preHigh (2 kHz – 20 kHz)               │
 │                    (device character narrows the same band)    │
 └───────────────────────────────────────────────────────────────┘
     │
     ▼
 ┌───────────────────────────────────────────────────────────────┐
 │ 2. PARAMETER MAPPING LAYER  (one function, no branching below) │
 │    lmode + amount + speed + texture + pmix → EngineSettings    │
 └───────────────────────────────────────────────────────────────┘
     │
     ▼
 ┌───────────────────────────────────────────────────────────────┐
 │ 3. LOSSY ENGINE   (shared knobs, different meanings)           │
 │    a) standard loss  : band split 900 Hz, random band gain     │
 │                        wobble + 10 ms pitch drift delay        │
 │    b) phase jitter   : 1–3.5 sample fractional delay, fast LFO │
 │    c) packet gate    : silence  ←→  loop of the last N samples │
 │    d) inverse        : preFiltered − standard loss (residual)  │
 └───────────────────────────────────────────────────────────────┘
     │
     ▼
 ┌───────────────────────────────────────────────────────────────┐
 │ 4. WEIRD BUS (optional, 8 toggles, shared weird amount)        │
 │    fold / cheb / comb / smear / whisper / freeze / mirror/blur │
 └───────────────────────────────────────────────────────────────┘
     │
     ▼
 ┌───────────────────────────────────────────────────────────────┐
 │ 5. DRY/WET + SAFETY                                           │
 │    out = (dry_delayed + (wet − dry) · gate · mix) · gain      │
 │    soft knee above 0.9 to guarantee no clipping                │
 └───────────────────────────────────────────────────────────────┘
     │
     ▼
    out
```

The dry path is delayed by exactly 1023 samples so the three stages can be
recombined without a phase offset.

## Parameter → mode mapping

| Knob | STANDARD | JITTER | P.LOSS | P.REPEAT | INVERSE | STD+P |
|---|---|---|---|---|---|---|
| **Amount** | band-wobble depth | jitter depth (0–2.5 samples) | how often packets drop | how often packets drop | residual mix | both at 0.8× |
| **Speed** | wobble rate 0.2–7 Hz | jitter LFO 2–62 Hz | gap + burst length 15–295 ms | same | 0.2–7 Hz | 10–190 ms |
| **Texture** | extra high-band grit | unused | 0.3× grit | 0.3× grit | high-band grit | high-band grit |
| **Filter type** | always: post filter on the wet path (OFF / LP / HP / BP / NOTCH, 12 or 24 dB) | same | same | same | same | same |
| **Pre Low / High** | always: input band-limit before degradation | same | same | same | same | same |

Smoothing: every user gain goes through `SmoothedValue` at 20 ms; band gains
use a fixed 0.004 one-pole per sample; the pitch drift uses 0.002.

## Why this is lightweight and realtime safe

- **No codec.** Everything is a parametric artifact: two band gains, one
  modulated delay, one fractional delay, one scheduler. No bitstream at all.
- **No allocation in `processBlock`.** Every buffer (`history`, `loopBuffer`,
  delay lines, FFT scratch) is sized in `prepareToPlay`.
- **No system randomness.** `juce::Random` is a deterministic LCG instance owned
  by the processor; it is never reseeded from the clock.
- **Coefficients cached.** The pre-filter and band split are only re-set when a
  parameter or the device character changes; the per-sample loop contains no
  `sin`, `exp` or `pow` (the jitter LFO is the one `sin`, at one call per sample,
  and it uses the same cached increment).
- **Three stages.** Pre-filter → engine → mix/limiter. The weird bus is a
  parallel extra, off by default.
- **Constant latency.** 1023 samples regardless of FFT size; smaller FFTs are
  padded so host delay compensation never jumps.

## Why STANDARD sounds like a broken MP3 at 0–25 % and breaks down after

At low amounts the two band gains wobble by only a few dB at 1–2 Hz and the
drift delay moves under 2 ms. That is exactly the signature of a low-bitrate
codec: a slowly breathing high band, missing stereo detail, a faint watery
flutter. Nothing is distorted, so the ear files it under "bad encoder".

Past 25 % the wobble becomes faster than the codec's own time constants, the
drift delay exceeds a millisecond per update, and the artifacts stop being
plausible encoding errors — they become a musical effect that could never have
come out of an encoder. The parameter is continuous, so the transition is a
slope, not a switch.
