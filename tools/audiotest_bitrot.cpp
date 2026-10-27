// Offline checks for the lossy engine. Builds against the plugin's shared code.
#include <JuceHeader.h>

#include <cmath>
#include <cstdio>
#include <vector>

#include "BitrotProcessor.h"

namespace
{
constexpr double kSr = 48000.0;
constexpr int kBlockSize = 256;
constexpr int kSeconds = 3;

int failures = 0;

double db (double v) { return 20.0 * std::log10 (std::max (1.0e-12, v)); }

struct Settings
{
    int lmode = 0;
    float amount = 0.0f, speed = 40.0f, texture = 30.0f, pmix = 60.0f;
    float preLow = 20.0f, preHigh = 20000.0f;
    int ftype = 0;      // 0 off, 1 lp12, 2 lp24, ...
    float cutoff = 20000.0f, fres = 0.0f;
    float weird = 0.0f;
    bool fold = false, cheb = false, comb = false, smear = false;
    bool whisper = false, freeze = false, mirror = false, blurt = false;
    float mix = 100.0f;
};

void set (BitrotProcessor& p, const char* id, float normalised)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (normalised);
}

void apply (BitrotProcessor& p, const Settings& s)
{
    set (p, "lmode", (float) s.lmode / 5.0f);
    set (p, "amount", s.amount * 0.01f);
    set (p, "speed", s.speed * 0.01f);
    set (p, "texture", s.texture * 0.01f);
    set (p, "pmix", s.pmix * 0.01f);
    set (p, "preLow", (float) p.apvts.getParameter ("preLow")->convertTo0to1 (s.preLow));
    set (p, "preHigh", (float) p.apvts.getParameter ("preHigh")->convertTo0to1 (s.preHigh));
    set (p, "ftype", (float) s.ftype / 6.0f);
    set (p, "cutoff", (float) p.apvts.getParameter ("cutoff")->convertTo0to1 (s.cutoff));
    set (p, "fres", s.fres * 0.01f);
    set (p, "weird", s.weird * 0.01f);
    set (p, "fx_fold", s.fold ? 1.0f : 0.0f);
    set (p, "fx_cheb", s.cheb ? 1.0f : 0.0f);
    set (p, "fx_comb", s.comb ? 1.0f : 0.0f);
    set (p, "fx_smear", s.smear ? 1.0f : 0.0f);
    set (p, "fx_whisper", s.whisper ? 1.0f : 0.0f);
    set (p, "fx_freeze", s.freeze ? 1.0f : 0.0f);
    set (p, "fx_mirror", s.mirror ? 1.0f : 0.0f);
    set (p, "fx_blurt", s.blurt ? 1.0f : 0.0f);
    set (p, "mix", s.mix * 0.01f);
}

struct Result
{
    double rmsIn = 0.0, rmsOut = 0.0, peakOut = 0.0;
    double minBlockRms = 1.0e9;
    bool finite = true;
};

Result run (const Settings& s)
{
    BitrotProcessor proc;
    proc.setPlayConfigDetails (2, 2, kSr, kBlockSize);
    proc.prepareToPlay (kSr, kBlockSize);
    apply (proc, s);

    const int total = (int) (kSr * kSeconds);
    juce::AudioBuffer<float> buffer (2, kBlockSize);

    Result r;
    double sumOut = 0.0;
    int count = 0;

    juce::Random rng (7);

    for (int pos = 0; pos < total; pos += kBlockSize)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlockSize; ++i)
            {
                const double t = (pos + i) / kSr;
                const float sine = (float) (0.25 * std::sin (juce::MathConstants<double>::twoPi * 440.0 * t)
                                            + 0.15 * std::sin (juce::MathConstants<double>::twoPi * 3100.0 * t));
                buffer.setSample (ch, i, sine + (rng.nextFloat() - 0.5f) * 0.002f);
            }

        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);

        double blockOut = 0.0;

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlockSize; ++i)
            {
                const float y = buffer.getSample (ch, i);

                if (! std::isfinite (y))
                    r.finite = false;

                blockOut += (double) y * y;
                r.peakOut = std::max (r.peakOut, (double) std::abs (y));
            }

        sumOut += blockOut;
        count += kBlockSize * 2;

        if (pos > (int) (kSr * 0.4))
            r.minBlockRms = std::min (r.minBlockRms, std::sqrt (blockOut / (2.0 * kBlockSize)));
    }

    r.rmsIn = std::sqrt (0.25 * 0.25 * 0.5 + 0.15 * 0.15 * 0.5 + 0.002 * 0.002 / 12.0);
    r.rmsOut = std::sqrt (sumOut / count);
    return r;
}

} // namespace

int main()
{
    std::printf ("== BADHERTS Losser999 offline checks ==\n");

    // 1. no degradation at amount 0
    {
        Settings s;
        s.amount = 0.0f;
        const auto r = run (s);
        std::printf ("bypass      rms %+6.2f dB  peak %.3f\n", db (r.rmsOut / r.rmsIn), r.peakOut);

        if (std::abs (db (r.rmsOut / r.rmsIn)) > 2.0 || ! r.finite)
        {
            std::printf ("FAIL: amount 0 is not transparent\n");
            ++failures;
        }
    }

    // 2. standard mode: audible change, stable level
    for (float amount : { 25.0f, 60.0f, 100.0f })
    {
        Settings s;
        s.lmode = 0;
        s.amount = amount;
        s.speed = 45.0f;
        const auto r = run (s);
        std::printf ("std %3.0f%%    rms %+6.2f dB  peak %.3f\n", amount, db (r.rmsOut / r.rmsIn), r.peakOut);

        if (! r.finite || r.peakOut > 2.0)
        {
            std::printf ("FAIL: standard %.0f%% unstable\n", amount);
            ++failures;
        }
    }

    // 3. packet loss must actually mute chunks
    {
        Settings s;
        s.lmode = 2;
        s.amount = 80.0f;
        s.speed = 55.0f;
        const auto r = run (s);
        std::printf ("p.loss      rms %+6.2f dB  min block %+6.1f dBFS\n",
                     db (r.rmsOut / r.rmsIn), db (r.minBlockRms));

        if (r.minBlockRms > 0.02)
        {
            std::printf ("FAIL: packet loss never mutes\n");
            ++failures;
        }
    }

    // 4. packet repeat keeps the level up
    {
        Settings s;
        s.lmode = 3;
        s.amount = 80.0f;
        s.speed = 55.0f;
        const auto r = run (s);
        std::printf ("p.repeat    rms %+6.2f dB  min block %+6.1f dBFS\n",
                     db (r.rmsOut / r.rmsIn), db (r.minBlockRms));

        if (r.minBlockRms < 0.01)
        {
            std::printf ("FAIL: repeat mode goes silent\n");
            ++failures;
        }
    }

    // 5. jitter, inverse, combined
    {
        const int modes[] = { 1, 4, 5 };
        const char* names[] = { "std", "jit", "loss", "rep", "inv", "std+p" };

        for (int m : modes)
        {
            Settings s;
            s.lmode = m;
            s.amount = 70.0f;
            const auto r = run (s);
            std::printf ("%-6s      rms %+6.2f dB  peak %.3f\n", names[m], db (r.rmsOut / r.rmsIn), r.peakOut);

            if (! r.finite || r.peakOut > 2.0)
            {
                std::printf ("FAIL: mode %d unstable\n", m);
                ++failures;
            }
        }
    }

    // 6. every weird toggle still runs
    {
        const char* names[] = { "fold", "cheb", "comb", "smear", "whisp", "frz", "mirr", "blur" };

        for (int i = 0; i < 8; ++i)
        {
            Settings s;
            s.lmode = 0;
            s.amount = 55.0f;
            s.weird = 85.0f;
            s.fold = i == 0; s.cheb = i == 1; s.comb = i == 2; s.smear = i == 3;
            s.whisper = i == 4; s.freeze = i == 5; s.mirror = i == 6; s.blurt = i == 7;

            const auto r = run (s);
            std::printf ("fx %-5s    rms %+6.2f dB  peak %.3f\n", names[i], db (r.rmsOut / r.rmsIn), r.peakOut);

            if (! r.finite || r.rmsOut < 1.0e-4)
            {
                std::printf ("FAIL: fx %s\n", names[i]);
                ++failures;
            }
        }
    }

    // post filter actually cuts
    {
        Settings s;
        s.amount = 0.0f;
        const auto open = run (s);

        s.ftype = 2;      // LP24
        s.cutoff = 500.0f;
        const auto lp = run (s);

        const double drop = db (lp.rmsOut / open.rmsOut);
        std::printf ("lp24 500hz  %+6.2f dB vs open\n", drop);

        if (drop > -6.0)
        {
            std::printf ("FAIL: LP24 barely attenuates\n");
            ++failures;
        }
    }

    // 7. randomiser must not touch mix / out
    {
        BitrotProcessor proc;
        juce::Random rng (99);
        const float mixBefore = proc.apvts.getRawParameterValue ("mix")->load();
        const float outBefore = proc.apvts.getRawParameterValue ("out")->load();

        proc.randomiseParameters (rng);

        const bool mixOk = std::abs (proc.apvts.getRawParameterValue ("mix")->load() - mixBefore) < 1.0e-4f;
        const bool outOk = std::abs (proc.apvts.getRawParameterValue ("out")->load() - outBefore) < 1.0e-4f;
        std::printf ("randomiser  mix %s  out %s\n", mixOk ? "ok" : "FAIL", outOk ? "ok" : "FAIL");

        if (! mixOk || ! outOk)
            ++failures;
    }

    if (failures == 0)
        std::printf ("ALL OK\n");
    else
        std::printf ("%d FAILURE(S)\n", failures);

    return failures == 0 ? 0 : 1;
}
