/* Offline harness: instantiate TapeGrainProcessor directly and push a sine through it.
   Links against the same shared-code static lib the plugin uses, so this exercises the
   real DSP path. Produces a pass/fail plus level figures. */
#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr double kTwoPi = 6.283185307179586;
} // namespace

static void runCase (const char* name,
                     float driveAmount,
                     bool crush,
                     bool wow,
                     bool tape,
                     double& peakIn,
                     double& peakOut,
                     double& rmsOut)
{
    TapeGrainProcessor proc;
    proc.setPlayConfigDetails (2, 2, kSampleRate, kBlockSize);

    auto setParam = [&proc] (const char* id, float normalised)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (normalised);
        else
            std::printf ("  !! missing parameter %s\n", id);
    };

    setParam ("drive_on", 1.0f);
    setParam ("drive", driveAmount);
    setParam ("crush_on", crush ? 1.0f : 0.0f);
    setParam ("bits", 0.4f);
    setParam ("rate", 0.6f);
    setParam ("wow_on", wow ? 1.0f : 0.0f);
    setParam ("wow", 0.6f);
    setParam ("tape_on", tape ? 1.0f : 0.0f);
    setParam ("hiss", 0.3f);

    proc.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    double phase = 0.0;
    double inPeak = 0.0, outPeak = 0.0, outSumSq = 0.0;
    const int blocks = (int) (kSampleRate * 1.5 / kBlockSize);

    for (int block = 0; block < blocks; ++block)
    {
        for (int i = 0; i < kBlockSize; ++i)
        {
            const float s = (float) (0.5 * std::sin (kTwoPi * 220.0 * phase));
            phase += 1.0 / kSampleRate;
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, s);
            inPeak = std::max (inPeak, std::abs ((double) s));
        }

        proc.processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);

            for (int i = 0; i < kBlockSize; ++i)
            {
                const double y = data[i];

                if (! std::isfinite (y))
                {
                    std::printf ("  !! NON-FINITE sample in %s\n", name);
                    std::exit (1);
                }

                outPeak = std::max (outPeak, std::abs (y));
                outSumSq += y * y;
            }
        }
    }

    proc.releaseResources();

    peakIn = inPeak;
    peakOut = outPeak;
    rmsOut = std::sqrt (outSumSq / (blocks * (double) kBlockSize * 2.0));

    std::printf ("  %-22s in=%.4f outPeak=%.4f outRms=%.4f\n", name, inPeak, outPeak, rmsOut);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    TapeGrainProcessor probe;
    std::printf ("parameters: %d\n", probe.getParameters().size());

    struct Case
    {
        const char* name;
        float drive;
        bool crush, wow, tape;
    };

    const Case cases[] = {
        { "dry-ish (tape only)", 0.0f, false, false, true },
        { "drive heavy", 0.8f, false, false, false },
        { "crush", 0.2f, true, false, false },
        { "wow+flutter", 0.2f, false, true, false },
        { "everything", 0.7f, true, true, true },
    };

    double worstOut = 0.0;

    for (const auto& c : cases)
    {
        double pin = 0.0, pout = 0.0, rms = 0.0;
        runCase (c.name, c.drive, c.crush, c.wow, c.tape, pin, pout, rms);

        if (pout <= 0.0001)
        {
            std::printf ("FAIL: %s produced silence\n", c.name);
            return 1;
        }

        worstOut = std::max (worstOut, pout);
    }

    if (worstOut > 4.0)
    {
        std::printf ("FAIL: runaway gain (peak %.2f)\n", worstOut);
        return 1;
    }

    std::printf ("ALL OK (max peak %.3f)\n", worstOut);
    return 0;
}
