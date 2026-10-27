#pragma once

#include <JuceHeader.h>

#include "DSP.h"

class TapeGrainProcessor final : public juce::AudioProcessor
{
public:
    TapeGrainProcessor();
    ~TapeGrainProcessor() override = default;

    //============================================================ lifecycle
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //============================================================ descriptors
    const juce::String getName() const override { return "TapeGrain"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    //============================================================ programs
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    //============================================================ state
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //============================================================ public data
    juce::AudioProcessorValueTreeState apvts;

    /** Block peak levels, written by the audio thread, read by the editor. */
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void cacheParameterPointers();
    void updateFilterCoefficients();

    // raw parameter pointers (never null after construction)
    struct Ptrs
    {
        std::atomic<float>* driveOn = nullptr;
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* bias = nullptr;
        std::atomic<float>* tone = nullptr;

        std::atomic<float>* crushOn = nullptr;
        std::atomic<float>* bits = nullptr;
        std::atomic<float>* rate = nullptr;
        std::atomic<float>* jitter = nullptr;

        std::atomic<float>* wowOn = nullptr;
        std::atomic<float>* wow = nullptr;
        std::atomic<float>* flutter = nullptr;
        std::atomic<float>* speed = nullptr;

        std::atomic<float>* tapeOn = nullptr;
        std::atomic<float>* hiss = nullptr;
        std::atomic<float>* lowpass = nullptr;
        std::atomic<float>* highpass = nullptr;

        std::atomic<float>* mix = nullptr;
        std::atomic<float>* output = nullptr;
    } p;

    double currentSampleRate = 44100.0;

    //============================================================ dsp state
    tag::OnePole driveTone[2];
    tag::TptSvf lowpassFilter[2], highpassFilter[2];
    tag::DelayLine wowDelay[2];
    tag::DcBlocker dcBlock[2];

    float wowPhase = 0.0f, flutterPhase = 0.0f;
    int holdCounter = 0;
    float holdValue[2] { 0.0f, 0.0f };
    juce::Random rng;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> sMix, sOutput, sDrive,
        sBias, sBits, sRate, sWow, sFlutter, sHiss, sTone, sLowpass, sHighpass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeGrainProcessor)
};
