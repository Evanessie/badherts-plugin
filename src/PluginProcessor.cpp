#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::NormalisableRange<float> logRange (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

juce::NormalisableRange<float> linRange (float lo, float hi)
{
    return juce::NormalisableRange<float> (lo, hi);
}
} // namespace

//==============================================================================
TapeGrainProcessor::TapeGrainProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    cacheParameterPointers();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TapeGrainProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addFloat = [&params] (const juce::String& id,
                               const juce::String& name,
                               juce::NormalisableRange<float> range,
                               float def,
                               const juce::String& unit = {})
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 },
            name,
            range,
            def,
            juce::AudioParameterFloatAttributes().withLabel (unit)));
    };

    auto addBool = [&params] (const juce::String& id, const juce::String& name, bool def)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };

    // --- DRIVE -------------------------------------------------------------
    addBool ("drive_on", "Drive On", true);
    addFloat ("drive", "Drive", linRange (0.0f, 36.0f), 9.0f, "dB");
    addFloat ("bias", "Bias", linRange (0.0f, 100.0f), 12.0f, "%");
    addFloat ("tone", "Tone", logRange (200.0f, 16000.0f, 2500.0f), 9000.0f, "Hz");

    // --- CRUSH -------------------------------------------------------------
    addBool ("crush_on", "Crush On", false);
    addFloat ("bits", "Bits", linRange (3.0f, 16.0f), 10.0f, "bit");
    addFloat ("rate", "Rate", logRange (1.0f, 40.0f, 6.0f), 1.0f, "x");
    addFloat ("jitter", "Jitter", linRange (0.0f, 100.0f), 0.0f, "%");

    // --- WOW ---------------------------------------------------------------
    addBool ("wow_on", "Wow On", false);
    addFloat ("wow", "Wow", linRange (0.0f, 100.0f), 30.0f, "%");
    addFloat ("flutter", "Flutter", linRange (0.0f, 100.0f), 12.0f, "%");
    addFloat ("speed", "Speed", logRange (0.25f, 4.0f, 1.0f), 1.0f, "x");

    // --- TAPE --------------------------------------------------------------
    addBool ("tape_on", "Tape On", true);
    addFloat ("hiss", "Hiss", linRange (0.0f, 100.0f), 6.0f, "%");
    addFloat ("lowpass", "Lowpass", logRange (300.0f, 18000.0f, 4000.0f), 16000.0f, "Hz");
    addFloat ("highpass", "Highpass", logRange (20.0f, 500.0f, 100.0f), 30.0f, "Hz");

    // --- GLOBAL ------------------------------------------------------------
    addFloat ("mix", "Mix", linRange (0.0f, 100.0f), 100.0f, "%");
    addFloat ("output", "Output", linRange (-24.0f, 12.0f), 0.0f, "dB");

    return { params.begin(), params.end() };
}

void TapeGrainProcessor::cacheParameterPointers()
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    p.driveOn = get ("drive_on");
    p.drive = get ("drive");
    p.bias = get ("bias");
    p.tone = get ("tone");

    p.crushOn = get ("crush_on");
    p.bits = get ("bits");
    p.rate = get ("rate");
    p.jitter = get ("jitter");

    p.wowOn = get ("wow_on");
    p.wow = get ("wow");
    p.flutter = get ("flutter");
    p.speed = get ("speed");

    p.tapeOn = get ("tape_on");
    p.hiss = get ("hiss");
    p.lowpass = get ("lowpass");
    p.highpass = get ("highpass");

    p.mix = get ("mix");
    p.output = get ("output");

    jassert (p.driveOn != nullptr && p.output != nullptr);
}

//==============================================================================
void TapeGrainProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const double sr = currentSampleRate;

    const int delaySize = (int) std::ceil (sr * 0.1);

    for (int ch = 0; ch < 2; ++ch)
    {
        wowDelay[ch].resize (delaySize);
        wowDelay[ch].reset();
        driveTone[ch].reset();
        lowpassFilter[ch].reset();
        highpassFilter[ch].reset();
        dcBlock[ch].reset();
        holdValue[ch] = 0.0f;
    }

    wowPhase = 0.0f;
    flutterPhase = 0.0f;
    holdCounter = 0;

    constexpr double ramp = 0.02;

    for (auto* s : { &sMix, &sOutput, &sDrive, &sBias, &sBits, &sRate,
                     &sWow, &sFlutter, &sHiss, &sTone, &sLowpass, &sHighpass })
        s->reset (sr, ramp);

    sMix.setCurrentAndTargetValue (p.mix->load() * 0.01f);
    sOutput.setCurrentAndTargetValue (tag::dbToGain (p.output->load()));
    sDrive.setCurrentAndTargetValue (tag::dbToGain (p.drive->load()));
    sBias.setCurrentAndTargetValue (p.bias->load() * 0.01f);
    sBits.setCurrentAndTargetValue (p.bits->load());
    sRate.setCurrentAndTargetValue (p.rate->load());
    sWow.setCurrentAndTargetValue (p.wow->load() * 0.01f);
    sFlutter.setCurrentAndTargetValue (p.flutter->load() * 0.01f);
    sHiss.setCurrentAndTargetValue (p.hiss->load() * 0.01f);
    sTone.setCurrentAndTargetValue (p.tone->load());
    sLowpass.setCurrentAndTargetValue (p.lowpass->load());
    sHighpass.setCurrentAndTargetValue (p.highpass->load());

    updateFilterCoefficients();
}

void TapeGrainProcessor::updateFilterCoefficients()
{
    const float lpHz = p.lowpass != nullptr ? p.lowpass->load() : 16000.0f;
    const float hpHz = p.highpass != nullptr ? p.highpass->load() : 30.0f;
    const float toneHz = p.tone != nullptr ? p.tone->load() : 9000.0f;

    for (int ch = 0; ch < 2; ++ch)
    {
        lowpassFilter[ch].set (lpHz, 0.707f, currentSampleRate);
        highpassFilter[ch].set (hpHz, 0.707f, currentSampleRate);
        driveTone[ch].setCutoff (toneHz, currentSampleRate);
    }
}

bool TapeGrainProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void TapeGrainProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const int numSamples = buffer.getNumSamples();
    const int totalChannels = buffer.getNumChannels();
    const int numChannels = juce::jmin (2, totalChannels);

    for (int ch = numChannels; ch < totalChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const bool driveOn = p.driveOn->load() > 0.5f;
    const bool crushOn = p.crushOn->load() > 0.5f;
    const bool wowOn = p.wowOn->load() > 0.5f;
    const bool tapeOn = p.tapeOn->load() > 0.5f;

    updateFilterCoefficients();

    sMix.setTargetValue (p.mix->load() * 0.01f);
    sOutput.setTargetValue (tag::dbToGain (p.output->load()));
    sDrive.setTargetValue (tag::dbToGain (p.drive->load()));
    sBias.setTargetValue (p.bias->load() * 0.01f);
    sBits.setTargetValue (p.bits->load());
    sRate.setTargetValue (p.rate->load());
    sWow.setTargetValue (p.wow->load() * 0.01f);
    sFlutter.setTargetValue (p.flutter->load() * 0.01f);
    sHiss.setTargetValue (p.hiss->load() * 0.01f);
    sTone.setTargetValue (p.tone->load());
    sLowpass.setTargetValue (p.lowpass->load());
    sHighpass.setTargetValue (p.highpass->load());

    const float jitterAmount = p.jitter->load() * 0.01f;
    const float speed = std::clamp (p.speed->load(), 0.25f, 4.0f);

    const float sr = (float) currentSampleRate;
    const float dt = 1.0f / sr;
    const float wowRate = 0.7f * speed * dt;
    const float flutterRate = 6.5f * speed * dt;
    const float baseDelay = 0.012f * sr;

    float peakIn = 0.0f, peakOut = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mixV = sMix.getNextValue();
        const float outGain = sOutput.getNextValue();
        const float driveGain = sDrive.getNextValue();
        const float biasAmt = sBias.getNextValue();
        const float bitsV = sBits.getNextValue();
        const float rateV = sRate.getNextValue();
        const float wowAmt = sWow.getNextValue();
        const float flutterAmt = sFlutter.getNextValue();
        const float hissAmt = sHiss.getNextValue();

        // ---- modulation sources ------------------------------------------
        wowPhase += wowRate;
        if (wowPhase >= 1.0f)
            wowPhase -= 1.0f;

        flutterPhase += flutterRate;
        if (flutterPhase >= 1.0f)
            flutterPhase -= 1.0f;

        const float wowDepthSamples = wowAmt * 0.0060f * sr;
        const float flutterDepthSamples = flutterAmt * 0.0012f * sr;

        const float modSamples = std::sin (wowPhase * 2.0f * tag::kPi) * wowDepthSamples
                               + std::sin (flutterPhase * 2.0f * tag::kPi + 0.7f) * flutterDepthSamples;

        const float crushDelay = baseDelay + modSamples;

        // ---- sample & hold clock -----------------------------------------
        bool holdNow = false;
        if (crushOn || wowOn)
        {
            if (holdCounter <= 0)
            {
                const float jitterScale = 1.0f + jitterAmount * (rng.nextFloat() * 2.0f - 1.0f) * 0.85f;
                holdCounter = juce::jmax (1, juce::roundToInt (rateV * jitterScale));
                holdNow = true;
            }
            else
            {
                --holdCounter;
            }
        }

        const float crushStep = 2.0f / std::exp2 (bitsV);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            const float dry = data[i];
            float x = dry;

            peakIn = juce::jmax (peakIn, std::abs (dry));

            // ---- DRIVE ----------------------------------------------------
            if (driveOn)
            {
                const float biasOffset = tag::fastTanh (biasAmt * 0.6f);
                float y = x * driveGain + biasAmt * 0.6f;
                y = tag::fastTanh (y) - biasOffset;
                y *= juce::jmax (0.35f, 1.0f / std::sqrt (driveGain));

                const float filtered = driveTone[ch].lp (y);
                x = 0.3f * y + 0.7f * filtered;
            }

            // ---- CRUSH ----------------------------------------------------
            if (crushOn)
            {
                const float q = std::floor (x / crushStep + 0.5f) * crushStep;
                if (holdNow)
                    holdValue[ch] = q;

                x = holdValue[ch];
            }
            else
            {
                holdValue[ch] = x;
            }

            // ---- WOW / FLUTTER -------------------------------------------
            wowDelay[ch].write (x);

            if (wowOn)
                x = wowDelay[ch].read (crushDelay);

            // ---- TAPE -----------------------------------------------------
            if (tapeOn)
            {
                x = highpassFilter[ch].hp (x);
                x = lowpassFilter[ch].lp (x);
                x += (rng.nextFloat() * 2.0f - 1.0f) * hissAmt * 0.10f;
            }

            x = dcBlock[ch].process (x, 0.9995f);

            float out = dry + (x - dry) * mixV;
            out *= outGain;

            if (! std::isfinite (out))
                out = 0.0f;

            data[i] = out;
            peakOut = juce::jmax (peakOut, std::abs (out));
        }
    }

    inputPeak.store (peakIn);
    outputPeak.store (peakOut);
}

//==============================================================================
juce::AudioProcessorEditor* TapeGrainProcessor::createEditor()
{
    return new TapeGrainEditor (*this);
}

void TapeGrainProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TapeGrainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TapeGrainProcessor();
}
