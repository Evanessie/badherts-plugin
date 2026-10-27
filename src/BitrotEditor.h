#pragma once

#include <JuceHeader.h>

#include <vector>

#include "pixel/PixelSpectrum.h"
#include "pixel/PixelWidgets.h"

class BitrotProcessor;

class BitrotEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit BitrotEditor (BitrotProcessor&);
    ~BitrotEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Pulls the newest audio into the spectrum view (also called by the timer). */
    void updateScopeNow();

private:
    void timerCallback() override;
    void randomiseAll();

    BitrotProcessor& processor;

    std::unique_ptr<tag::pixel::PixelSlider> amountSlider, speedSlider, textureSlider, cutoffSlider, resSlider,
        preLowSlider, preHighSlider, weirdSlider, mixSlider, outSlider;
    std::unique_ptr<tag::pixel::PixelSpectrum> spectrum;
    std::unique_ptr<tag::pixel::PixelMatrix> matrix;
    float matrixLevels[8] { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    std::unique_ptr<tag::pixel::PixelTabs> tabs, fftTabs;
    std::unique_ptr<tag::pixel::PixelButton> noiseButton, randomButton, foldButton, chebButton, combButton, smearButton,
        whisperButton, freezeButton, mirrorButton, blurButton;
    std::unique_ptr<tag::pixel::PixelStepper> filterStepper, typeStepper, lfoTargetStepper;
    std::unique_ptr<tag::pixel::PixelSlider> lfoRateSlider, lfoDepthSlider;
    std::unique_ptr<tag::pixel::PixelLfo> lfoScope;
    std::unique_ptr<tag::pixel::PixelField> fieldControl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> foldAttach, chebAttach, combAttach, smearAttach,
        whisperAttach, freezeAttach, mirrorAttach, blurAttach;

    // fft scratch
    std::vector<float> timeBuffer;
    std::vector<float> magnitudes;
    std::vector<float> smoothed;

    int tick = 0;          // drives the slow blink of the decorations
    int lastMode = -1;
    bool lastHideDots = false;
    float lastLoss = -1.0f; // repaint the crumbs when the loss amount moves

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BitrotEditor)
};
