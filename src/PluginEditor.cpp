#include "PluginEditor.h"

#include "PluginProcessor.h"

namespace
{
juce::String formatHz (float hz)
{
    if (hz >= 1000.0f)
        return juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";

    return juce::String (juce::roundToInt (hz)) + " Hz";
}

juce::String formatPercent (float v)
{
    return juce::String (juce::roundToInt (v)) + "%";
}

juce::String formatDb (float v)
{
    return juce::String (v, 1) + " dB";
}

juce::String formatBits (float v)
{
    return juce::String (juce::roundToInt (v)) + " bit";
}

juce::String formatTimes (float v)
{
    return juce::String (v, 2) + "x";
}
} // namespace

//==============================================================================
TapeGrainEditor::TapeGrainEditor (TapeGrainProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&*look);
    setOpaque (true);
    setResizable (false, false);

    using Spec = tag::ui::ModulePanel::Spec;

    drivePanel = std::make_unique<tag::ui::ModulePanel> (
        processor.apvts, "DRIVE", juce::Colour (0xffff8a5b), "drive_on",
        std::vector<Spec> { { "Drive", "drive", formatDb },
                            { "Bias", "bias", formatPercent },
                            { "Tone", "tone", formatHz } });

    crushPanel = std::make_unique<tag::ui::ModulePanel> (
        processor.apvts, "CRUSH", juce::Colour (0xff56d9c7), "crush_on",
        std::vector<Spec> { { "Bits", "bits", formatBits },
                            { "Rate", "rate", formatTimes },
                            { "Jitter", "jitter", formatPercent } });

    wowPanel = std::make_unique<tag::ui::ModulePanel> (
        processor.apvts, "WOW / FLUTTER", juce::Colour (0xffa98bff), "wow_on",
        std::vector<Spec> { { "Wow", "wow", formatPercent },
                            { "Flutter", "flutter", formatPercent },
                            { "Speed", "speed", formatTimes } });

    tapePanel = std::make_unique<tag::ui::ModulePanel> (
        processor.apvts, "TAPE", juce::Colour (0xfff2c46b), "tape_on",
        std::vector<Spec> { { "Hiss", "hiss", formatPercent },
                            { "Lowpass", "lowpass", formatHz },
                            { "Highpass", "highpass", formatHz } });

    mixKnob = std::make_unique<tag::ui::Knob> (processor.apvts, "mix", "MIX",
                                               juce::Colour (0xffd7dfe8), formatPercent);
    outputKnob = std::make_unique<tag::ui::Knob> (processor.apvts, "output", "OUTPUT",
                                                  juce::Colour (0xffd7dfe8), formatDb);

    inMeter = std::make_unique<tag::ui::Meter> ("IN", juce::Colour (0xff4f7cff));
    outMeter = std::make_unique<tag::ui::Meter> ("OUT", juce::Colour (0xff56d9c7));

    for (auto* c : { static_cast<juce::Component*> (drivePanel.get()),
                     static_cast<juce::Component*> (crushPanel.get()),
                     static_cast<juce::Component*> (wowPanel.get()),
                     static_cast<juce::Component*> (tapePanel.get()),
                     static_cast<juce::Component*> (mixKnob.get()),
                     static_cast<juce::Component*> (outputKnob.get()),
                     static_cast<juce::Component*> (inMeter.get()),
                     static_cast<juce::Component*> (outMeter.get()) })
        addAndMakeVisible (c);

    setSize (680, 486);
    startTimerHz (30);
}

TapeGrainEditor::~TapeGrainEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void TapeGrainEditor::paint (juce::Graphics& g)
{
    g.fillAll (tag::ui::bg);

    juce::ColourGradient glow (juce::Colour (0xff1a2029), (float) getWidth() * 0.5f, -140.0f,
                               tag::ui::bg, (float) getWidth() * 0.5f, 320.0f, false);
    g.setGradientFill (glow);
    g.fillRect (getLocalBounds());

    g.setColour (tag::ui::textBright);
    g.setFont (tag::ui::font (21.0f, juce::Font::bold));
    g.drawText ("TapeGrain", 16, 8, 320, 28, juce::Justification::centredLeft, false);

    g.setColour (tag::ui::textDim);
    g.setFont (tag::ui::font (11.0f));
    g.drawText ("tape saturation  /  bit crush  /  wow & flutter  /  noise",
                17, 35, 420, 14, juce::Justification::centredLeft, false);

    auto badge = juce::Rectangle<int> (getWidth() - 104, 16, 88, 20).toFloat();
    g.setColour (juce::Colour (0xff1b2028));
    g.fillRoundedRectangle (badge, 4.0f);
    g.setColour (juce::Colour (0xff3a424f));
    g.drawRoundedRectangle (badge.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (tag::ui::textDim);
    g.setFont (tag::ui::font (10.0f));
    g.drawText ("VST3  v0.1.0", badge.toNearestInt(), juce::Justification::centred, false);
}

void TapeGrainEditor::resized()
{
    if (drivePanel == nullptr || mixKnob == nullptr || inMeter == nullptr)
        return; // resized() can fire before the children exist

    const int margin = 15;
    const int gap = 10;
    const int panelW = (getWidth() - 2 * margin - 3 * gap) / 4;
    const int panelY = 60;
    const int panelH = 296;

    tag::ui::ModulePanel* panels[] = { drivePanel.get(), crushPanel.get(), wowPanel.get(), tapePanel.get() };

    for (int i = 0; i < 4; ++i)
        panels[i]->setBounds (margin + i * (panelW + gap), panelY, panelW, panelH);

    const int bottomY = panelY + panelH + 14;

    mixKnob->setBounds (margin + 4, bottomY, 72, 92);
    outputKnob->setBounds (margin + 82, bottomY, 72, 92);

    inMeter->setBounds (margin + 176, bottomY + 26, 220, 16);
    outMeter->setBounds (margin + 176, bottomY + 52, 220, 16);
}

void TapeGrainEditor::timerCallback()
{
    auto toNorm = [] (float gain)
    {
        const float db = juce::Decibels::gainToDecibels (gain, -60.0f);
        return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    };

    const float inPeak = processor.inputPeak.exchange (0.0f);
    const float outPeak = processor.outputPeak.exchange (0.0f);

    meterIn = juce::jmax (inPeak, meterIn * 0.80f);
    meterOut = juce::jmax (outPeak, meterOut * 0.80f);

    inMeter->setLevel (toNorm (meterIn));
    outMeter->setLevel (toNorm (meterOut));
}
