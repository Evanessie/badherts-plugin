#pragma once

#include <JuceHeader.h>

#include <cmath>
#include <vector>

#include "../DSP.h"
#include "PixelWidgets.h"

namespace tag::pixel
{
/** Log-frequency spectrum with the user filter's response drawn on top and a
    draggable spectral-gate threshold line. */
class PixelSpectrum final : public juce::Component
{
public:
    static constexpr int kBins = 72;
    static constexpr float kMinHz = 20.0f;
    static constexpr float kMaxHz = 20000.0f;
    static constexpr float kFloorDb = -66.0f;

    explicit PixelSpectrum (juce::AudioProcessorValueTreeState& state)
    {
        gateParam = state.getParameter ("gate");
        invParam = state.getParameter ("ginv");
        preLowParam = state.getParameter ("preLow");
        preHighParam = state.getParameter ("preHigh");
        fftParam = state.getParameter ("fft");

        setInterceptsMouseClicks (true, false);
        mags.assign ((size_t) kBins, 0.0f);
    }

    void setMagnitudes (const std::vector<float>& newMags)
    {
        if ((int) newMags.size() == kBins)
            mags = newMags;

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (panel);
        g.fillRect (r);

        auto labelsArea = r.removeFromBottom (vs (11));
        auto plot = r.reduced (vs (2), vs (3));

        drawGrid (g, plot);
        drawBars (g, plot);
        drawCutMarkers (g, plot);
        drawGate (g, plot);
        drawFooter (g, labelsArea, plot);

        drawFrame (g, getLocalBounds(), edge, 1);
    }

    juce::Rectangle<int> invertBox()
    {
        const auto plot = plotArea();
        const int y = gateY (plot);
        return { plot.getX() + vs (3),
                 juce::jlimit (plot.getY(), plot.getBottom() - vs (6), y - vs (7)),
                 vs (5), vs (5) };
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (invertBox().expanded (vs (1)).contains (e.getPosition()) && invParam != nullptr)
        {
            invParam->setValueNotifyingHost (invParam->getValue() > 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }

        if (std::abs (e.y - gateY (plotArea())) <= 9)
            dragMode = 1;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragMode != 1 || gateParam == nullptr)
            return;

        auto plot = plotArea();
        const float amount = juce::jlimit (0.0f, 1.0f, (dbForY ((float) e.y, plot) - kFloorDb) / 60.0f);
        gateParam->setValueNotifyingHost (gateParam->convertTo0to1 (amount * 100.0f));
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (gateParam == nullptr)
            return;

        if (std::abs (gateY (plotArea()) - getMouseXYRelative().y) > 12)
            return;

        const float amount = currentValue (gateParam);

        if (amount > 1.0f)
        {
            lastGateAmount = amount;                       // remember it for the next double click
            gateParam->setValueNotifyingHost (gateParam->convertTo0to1 (0.0f));
        }
        else
        {
            gateParam->setValueNotifyingHost (gateParam->convertTo0to1 (juce::jmax (10.0f, lastGateAmount)));
        }

        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { dragMode = 0; }

private:
    juce::Rectangle<int> plotArea() const
    {
        auto r = getLocalBounds();
        r.removeFromBottom (vs (11));
        return r.reduced (vs (2), vs (3));
    }

    static float currentValue (juce::RangedAudioParameter* p)
    {
        return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f;
    }

    float gateDb() const
    {
        return kFloorDb + 60.0f * (currentValue (const_cast<PixelSpectrum*> (this)->gateParam) * 0.01f);
    }

    int gateY (juce::Rectangle<int> plot)
    {
        const int raw = (int) std::round (yForDb (gateDb(), plot));
        return juce::jlimit (plot.getY() + vs (4), plot.getBottom() - vs (7), raw);
    }

    static float yForDb (float db, juce::Rectangle<int> plot)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db - kFloorDb) / (0.0f - kFloorDb));
        return (float) plot.getBottom() - t * (float) plot.getHeight();
    }

    static float dbForY (float y, juce::Rectangle<int> plot)
    {
        const float t = juce::jlimit (0.0f, 1.0f, ((float) plot.getBottom() - y) / (float) juce::jmax (1, plot.getHeight()));
        return kFloorDb + t * (0.0f - kFloorDb);
    }

    static float freqForBin (int i)
    {
        return kMinHz * std::pow (kMaxHz / kMinHz, (float) i / (float) (kBins - 1));
    }

    static float xForFreq (float hz, juce::Rectangle<int> plot)
    {
        const float t = std::log (juce::jlimit (kMinHz, kMaxHz, hz) / kMinHz) / std::log (kMaxHz / kMinHz);
        return (float) plot.getX() + t * (float) plot.getWidth();
    }

    static float freqForX (float x, juce::Rectangle<int> plot)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - (float) plot.getX()) / (float) juce::jmax (1, plot.getWidth()));
        return kMinHz * std::pow (kMaxHz / kMinHz, t);
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<int> plot)
    {
        g.setColour (juce::Colour (0xffeaf1ec));

        for (float hz : { 100.0f, 1000.0f, 10000.0f })
            g.fillRect ((int) xForFreq (hz, plot), plot.getY(), 1, plot.getHeight());

        for (int i = 1; i < 4; ++i)
            g.fillRect (plot.getX(), plot.getY() + plot.getHeight() * i / 4, plot.getWidth(), 1);
    }

    void drawBars (juce::Graphics& g, juce::Rectangle<int> plot)
    {
        for (int i = 0; i < kBins; ++i)
        {
            const int x0 = (int) xForFreq (freqForBin (i), plot);
            const int x1 = (i + 1 < kBins) ? (int) xForFreq (freqForBin (i + 1), plot) : plot.getRight();

            const float db = juce::jlimit (kFloorDb, 0.0f, juce::Decibels::gainToDecibels (mags[(size_t) i], kFloorDb));
            const float norm = (db - kFloorDb) / (0.0f - kFloorDb);
            const int h = juce::jmax (kScale, (int) std::round (norm * (float) plot.getHeight()));

            const float t = (float) i / (float) kBins;
            const juce::Colour tone = cyan.interpolatedWith (green, t * 1.4f);
            g.setColour (tone.withAlpha (0.78f));
            g.fillRect (x0, plot.getBottom() - h, juce::jmax (kScale, x1 - x0 - kScale), h);
        }
    }

    void drawCutMarkers (juce::Graphics& g, juce::Rectangle<int> plot)
    {
        float lo = currentValue (preLowParam);
        float hi = currentValue (preHighParam);
        tag::clampPreFilter (lo, hi);

        const float freqs[2] = { lo, hi };
        const char* names[2] = { "LO", "HI" };

        for (int i = 0; i < 2; ++i)
        {
            const int x = (int) std::round (xForFreq (freqs[i], plot));

            if (x <= plot.getX() + vs (2) || x >= plot.getRight() - vs (2))
                continue;

            g.setColour (cyan.withAlpha (0.5f));

            for (int y = plot.getY() + vs (2); y < plot.getBottom() - vs (1); y += vs (3))
                g.fillRect (x, y, 1, vs (2));

            drawText (g, names[i], x + vs (1), plot.getBottom() - vs (5), cyan, 1);
        }
    }

    void drawGate (juce::Graphics& g, juce::Rectangle<int> plot)
    {
        const float amount = currentValue (gateParam);
        const bool on = amount > 1.0f;
        const int y = gateY (plot);
        lastGateY = y;

        g.setColour (on ? text : edge);

        for (int x = plot.getX(); x < plot.getRight(); x += vs (3))
            g.fillRect (x, y, vs (2), 1);

        g.fillRect (plot.getX(), y - 1, vs (3), 3);
        g.fillRect (plot.getRight() - vs (3), y - 1, vs (3), 3);

        const bool inverted = invParam != nullptr && invParam->getValue() > 0.5f;
        auto box = invertBox();

        // little invert toggle: filled = keep only what is under the threshold
        if (inverted)
        {
            g.setColour (pink);
            g.fillRect (box);
        }
        else
        {
            drawFrame (g, box, edge, 1);
        }

        const juce::String label = on ? juce::String (inverted ? "INV " : "") + "GATE "
                                          + juce::String (juce::roundToInt (gateDb())) + "DB"
                                      : juce::String ("GATE OFF");
        drawText (g, label, box.getRight() + vs (3), box.getY(),
                  on ? text : dim, 1);
    }

    void drawFooter (juce::Graphics& g, juce::Rectangle<int> area, juce::Rectangle<int> plot)
    {
        float lo = currentValue (preLowParam);
        float hi = currentValue (preHighParam);
        tag::clampPreFilter (lo, hi);

        auto hzText = [] (float hz)
        {
            return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + "K"
                                 : juce::String (juce::roundToInt (hz)) + "HZ";
        };

        const juce::String left = "BP " + hzText (lo) + " - " + hzText (hi);

        drawText (g, left, plot.getX() + vs (2), area.getY() + vs (2), text, 1);

        const int fftIndex = juce::roundToInt (currentValue (fftParam));
        const char* fftNames[] = { "256", "512", "1024" };
        drawTextRight (g, juce::String ("FFT ") + fftNames[juce::jlimit (0, 2, fftIndex)],
                       plot.getRight() - vs (2), area.getY() + vs (2), dim, 1);
    }

    juce::RangedAudioParameter* gateParam = nullptr;
    juce::RangedAudioParameter* invParam = nullptr;
    juce::RangedAudioParameter* preLowParam = nullptr;
    juce::RangedAudioParameter* preHighParam = nullptr;
    juce::RangedAudioParameter* fftParam = nullptr;

    std::vector<float> mags;
    int dragMode = 0;
    int lastGateY = -100;
    float lastGateAmount = 45.0f;
};
} // namespace tag::pixel
