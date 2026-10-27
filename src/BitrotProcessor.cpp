#include "BitrotProcessor.h"
#include "BitrotEditor.h"

namespace
{
juce::NormalisableRange<float> logRange (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

} // namespace

//==============================================================================
BitrotProcessor::BitrotProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createLayout())
{
    cacheParameters();

    for (auto& s : scopeData)
        s.store (0.0f);

    updateDeviceCharacter ((int) p.mode->load());
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BitrotProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 }, "Character",
        juce::StringArray { "CLEAN", "PHONE", "RADIO", "BROKEN" }, 1));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "lmode", 1 }, "Loss Mode",
        juce::StringArray { "STANDARD", "JITTER", "P.LOSS", "P.REPEAT", "INVERSE", "STD+P" }, 0));

    auto addFloat = [&params] (const juce::String& id, const juce::String& name,
                               juce::NormalisableRange<float> range, float def)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def));
    };

    addFloat ("amount", "Amount", juce::NormalisableRange<float> (0.0f, 100.0f), 45.0f);
    addFloat ("speed", "Speed", juce::NormalisableRange<float> (0.0f, 100.0f), 40.0f);
    addFloat ("texture", "Texture", juce::NormalisableRange<float> (0.0f, 100.0f), 30.0f);
    addFloat ("pmix", "Packet Mix", juce::NormalisableRange<float> (0.0f, 100.0f), 60.0f);

    addFloat ("preLow", "Pre Low", logRange (20.0f, 20000.0f, 400.0f), 60.0f);
    addFloat ("preHigh", "Pre High", logRange (20.0f, 20000.0f, 4000.0f), 16000.0f);

    addFloat ("field", "Field", juce::NormalisableRange<float> (0.0f, 200.0f), 100.0f);

    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "fx_fold", 1 }, "Fold", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "fx_cheb", 1 }, "Chebyshev", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "fx_comb", 1 }, "Comb", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "fx_smear", 1 }, "Smear", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "fx_whisper", 1 }, "Whisper", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "fx_freeze", 1 }, "Freeze", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "fx_mirror", 1 }, "Mirror", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "fx_blurt", 1 }, "Blur Time", false));

    addFloat ("weird", "Weird", juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f);

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "lfoTarget", 1 }, "LFO Target",
        juce::StringArray { "NONE", "AMOUNT", "SPEED", "TEXTURE", "P-MIX", "PRE-LO", "PRE-HI",
                            "WEIRD", "GATE", "MIX", "OUT", "FIELD" }, 1));

    addFloat ("lfoRate", "LFO Rate", logRange (0.05f, 20.0f, 2.0f), 1.5f);
    addFloat ("lfoDepth", "LFO Depth", juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f);

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "fft", 1 }, "FFT",
        juce::StringArray { "256", "512", "1024" }, 2));

    addFloat ("gate", "Gate", juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f);

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "ginv", 1 }, "Gate Invert", false));

    addFloat ("mix", "Mix", juce::NormalisableRange<float> (0.0f, 100.0f), 100.0f);
    addFloat ("out", "Output", juce::NormalisableRange<float> (-24.0f, 12.0f), 0.0f);

    return { params.begin(), params.end() };
}

void BitrotProcessor::cacheParameters()
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    p.amount = get ("amount");
    p.speed = get ("speed");
    p.texture = get ("texture");
    p.pmix = get ("pmix");
    p.preLow = get ("preLow");
    p.preHigh = get ("preHigh");
    p.field = get ("field");
    p.lmode = get ("lmode");
    p.mode = get ("mode");
    p.weird = get ("weird");
    p.fxFold = get ("fx_fold");
    p.fxCheb = get ("fx_cheb");
    p.fxComb = get ("fx_comb");
    p.fxSmear = get ("fx_smear");
    p.fxWhisper = get ("fx_whisper");
    p.fxFreeze = get ("fx_freeze");
    p.fxMirror = get ("fx_mirror");
    p.fxBlurT = get ("fx_blurt");
    p.fft = get ("fft");
    p.gate = get ("gate");
    p.ginv = get ("ginv");
    p.mix = get ("mix");
    p.out = get ("out");
    p.lfoTarget = get ("lfoTarget");
    p.lfoRate = get ("lfoRate");
    p.lfoDepth = get ("lfoDepth");

    jassert (p.amount != nullptr && p.out != nullptr);
}

//==============================================================================
BitrotProcessor::EngineSettings BitrotProcessor::mapParameters (int mode, float amount,
                                                                float speed, float texture,
                                                                float pmix)
{
    EngineSettings e;

    const float a = juce::jlimit (0.0f, 1.0f, amount);
    const float sp = juce::jlimit (0.0f, 1.0f, speed);
    const float tx = juce::jlimit (0.0f, 1.0f, texture);
    const float pm = juce::jlimit (0.0f, 1.0f, pmix);

    switch (mode)
    {
        case modeJitter:
            e.jitterAmount = a;
            e.jitterRate = 2.0f + sp * 60.0f;
            e.texture = tx;
            break;

        case modePacketLoss:
        case modeRepeat:
            e.packetAmount = 0.15f + a * 0.85f;
            e.packetLen = 0.015f + sp * 0.28f;
            e.packetMix = (mode == modeRepeat) ? 1.0f : 0.0f;
            e.texture = tx * 0.3f;
            break;

        case modeInverse:
            e.stdAmount = a;
            e.stdRate = 0.2f + sp * 7.0f;
            e.texture = tx * a;
            e.inverse = true;
            break;

        case modeStandardPlusPacket:
            e.stdAmount = a * 0.8f;
            e.stdRate = 0.2f + sp * 7.0f;
            e.texture = tx * a;
            e.packetAmount = 0.1f + a * 0.7f;
            e.packetLen = 0.01f + sp * 0.18f;
            e.packetMix = pm;
            break;

        case modeStandard:
        default:
            e.stdAmount = a;
            e.stdRate = 0.2f + sp * 7.0f;
            e.texture = tx * a;
            break;
    }

    return e;
}

//==============================================================================
void BitrotProcessor::updateDeviceCharacter (int mode)
{
    switch (mode)
    {
        case 0: deviceLowpass = 19000.0f; deviceHighpass = 20.0f;   deviceNoise = 0.00015f; deviceMakeup = 1.00f; break;
        case 2: deviceLowpass = 8000.0f;  deviceHighpass = 300.0f;  deviceNoise = 0.0016f;  deviceMakeup = 1.10f; break;
        case 3: deviceLowpass = 6000.0f;  deviceHighpass = 450.0f;  deviceNoise = 0.0032f;  deviceMakeup = 1.30f; break;
        case 1:
        default: deviceLowpass = 12000.0f; deviceHighpass = 120.0f; deviceNoise = 0.0006f; deviceMakeup = 1.05f; break;
    }
}

void BitrotProcessor::updatePreFilter()
{
    const float userLo = p.preLow != nullptr ? p.preLow->load() : 60.0f;
    const float userHi = p.preHigh != nullptr ? p.preHigh->load() : 16000.0f;

    float lo = juce::jmax (userLo, deviceHighpass);
    float hi = juce::jmin (userHi, deviceLowpass);
    tag::clampPreFilter (lo, hi);

    for (int ch = 0; ch < 2; ++ch)
    {
        preHighpass[ch].set (lo, 0.707f, currentSampleRate);
        preLowpass[ch].set (hi, 0.707f, currentSampleRate);
    }
}

void BitrotProcessor::rebuildGate (int fftSize)
{
    currentFftSize = fftSize;

    for (int ch = 0; ch < 2; ++ch)
    {
        spectralGate[ch].prepare (fftSize);
        gatePad[ch].resize (2048);
        gatePad[ch].reset();
    }

    padSamples = juce::jmax (0, kReportedLatency - spectralGate[0].latency());
}

void BitrotProcessor::resetState()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        preHighpass[ch].reset();
        preLowpass[ch].reset();
        bandLow[ch].reset();
        bandHigh[ch].reset();
        driftDelay[ch].reset();
        jitterDelay[ch].reset();
        spectralGate[ch].reset();
        dryDelay[ch].reset();
        gatePad[ch].reset();
        combDelay[ch].reset();
        dc[ch].reset();
        fieldHigh.reset();

        envelope[ch] = 0.0f;
        held[ch] = 0.0f;

        std::fill (history[ch].begin(), history[ch].end(), 0.0f);
        std::fill (loopBuffer[ch].begin(), loopBuffer[ch].end(), 0.0f);
    }

    lfoPhase = 0.0f;
    historyPos = 0;
    packetCountdown = 0;
    packetRemaining = 0;
    packetLenSamples = 0;
    packetRepeat = false;
    loopPos = 0;
    bandCounter = 0;
    driftCounter = 0;
    driftSmooth = 0.0f;
    driftTarget = 0.0f;
    jitterPhase = 0.0f;

    for (int b = 0; b < 2; ++b)
    {
        bandGain[b][0] = bandGain[b][1] = 1.0f;
        bandTarget[b][0] = bandTarget[b][1] = 1.0f;
    }
}

void BitrotProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    const int historyLen = (int) (currentSampleRate * 1.2);

    for (int ch = 0; ch < 2; ++ch)
    {
        history[ch].assign ((size_t) historyLen, 0.0f);
        loopBuffer[ch].assign ((size_t) (currentSampleRate * 0.45), 0.0f);

        driftDelay[ch].resize ((int) (currentSampleRate * 0.05));
        jitterDelay[ch].resize (64);
        dryDelay[ch].resize (4096);
        combDelay[ch].resize (4096);

        fieldHigh.set (6000.0f, 0.707f, currentSampleRate);
        bandLow[ch].set (900.0f, 0.707f, currentSampleRate);
        bandHigh[ch].set (900.0f, 0.707f, currentSampleRate);
    }

    rebuildGate ((int) p.fft->load());
    resetState();
    updateDeviceCharacter ((int) p.mode->load());
    updatePreFilter();

    setLatencySamples (kReportedLatency);

    sMix.reset (currentSampleRate, 0.02);
    sOut.reset (currentSampleRate, 0.02);
    sMix.setCurrentAndTargetValue (p.mix->load() * 0.01f);
    sOut.setCurrentAndTargetValue (tag::dbToGain (p.out->load()));
}

bool BitrotProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void BitrotProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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

    const int deviceMode = (int) p.mode->load();

    if (deviceMode != lastDeviceMode)
    {
        lastDeviceMode = deviceMode;
        updateDeviceCharacter (deviceMode);
    }

    const int fftSize = (int) p.fft->load();

    if (fftSize != currentFftSize)
        rebuildGate (fftSize);

    updatePreFilter();

    // ---- parameter mapping layer -----------------------------------------
    const int lossMode = (int) p.lmode->load();

    float amount = p.amount->load() * 0.01f;
    float speed = p.speed->load() * 0.01f;
    float texture = p.texture->load() * 0.01f;
    float pmix = p.pmix->load() * 0.01f;
    float weirdAmount = p.weird->load() * 0.01f;
    float gateAmount = p.gate->load() * 0.01f;
    float mixValue = p.mix->load() * 0.01f;
    float outDb = p.out->load();
    float fieldValue = p.field->load();

    // ---- lfo --------------------------------------------------------------
    const float lfoRateHz = p.lfoRate->load();
    const float lfoDepth = juce::jlimit (0.0f, 1.0f, p.lfoDepth->load() * 0.01f);
    const int lfoTarget = (int) p.lfoTarget->load();
    const bool lfoOn = lfoDepth > 0.001f && lfoTarget != 0;
    const float lfoInc = lfoRateHz / (float) currentSampleRate;
    const float lfoBlock = lfoOn ? std::sin (2.0f * juce::MathConstants<float>::pi * lfoPhase) * lfoDepth : 0.0f;

    switch (lfoTarget)
    {
        case 1:  amount = juce::jlimit (0.0f, 1.0f, amount + lfoBlock); break;
        case 2:  speed = juce::jlimit (0.0f, 1.0f, speed + lfoBlock); break;
        case 3:  texture = juce::jlimit (0.0f, 1.0f, texture + lfoBlock); break;
        case 4:  pmix = juce::jlimit (0.0f, 1.0f, pmix + lfoBlock); break;
        case 5:  updatePreFilter(); break;
        case 6:  updatePreFilter(); break;
        case 7:  weirdAmount = juce::jlimit (0.0f, 1.0f, weirdAmount + lfoBlock); break;
        case 8:  gateAmount = juce::jlimit (0.0f, 1.0f, gateAmount + lfoBlock); break;
        case 9:  mixValue = juce::jlimit (0.0f, 1.0f, mixValue + lfoBlock); break;
        case 10: outDb = juce::jlimit (-24.0f, 12.0f, outDb + lfoBlock * 12.0f); break;
        case 11: fieldValue = juce::jlimit (0.0f, 200.0f, fieldValue + lfoBlock * 100.0f); break;
        default: break;
    }

    sMix.setTargetValue (mixValue);
    sOut.setTargetValue (tag::dbToGain (outDb));

    const EngineSettings eng = mapParameters (lossMode, amount, speed, texture, pmix);

    // ---- cached engine constants (no trig inside the sample loop) ---------
    const float sr = (float) currentSampleRate;
    const int bandPeriod = juce::jmax (32, (int) (sr / (juce::jmax (0.05f, eng.stdRate) * 6.0f)));
    const int driftPeriod = juce::jmax (64, (int) (sr / (juce::jmax (0.05f, eng.stdRate) * 2.0f)));
    const float driftDepthSamples = 0.006f * sr * eng.stdAmount;
    const float driftBaseSamples = 0.010f * sr;

    const float jitterDepthSamples = eng.jitterAmount * 2.5f;
    const float jitterInc = eng.jitterRate / sr;

    const bool packetOn = eng.packetAmount > 0.01f;
    const int packetMaxLen = juce::jmax (16, (int) (sr * 0.4f));

    const float widthGain = juce::jlimit (0.0f, 2.0f, fieldValue * 0.01f);
    const float fieldCollapse = juce::jlimit (0.0f, 1.0f, 1.0f - juce::jmin (1.0f, widthGain));
    const bool fieldActive = numChannels == 2
                             && (std::abs (widthGain - 1.0f) > 0.002f || fieldCollapse > 0.002f);

    const bool gateEnabled = gateAmount > 0.01f;

    tag::SpectralGate::Params gateParams;
    gateParams.smear = (p.fxSmear->load() > 0.5f) && weirdAmount > 0.01f ? weirdAmount : 0.0f;
    gateParams.blurTime = (p.fxBlurT->load() > 0.5f) && weirdAmount > 0.01f ? weirdAmount : 0.0f;
    gateParams.mirror = (p.fxMirror->load() > 0.5f) && weirdAmount > 0.01f ? weirdAmount : 0.0f;
    gateParams.freeze = (p.fxFreeze->load() > 0.5f) && weirdAmount > 0.01f ? weirdAmount : 0.0f;
    gateParams.whisper = (p.fxWhisper->load() > 0.5f) && weirdAmount > 0.01f ? weirdAmount : 0.0f;
    gateParams.gate = gateEnabled;
    gateParams.invert = p.ginv->load() > 0.5f;
    gateParams.threshold = tag::dbToGain (-66.0f + 60.0f * gateAmount);

    const bool fxFoldOn = p.fxFold->load() > 0.5f && weirdAmount > 0.01f;
    const bool fxChebOn = p.fxCheb->load() > 0.5f && weirdAmount > 0.01f;
    const bool fxCombOn = p.fxComb->load() > 0.5f && weirdAmount > 0.01f;
    const float foldDrive = 1.0f + weirdAmount * 7.0f;
    const int chebOrder = 2 + (int) std::round (weirdAmount * 13.0f);
    const float combFeedback = weirdAmount * 0.88f;
    const float combSamples = (0.002f + 0.010f * weirdAmount) * sr;

    const int scopeStep = juce::jmax (1, (int) (currentSampleRate / 48000.0));
    const int historyLen = (int) history[0].size();

    float* channels[2] = { nullptr, nullptr };

    for (int ch = 0; ch < numChannels; ++ch)
        channels[ch] = buffer.getWritePointer (ch);

    for (int i = 0; i < numSamples; ++i)
    {
        // ---- standard loss modulation, updated once per band period -------
        if (eng.stdAmount > 0.0001f)
        {
            if (--bandCounter <= 0)
            {
                bandCounter = bandPeriod;
                const float depth = eng.stdAmount;

                bandTarget[0][0] = 1.0f + depth * (rng.nextFloat() * 2.0f - 1.0f) * 0.55f;
                bandTarget[0][1] = 1.0f + depth * (rng.nextFloat() * 2.0f - 1.0f) * (0.65f + eng.texture * 0.8f);
            }

            if (--driftCounter <= 0)
            {
                driftCounter = driftPeriod;
                driftTarget = (rng.nextFloat() * 2.0f - 1.0f) * driftDepthSamples;
            }
        }

        if (eng.jitterAmount > 0.0001f)
        {
            jitterPhase += jitterInc;

            if (jitterPhase >= 1.0f)
                jitterPhase -= 1.0f;
        }

        // ---- packet scheduler ---------------------------------------------
        if (packetOn && packetRemaining <= 0 && --packetCountdown <= 0)
        {
            packetLenSamples = juce::jlimit (16, packetMaxLen,
                                             (int) (eng.packetLen * (0.6f + rng.nextFloat() * 0.9f) * sr));
            packetRemaining = packetLenSamples;
            packetRepeat = rng.nextFloat() < eng.packetMix;

            if (packetRepeat)
            {
                loopPos = 0;

                for (int ch = 0; ch < numChannels; ++ch)
                    for (int s = 0; s < packetLenSamples; ++s)
                    {
                        const int idx = (historyPos - packetLenSamples + s + historyLen * 2) % historyLen;
                        loopBuffer[ch][(size_t) s] = history[ch][(size_t) idx];
                    }
            }

            const float gap = eng.packetLen * (1.0f + 5.0f * (1.0f - eng.packetAmount) * rng.nextFloat());
            packetCountdown = juce::jmax (1, (int) (gap * sr * 2.0f));
        }

        const bool inPacket = packetRemaining > 0;

        if (eng.stdAmount > 0.0001f)
        {
            bandGain[0][0] += (bandTarget[0][0] - bandGain[0][0]) * 0.004f;
            bandGain[0][1] += (bandTarget[0][1] - bandGain[0][1]) * 0.004f;
            driftSmooth += (driftTarget - driftSmooth) * 0.002f;
        }

        const float mix = sMix.getNextValue();
        const float outGain = sOut.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = channels[ch][i];
            float x = dry;

            // ---- 1. pre filter -------------------------------------------
            x = preHighpass[ch].hp (x);
            x = preLowpass[ch].lp (x);
            const float preFiltered = x;

            envelope[ch] = juce::jmax (envelope[ch] * 0.9993f, std::abs (preFiltered));
            const float noiseGate = juce::jlimit (0.0f, 1.0f, envelope[ch] * 18.0f);

            if (deviceNoise > 0.0f && noiseGate > 0.0f)
                x += (rng.nextFloat() * 2.0f - 1.0f) * deviceNoise * noiseGate;

            // ---- 2. standard loss -----------------------------------------
            if (eng.stdAmount > 0.0001f)
            {
                const float lo = bandLow[ch].lp (x);
                const float hi = x - lo;

                x = lo * bandGain[0][0] + hi * bandGain[0][1];

                driftDelay[ch].write (x);
                x = driftDelay[ch].read (driftBaseSamples + driftSmooth);
            }

            // ---- 3. phase jitter -------------------------------------------
            if (eng.jitterAmount > 0.0001f)
            {
                const float lfo = std::sin (2.0f * juce::MathConstants<float>::pi * jitterPhase);
                const float d = 1.0f + jitterDepthSamples * (0.5f + 0.5f * lfo);

                jitterDelay[ch].write (x);
                x = jitterDelay[ch].read (juce::jlimit (1.0f, 20.0f, d));
            }

            // ---- 4. packet loss / repeat ------------------------------------
            if (inPacket)
                x = packetRepeat ? loopBuffer[ch][(size_t) juce::jmin (loopPos, packetLenSamples - 1)] : 0.0f;

            // ---- 5. inverse --------------------------------------------------
            if (eng.inverse)
                x = juce::jlimit (-2.0f, 2.0f, preFiltered - x);

            // ---- shared weird effects ----------------------------------------
            if (fxFoldOn)
                x = 0.75f * tag::triangleFold (x * foldDrive);

            if (fxChebOn)
                x = 0.8f * tag::chebyshev (chebOrder, x);

            if (fxCombOn)
            {
                const float echo = combDelay[ch].read (combSamples);
                const float summed = x + echo * combFeedback;
                combDelay[ch].write (juce::jlimit (-4.0f, 4.0f, summed));
                x = summed * (1.0f - 0.45f * combFeedback);
            }
            else
            {
                combDelay[ch].write (x);
            }

            history[ch][(size_t) historyPos] = x;

            x = dc[ch].process (x, 0.9985f);

            // ---- spectral gate on the degradation only ----------------------
            const float diff = x - dry;
            float gated = spectralGate[ch].process (diff, gateParams);

            if (padSamples > 0)
            {
                gatePad[ch].write (gated);
                gated = gatePad[ch].read ((float) padSamples);
            }

            dryDelay[ch].write (dry);
            const float dryAligned = dryDelay[ch].read ((float) kReportedLatency);

            const float out = (dryAligned + gated * mix) * outGain;
            channels[ch][i] = std::isfinite (out) ? out : 0.0f;
        }

        // ---- stereo field: M/S width + joint-stereo style high collapse ----
        if (fieldActive)
        {
            const float l = channels[0][i];
            const float r = channels[1][i];
            const float mid = (l + r) * 0.5f;

            float side = (l - r) * 0.5f;
            const float sideHigh = fieldHigh.hp (side);
            const float sideLow = side - sideHigh;

            side = sideLow + sideHigh * (1.0f - 0.85f * fieldCollapse);
            side *= widthGain;

            channels[0][i] = mid + side;
            channels[1][i] = mid - side;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float out = channels[ch][i];

            if (std::abs (out) > 0.9f)
            {
                const float over = std::abs (out) - 0.9f;
                out = std::copysign (0.9f + 0.12f * std::tanh (over * 3.0f), out);
            }

            channels[ch][i] = std::isfinite (out) ? out : 0.0f;
        }

        if (inPacket && --packetRemaining <= 0)
            packetRemaining = 0;

        if (inPacket)
            ++loopPos;

        if ((i % scopeStep) == 0)
        {
            const int pos = scopeWrite.load (std::memory_order_relaxed);
            scopeData[(size_t) pos].store (channels[0][i], std::memory_order_relaxed);
            scopeWrite.store ((pos + 1) % kScopeSize, std::memory_order_relaxed);
        }

        if (++historyPos >= historyLen)
            historyPos = 0;
    }
}

//==============================================================================
juce::AudioProcessorEditor* BitrotProcessor::createEditor()
{
    return new BitrotEditor (*this);
}

void BitrotProcessor::randomiseParameters (juce::Random& rng, const juce::StringArray& skip)
{
    for (auto* parameter : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (parameter);

        if (rp == nullptr)
            continue;

        const auto id = rp->getParameterID();

        if (id == "mix" || id == "out" || skip.contains (id))
            continue;

        rp->setValueNotifyingHost (rng.nextFloat());
    }
}

void BitrotProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BitrotProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BitrotProcessor();
}
