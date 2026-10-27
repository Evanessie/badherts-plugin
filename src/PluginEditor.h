#pragma once

#include <JuceHeader.h>

#include "UI.h"

class TapeGrainProcessor;

class TapeGrainEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit TapeGrainEditor (TapeGrainProcessor&);
    ~TapeGrainEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    TapeGrainProcessor& processor;

    juce::SharedResourcePointer<tag::ui::Look> look;

    std::unique_ptr<tag::ui::ModulePanel> drivePanel, crushPanel, wowPanel, tapePanel;
    std::unique_ptr<tag::ui::Knob> mixKnob, outputKnob;
    std::unique_ptr<tag::ui::Meter> inMeter, outMeter;

    float meterIn = 0.0f, meterOut = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeGrainEditor)
};
