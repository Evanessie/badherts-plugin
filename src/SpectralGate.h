#pragma once

#include "DSP.h"

namespace tag
{
/** STFT processor for the degradation signal: gate, spectral smear, temporal blur,
    mirror, freeze and phase randomisation (whisper). Latency is exactly N-1 samples. */
struct SpectralGate
{
    struct Params
    {
        float threshold = 0.0f;
        bool gate = false;
        bool invert = false;
        float smear = 0.0f;      // blur across frequency
        float blurTime = 0.0f;   // blur across frames
        float mirror = 0.0f;     // 0..1 spectral mirror
        float freeze = 0.0f;     // 0..1 hold the spectrum
        float whisper = 0.0f;    // 0..1 phase randomisation
    };

    void prepare (int fftSize)
    {
        size = juce::jlimit (128, 4096, fftSize);
        hop = juce::jmax (1, size / 4);
        half = size / 2;

        window.resize ((std::size_t) size);

        for (int i = 0; i < size; ++i)
            window[(std::size_t) i] = 0.5f - 0.5f * std::cos (2.0f * kPi * (float) i / (float) size);

        inRing.assign ((std::size_t) size, 0.0f);
        ola.assign ((std::size_t) size, 0.0f);
        re.assign ((std::size_t) size, 0.0f);
        im.assign ((std::size_t) size, 0.0f);
        binGain.assign ((std::size_t) half, 0.0f);
        magNow.assign ((std::size_t) half, 0.0f);
        magTarget.assign ((std::size_t) half, 0.0f);
        temporal.assign ((std::size_t) half, 0.0f);
        frozen.assign ((std::size_t) half, 0.0f);
        phaseOff.assign ((std::size_t) half, 0.0f);

        for (auto& v : phaseOff)
            v = rng.nextFloat() * 2.0f * kPi - kPi;

        std::vector<float> acc ((std::size_t) size, 0.0f);

        for (int k = 0; k < juce::jmax (1, size / hop); ++k)
            for (int i = 0; i < size; ++i)
            {
                const float w = window[(std::size_t) ((i + k * hop) % size)];
                acc[(std::size_t) i] += w * w;
            }

        float mean = 0.0f;

        for (float v : acc)
            mean += v;

        mean /= (float) size;
        overlapGain = mean > 1.0e-6f ? 1.0f / mean : 1.0f;

        reset();
    }

    int latency() const noexcept { return size - 1; }

    void reset()
    {
        std::fill (inRing.begin(), inRing.end(), 0.0f);
        std::fill (ola.begin(), ola.end(), 0.0f);
        std::fill (binGain.begin(), binGain.end(), 0.0f);
        std::fill (temporal.begin(), temporal.end(), 0.0f);
        frozenValid = false;
        inPos = 0;
        hopCount = 0;
    }

    inline float process (float x, const Params& gp) noexcept
    {
        inRing[(std::size_t) inPos] = x;

        if (++hopCount >= hop)
        {
            hopCount = 0;
            processFrame (gp);
        }

        const int outPos = (inPos + 1) % size;
        const std::size_t idx = (std::size_t) outPos;
        const float y = ola[idx];
        ola[idx] = 0.0f;

        if (++inPos >= size)
            inPos = 0;

        return y;
    }

private:
    static float sineOf (float phase) noexcept
    {
        static const std::vector<float> table = []
        {
            std::vector<float> t (4096);

            for (int i = 0; i < 4096; ++i)
                t[(std::size_t) i] = std::sin (2.0f * kPi * (float) i / 4096.0f);

            return t;
        }();

        float p = phase * 650.8783f;
        p -= std::floor (p / 4096.0f) * 4096.0f;
        return table[(std::size_t) (((int) p) & 4095)];
    }

    static float cosineOf (float phase) noexcept { return sineOf (phase + 1.5707963f); }

    void processFrame (const Params& gp) noexcept
    {
        const int start = (inPos + 1) % size;

        for (int j = 0; j < size; ++j)
        {
            re[(std::size_t) j] = inRing[(std::size_t) ((start + j) % size)] * window[(std::size_t) j];
            im[(std::size_t) j] = 0.0f;
        }

        fftRadix2 (re, im);

        const float scale = 2.0f / (float) size;

        for (int k = 1; k < half; ++k)
            magNow[(std::size_t) k] = std::sqrt (re[(std::size_t) k] * re[(std::size_t) k]
                                               + im[(std::size_t) k] * im[(std::size_t) k]) * scale;

        magNow[0] = magNow[1];

        // ---- mirror --------------------------------------------------------
        if (gp.mirror > 0.001f)
        {
            for (int k = 1; k < half; ++k)
                magTarget[(std::size_t) k] = magNow[(std::size_t) k]
                                           + gp.mirror * (magNow[(std::size_t) (half - k)]
                                                          - magNow[(std::size_t) k]);
        }
        else
        {
            for (int k = 1; k < half; ++k)
                magTarget[(std::size_t) k] = magNow[(std::size_t) k];
        }

        // ---- freeze --------------------------------------------------------
        if (gp.freeze > 0.001f)
        {
            if (! frozenValid)
            {
                frozen = magTarget;
                frozenValid = true;
            }
            else
            {
                for (int k = 1; k < half; ++k)
                    magTarget[(std::size_t) k] = magTarget[(std::size_t) k]
                                               + gp.freeze * (frozen[(std::size_t) k]
                                                              - magTarget[(std::size_t) k]);
            }
        }
        else
        {
            frozenValid = false;
        }

        // ---- blur across time ------------------------------------------
        if (gp.blurTime > 0.001f)
        {
            const float a = juce::jlimit (0.0f, 0.97f, gp.blurTime * 0.95f);

            for (int k = 1; k < half; ++k)
            {
                temporal[(std::size_t) k] = temporal[(std::size_t) k] * a
                                          + magTarget[(std::size_t) k] * (1.0f - a);
                magTarget[(std::size_t) k] = temporal[(std::size_t) k];
            }
        }

        // ---- blur across frequency -------------------------------------
        if (gp.smear > 0.001f)
        {
            for (int k = 1; k < half; ++k)
            {
                const float prev = magTarget[(std::size_t) std::max (1, k - 1)];
                const float next = magTarget[(std::size_t) std::min (half - 1, k + 1)];
                magTarget[(std::size_t) k] += gp.smear * 0.5f * (prev + next - 2.0f * magTarget[(std::size_t) k]);
            }
        }

        // ---- gate, whisper, write back ----------------------------------
        for (int k = 1; k < half; ++k)
        {
            const float mag = magNow[(std::size_t) k];

            float g = 1.0f;

            if (gp.gate)
            {
                const float t = magTarget[(std::size_t) k] / std::max (gp.threshold, 1.0e-6f);

                if (! gp.invert)
                    g = t >= 1.0f ? 1.0f : t * t;
                else
                    g = t <= 1.0f ? 1.0f : (1.0f / (t * t));
            }

            float& smoothed = binGain[(std::size_t) k];
            smoothed = std::max (g, smoothed * 0.82f);

            const float ratio = magTarget[(std::size_t) k] / std::max (mag, 1.0e-9f);
            const float total = ratio * smoothed;

            float outRe = re[(std::size_t) k] * total;
            float outIm = im[(std::size_t) k] * total;

            if (gp.whisper > 0.001f)
            {
                float& off = phaseOff[(std::size_t) k];
                off += (rng.nextFloat() - 0.5f) * 0.6f;

                if (off > kPi) off -= 2.0f * kPi;
                if (off < -kPi) off += 2.0f * kPi;

                const float ang = gp.whisper * off;
                const float c = cosineOf (ang);
                const float sn = sineOf (ang);
                const float nr = outRe * c - outIm * sn;
                const float ni = outRe * sn + outIm * c;
                outRe = nr;
                outIm = ni;
            }

            re[(std::size_t) k] = outRe;
            im[(std::size_t) k] = outIm;
            re[(std::size_t) (size - k)] = outRe;
            im[(std::size_t) (size - k)] = -outIm;
        }

        for (int j = 0; j < size; ++j)
            im[(std::size_t) j] = -im[(std::size_t) j];

        fftRadix2 (re, im);

        const float inv = 1.0f / (float) size;

        for (int j = 0; j < size; ++j)
        {
            const float sample = re[(std::size_t) j] * inv * window[(std::size_t) j] * overlapGain;
            ola[(std::size_t) ((start + j) % size)] += sample;
        }
    }

    std::vector<float> window, inRing, ola, re, im, binGain;
    std::vector<float> magNow, magTarget, temporal, frozen, phaseOff;
    juce::Random rng { 0x51ee7 };
    int size = 1024, hop = 256, half = 512, inPos = 0, hopCount = 0;
    bool frozenValid = false;
    float overlapGain = 1.0f;
};
} // namespace tag
