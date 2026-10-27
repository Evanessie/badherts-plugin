#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace tag
{
constexpr float kPi = 3.14159265358979323846f;

/** Cheap tanh for soft clipping (Pade, clamped). */
inline float fastTanh (float x) noexcept
{
    x = std::clamp (x, -3.0f, 3.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

/** One-pole lowpass; also usable as a cheap DC blocker when a is small. */
struct OnePole
{
    float a = 1.0f, z = 0.0f;

    void setCutoff (float hz, double sr) noexcept
    {
        const float f = std::clamp (hz, 5.0f, (float) sr * 0.49f);
        a = 1.0f - std::exp (-2.0f * kPi * f / (float) sr);
    }

    inline float lp (float x) noexcept
    {
        z += a * (x - z);
        return z;
    }

    void reset() noexcept { z = 0.0f; }
};

/** 12 dB lowpass, Zavalishin TPT state variable filter. */
struct TptSvf
{
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;

    void set (float cutoffHz, float q, double sr) noexcept
    {
        const float f = std::clamp (cutoffHz, 20.0f, (float) sr * 0.45f);
        g = std::tan (kPi * f / (float) sr);
        k = 1.0f / std::max (0.2f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline float lp (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }

    /** Unity-gain bandpass. */
    inline float bp (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return k * v1;
    }

    /** Notch (low + high). */
    inline float notch (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return x - k * v1;
    }

    /** Same filter state, highpass output (zavalishin: input - k*band - low). */
    inline float hp (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return x - k * v1 - v2;
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }
};

/** Keeps pre low below pre high (minimum 1.2x apart) while letting both move freely. */
inline void clampPreFilter (float& lo, float& hi) noexcept
{
    constexpr float kMin = 20.0f, kMax = 20000.0f, kRatio = 1.2f;

    lo = juce::jlimit (kMin, kMax, lo);
    hi = juce::jlimit (kMin, kMax, hi);

    if (hi < lo * kRatio)
    {
        hi = lo * kRatio;

        if (hi > kMax)
        {
            hi = kMax;
            lo = juce::jmin (lo, hi / kRatio);
        }
    }
}

/** 2nd order lowpass magnitude, used for drawing the pre filter curve. */
inline float lowpassMagnitude (float f, float fc, float q) noexcept
{
    const float r = f / juce::jmax (1.0f, fc);
    const float r2 = r * r;
    return 1.0f / std::sqrt ((1.0f - r2) * (1.0f - r2) + (r / q) * (r / q));
}

/** Response of the pre filter band (highpass at lo, lowpass at hi) in dB. */
inline float preFilterMagnitudeDb (float hz, float loHz, float hiHz) noexcept
{
    const float r = hz / juce::jmax (1.0f, loHz);
    const float hp = lowpassMagnitude (loHz, hz, 0.707f) * r * r;
    const float lp = lowpassMagnitude (hz, hiHz, 0.707f);
    return juce::Decibels::gainToDecibels (juce::jmax (1.0e-5f, hp * lp));
}

/** Triangle wavefolder - the classic digital fold, very metallic. */
inline float triangleFold (float x) noexcept
{
    x = std::fmod (x + 1.0f, 4.0f);

    if (x < 0.0f)
        x += 4.0f;

    return std::abs (x - 2.0f) - 1.0f;
}

/** Chebyshev shaper: order n adds exactly the n-th harmonic. */
inline float chebyshev (int order, float x) noexcept
{
    x = std::clamp (x, -1.0f, 1.0f);

    if (order <= 0)
        return 1.0f;

    float t0 = 1.0f, t1 = x;

    for (int n = 2; n <= order; ++n)
    {
        const float t2 = 2.0f * x * t1 - t0;
        t0 = t1;
        t1 = t2;
    }

    return t1;
}

/** 4th-order Linkwitz-Riley section: two cascaded Butterworth stages. */
struct Lr4Split
{
    TptSvf a, b;

    void set (float hz, double sr) noexcept
    {
        a.set (hz, 0.7071f, sr);
        b.set (hz, 0.7071f, sr);
    }

    inline float lp (float x) noexcept { return b.lp (a.lp (x)); }
    inline float hp (float x) noexcept { return b.hp (a.hp (x)); }

    void reset() noexcept { a.reset(); b.reset(); }
};

/** Fractional delay line with linear interpolation. */struct DelayLine
{
    std::vector<float> buf;
    int writePos = 0;

    void resize (int n)
    {
        buf.assign ((std::size_t) std::max (n, 4), 0.0f);
        writePos = 0;
    }

    inline void write (float x) noexcept
    {
        buf[(std::size_t) writePos] = x;
        if (++writePos >= (int) buf.size())
            writePos = 0;
    }

    inline float read (float delaySamples) const noexcept
    {
        const int size = (int) buf.size();
        float r = (float) writePos - delaySamples;
        while (r < 0.0f)
            r += (float) size;

        const int i0 = (int) r;
        const float frac = r - (float) i0;
        const int i1 = (i0 + 1) % size;
        return buf[(std::size_t) i0] + frac * (buf[(std::size_t) i1] - buf[(std::size_t) i0]);
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        writePos = 0;
    }
};

/** ~same shape as a bipolar signal minus its DC (used after the bias stage). */
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f;

    inline float process (float x, float r) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    void reset() noexcept { x1 = y1 = 0.0f; }
};


//==============================================================================
/** In-place radix-2 FFT. Size must be a power of two. */
inline void fftRadix2 (std::vector<float>& re, std::vector<float>& im)
{
    const int n = (int) re.size();

    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;

        for (; j & bit; bit >>= 1)
            j ^= bit;

        j ^= bit;

        if (i < j)
        {
            std::swap (re[(std::size_t) i], re[(std::size_t) j]);
            std::swap (im[(std::size_t) i], im[(std::size_t) j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const float ang = -2.0f * kPi / (float) len;
        const float wr = std::cos (ang), wi = std::sin (ang);

        for (int i = 0; i < n; i += len)
        {
            float cr = 1.0f, ci = 0.0f;

            for (int k = 0; k < len / 2; ++k)
            {
                const int a = i + k, b = a + len / 2;
                const float ur = re[(std::size_t) a], ui = im[(std::size_t) a];
                const float vr = re[(std::size_t) b] * cr - im[(std::size_t) b] * ci;
                const float vi = re[(std::size_t) b] * ci + im[(std::size_t) b] * cr;
                re[(std::size_t) a] = ur + vr; im[(std::size_t) a] = ui + vi;
                re[(std::size_t) b] = ur - vr; im[(std::size_t) b] = ui - vi;

                const float ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
}

/** STFT spectral gate (Hann analysis/synthesis, 75% overlap).
    Latency is exactly N-1 samples, the caller has to delay the dry path to match. */

//==============================================================================
/** One of the classic filter shapes, 12 or 24 dB. */
struct Filter
{
    enum Type { off = 0, lp12, lp24, hp12, hp24, bp, notch, numTypes };

    void set (float cutoffHz, float q, double sr) noexcept
    {
        a.set (cutoffHz, q, sr);
        b.set (cutoffHz, q, sr);
    }

    inline float process (int type, float x) noexcept
    {
        switch (type)
        {
            case lp12:  return a.lp (x);
            case lp24:  return b.lp (a.lp (x));
            case hp12:  return a.hp (x);
            case hp24:  return b.hp (a.hp (x));
            case bp:    return a.bp (x);
            case notch: return a.notch (x);
            default:    return x;
        }
    }

    void reset() noexcept { a.reset(); b.reset(); }

    /** Analytic magnitude response, used to draw the curve on the spectrum. */
    static float magnitudeDb (int type, float f, float fc, float q)
    {
        if (type == off || fc <= 1.0f || f <= 0.0f)
            return 0.0f;

        const float r = f / fc;
        const float r2 = r * r;
        const float denom = std::sqrt ((1.0f - r2) * (1.0f - r2) + (r / q) * (r / q));
        float mag = 1.0f;

        switch (type)
        {
            case lp12:  mag = 1.0f / denom; break;
            case lp24:  mag = 1.0f / (denom * denom); break;
            case hp12:  mag = r2 / denom; break;
            case hp24:  mag = r2 * r2 / (denom * denom); break;
            case bp:    mag = (r / q) / denom; break;
            case notch: mag = std::abs (1.0f - r2) / denom; break;
            default:    break;
        }

        return 20.0f * std::log10 (std::max (1.0e-6f, mag));
    }

    TptSvf a, b;
};

} // namespace tag

#include "SpectralGate.h"
