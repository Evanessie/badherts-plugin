#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <vector>

#include "DSP.h"

/** BADHERTS Losser999 - lossy artifact generator.
    Chain: pre-filter -> parameter mapping -> degradation engine -> dry/wet -> safety.
    Everything it needs is allocated in prepareToPlay; processBlock never allocates. */
class BitrotProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kScopeSize = 1024;
    static constexpr int kReportedLatency = 1023;

    enum LossMode { modeStandard = 0, modeJitter, modePacketLoss, modeRepeat, modeInverse, modeStandardPlusPacket, numLossModes };

    BitrotProcessor();
    ~BitrotProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BADHERTS Losser999"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    const std::atomic<float>* getScopeData() const { return scopeData.data(); }
    int getScopeWritePosition() const { return scopeWrite.load (std::memory_order_relaxed); }

    /** Randomises every parameter except mix / out. */
    void randomiseParameters (juce::Random& rng, const juce::StringArray& skip = {});

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void cacheParameters();
    void updateDeviceCharacter (int mode);
    void updatePreFilter();
    void rebuildGate (int fftSize);
    void resetState();

    /** The shared mapping layer: one set of knobs, reinterpreted per mode. */
    struct EngineSettings
    {
        float stdAmount = 0.0f;
        float stdRate = 0.0f;
        float texture = 0.0f;
        float packetAmount = 0.0f;
        float packetMix = 0.0f;
        float packetLen = 0.0f;
        float jitterAmount = 0.0f;
        float jitterRate = 0.0f;
        bool inverse = false;
    };

    static EngineSettings mapParameters (int mode, float amount, float speed, float texture, float pmix);

    struct Ptrs
    {
        std::atomic<float>* amount = nullptr;
        std::atomic<float>* speed = nullptr;
        std::atomic<float>* texture = nullptr;
        std::atomic<float>* pmix = nullptr;
        std::atomic<float>* preLow = nullptr;
        std::atomic<float>* preHigh = nullptr;
        std::atomic<float>* field = nullptr;
        std::atomic<float>* lmode = nullptr;
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* weird = nullptr;
        std::atomic<float>* fxFold = nullptr;
        std::atomic<float>* fxCheb = nullptr;
        std::atomic<float>* fxComb = nullptr;
        std::atomic<float>* fxSmear = nullptr;
        std::atomic<float>* fxWhisper = nullptr;
        std::atomic<float>* fxFreeze = nullptr;
        std::atomic<float>* fxMirror = nullptr;
        std::atomic<float>* fxBlurT = nullptr;
        std::atomic<float>* fft = nullptr;
        std::atomic<float>* gate = nullptr;
        std::atomic<float>* ginv = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* out = nullptr;
        std::atomic<float>* lfoTarget = nullptr;
        std::atomic<float>* lfoRate = nullptr;
        std::atomic<float>* lfoDepth = nullptr;
    } p;

    double currentSampleRate = 44100.0;
    int lastDeviceMode = -1;
    int currentFftSize = 0;
    int padSamples = 0;

    // pre filter
    tag::TptSvf preHighpass[2], preLowpass[2];
    tag::TptSvf fieldHigh;   // splits the S signal for the intensity stage

    // standard loss: two bands with random gain wobble + short pitch drift
    tag::TptSvf bandLow[2], bandHigh[2];
    tag::DelayLine driftDelay[2];
    float bandGain[2][2] { { 1.0f, 1.0f }, { 1.0f, 1.0f } };
    float bandTarget[2][2] { { 1.0f, 1.0f }, { 1.0f, 1.0f } };
    int bandCounter = 0;
    float driftTarget = 0.0f;
    float driftSmooth = 0.0f;
    int driftCounter = 0;

    // phase jitter
    tag::DelayLine jitterDelay[2];
    float jitterPhase = 0.0f;

    // packet loss / repeat
    std::vector<float> history[2];
    int historyPos = 0;
    int packetCountdown = 0;
    int packetRemaining = 0;
    int packetLenSamples = 0;
    bool packetRepeat = false;
    std::vector<float> loopBuffer[2];
    int loopPos = 0;

    // shared
    tag::Filter filter[2];
    tag::SpectralGate spectralGate[2];
    tag::DelayLine dryDelay[2], gatePad[2], combDelay[2];
    tag::DcBlocker dc[2];

    float envelope[2] { 0.0f, 0.0f };
    float lfoPhase = 0.0f;
    float held[2] { 0.0f, 0.0f };

    juce::Random rng { 0x1e5c0de };

    float deviceLowpass = 19000.0f, deviceHighpass = 20.0f, deviceNoise = 0.00015f, deviceMakeup = 1.0f;

    std::array<std::atomic<float>, kScopeSize> scopeData;
    std::atomic<int> scopeWrite { 0 };

    juce::SmoothedValue<float> sMix, sOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BitrotProcessor)
};
