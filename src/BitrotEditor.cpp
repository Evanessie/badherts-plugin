#include "BitrotEditor.h"

#include "BitrotProcessor.h"
#include "pixel/PixelIcons.h"

using namespace tag::pixel;

namespace
{
constexpr int kWidth = 640;
constexpr int kHeight = 640;

juce::String pct (double v)      { return juce::String (juce::roundToInt (v)) + "%"; }
juce::String bitsText (double v) { return juce::String (juce::roundToInt (v)) + "BIT"; }
juce::String hzText (double v)
{
    if (v >= 1000.0)
        return juce::String (v / 1000.0, v >= 10000.0 ? 1 : 2) + "K";

    return juce::String (juce::roundToInt (v)) + "HZ";
}
juce::String dbText (double v)   { return juce::String (v, 1) + "DB"; }

//==============================================================================
void fftRadix2 (std::vector<float>& re, std::vector<float>& im)
{
    const int n = (int) re.size();

    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;

        if (i < j)
        {
            std::swap (re[(size_t) i], re[(size_t) j]);
            std::swap (im[(size_t) i], im[(size_t) j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const float ang = -2.0f * juce::MathConstants<float>::pi / (float) len;
        const float wr = std::cos (ang), wi = std::sin (ang);

        for (int i = 0; i < n; i += len)
        {
            float cr = 1.0f, ci = 0.0f;

            for (int k = 0; k < len / 2; ++k)
            {
                const int a = i + k, b = a + len / 2;
                const float ur = re[(size_t) a], ui = im[(size_t) a];
                const float vr = re[(size_t) b] * cr - im[(size_t) b] * ci;
                const float vi = re[(size_t) b] * ci + im[(size_t) b] * cr;
                re[(size_t) a] = ur + vr; im[(size_t) a] = ui + vi;
                re[(size_t) b] = ur - vr; im[(size_t) b] = ui - vi;

                const float ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
}

void computeMagnitudes (const std::vector<float>& time, double sampleRate, std::vector<float>& out)
{
    const int n = (int) time.size();

    if (n < 64)
    {
        out.assign ((size_t) PixelSpectrum::kBins, 0.0f);
        return;
    }

    std::vector<float> re ((size_t) n), im ((size_t) n, 0.0f);

    for (int i = 0; i < n; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                * (float) i / (float) (n - 1));
        re[(size_t) i] = time[(size_t) i] * w;
    }

    fftRadix2 (re, im);

    out.assign ((size_t) PixelSpectrum::kBins, 0.0f);
    const double binHz = sampleRate / (double) n;
    const int maxBin = n / 2 - 1;

    for (int b = 0; b < PixelSpectrum::kBins; ++b)
    {
        const float t0 = (float) b / (float) (PixelSpectrum::kBins - 1);
        const float t1 = (float) (b + 1) / (float) (PixelSpectrum::kBins - 1);
        const float f0 = PixelSpectrum::kMinHz * std::pow (PixelSpectrum::kMaxHz / PixelSpectrum::kMinHz, t0);
        const float f1 = PixelSpectrum::kMinHz * std::pow (PixelSpectrum::kMaxHz / PixelSpectrum::kMinHz, t1);

        int i0 = (int) std::floor (f0 / binHz);
        int i1 = (int) std::ceil (f1 / binHz);
        i0 = juce::jlimit (1, maxBin, i0);
        i1 = juce::jlimit (i0 + 1, maxBin, i1);

        float best = 0.0f;

        for (int i = i0; i <= i1 && i <= maxBin; ++i)
        {
            const float m = 2.0f * std::sqrt (re[(size_t) i] * re[(size_t) i]
                                              + im[(size_t) i] * im[(size_t) i]) / (float) n;
            best = juce::jmax (best, m);
        }

        out[(size_t) b] = best;
    }
}
} // namespace

//==============================================================================
BitrotEditor::BitrotEditor (BitrotProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setOpaque (true);
    setResizable (false, false);

    auto& state = processor.apvts;

    amountSlider  = std::make_unique<PixelSlider> (state, "amount", "AMOUNT", pink, pct, true, "heart");
    speedSlider   = std::make_unique<PixelSlider> (state, "speed", "SPEED", blue, pct, true, "sparkle");
    textureSlider = std::make_unique<PixelSlider> (state, "texture", "TEXTURE", blue, pct, true, "star");
    pmixSlider    = std::make_unique<PixelSlider> (state, "pmix", "P-MIX", green, pct, true, "skull");
    preLowSlider  = std::make_unique<PixelSlider> (state, "preLow", "PRE-LO", green, hzText, true, "eye");
    preHighSlider = std::make_unique<PixelSlider> (state, "preHigh", "PRE-HI", pink, hzText, true, "heartOut");
    weirdSlider = std::make_unique<PixelSlider> (state, "weird", "WEIRD", pink, pct, true, "bolt");
    mixSlider    = std::make_unique<PixelSlider> (state, "mix", "MIX", green, pct, true, "check");
    outSlider    = std::make_unique<PixelSlider> (state, "out", "OUT", blue, dbText, true, "arrow");

    spectrum = std::make_unique<PixelSpectrum> (state);
    matrix = std::make_unique<PixelMatrix>();
    fieldControl = std::make_unique<PixelField> (state);

    tabs = std::make_unique<PixelTabs> (state, "mode",
                                        juce::StringArray { "CLEAN", "PHONE", "RADIO", "BROKEN" },
                                        green);
    tabs->setIcons ({ "check", "disc", "bolt", "skull" });

    filterStepper = std::make_unique<PixelStepper> (state, "lmode", text, "->");
    filterStepper->setIcons ({ "disc", "bolt", "ghost", "arrow", "heartOut", "sparkle" });
    foldButton  = std::make_unique<PixelButton> ("FOLD", pink);
    chebButton  = std::make_unique<PixelButton> ("CHEB", pink);
    combButton  = std::make_unique<PixelButton> ("COMB", pink);
    smearButton = std::make_unique<PixelButton> ("SMEAR", pink);

    foldButton->setClickingTogglesState (true);
    chebButton->setClickingTogglesState (true);
    combButton->setClickingTogglesState (true);
    smearButton->setClickingTogglesState (true);

    foldAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "fx_fold", *foldButton);
    chebAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "fx_cheb", *chebButton);
    combAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "fx_comb", *combButton);
    smearAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "fx_smear", *smearButton);

    whisperButton = std::make_unique<PixelButton> ("WHISP", pink);
    freezeButton  = std::make_unique<PixelButton> ("FRZ", pink);
    mirrorButton  = std::make_unique<PixelButton> ("MIRR", pink);
    blurButton    = std::make_unique<PixelButton> ("BLUR", pink);

    whisperButton->setClickingTogglesState (true);
    freezeButton->setClickingTogglesState (true);
    mirrorButton->setClickingTogglesState (true);
    blurButton->setClickingTogglesState (true);

    whisperAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "fx_whisper", *whisperButton);
    freezeAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "fx_freeze", *freezeButton);
    mirrorAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "fx_mirror", *mirrorButton);
    blurAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "fx_blurt", *blurButton);

    lfoRateSlider  = std::make_unique<PixelSlider> (state, "lfoRate", "RATE", pink,
                                                    [](double v) { return juce::String (v, v < 10.0 ? 2 : 1) + "HZ"; });
    lfoDepthSlider = std::make_unique<PixelSlider> (state, "lfoDepth", "DEPTH", pink, pct);
    lfoScope       = std::make_unique<PixelLfo> (state);

    lfoTargetStepper = std::make_unique<PixelStepper> (state, "lfoTarget", pink, "->");

    fftTabs = std::make_unique<PixelTabs> (state, "fft",
                                           juce::StringArray { "256", "512", "1024" },
                                           blue);

    noiseButton = std::make_unique<PixelButton> ("NO DOTS", green);
    noiseButton->setIcon ("eye");
    noiseButton->setClickingTogglesState (true);
    noiseButton->onClick = [this] { repaint(); };
    noiseButton->setTooltip ("hide every dot in the interface");

    randomButton = std::make_unique<PixelButton> ("RANDOM", green);
    randomButton->setIcon ("dice");
    randomButton->onClick = [this] { randomiseAll(); };

    for (auto* c : { static_cast<juce::Component*> (noiseButton.get()),
                     static_cast<juce::Component*> (randomButton.get()),
                     static_cast<juce::Component*> (amountSlider.get()),
                     static_cast<juce::Component*> (speedSlider.get()),
                     static_cast<juce::Component*> (textureSlider.get()),
                     static_cast<juce::Component*> (pmixSlider.get()),
                     static_cast<juce::Component*> (preLowSlider.get()),
                     static_cast<juce::Component*> (preHighSlider.get()),
                     static_cast<juce::Component*> (weirdSlider.get()),
                     static_cast<juce::Component*> (mixSlider.get()),
                     static_cast<juce::Component*> (outSlider.get()),
                     static_cast<juce::Component*> (spectrum.get()),
                     static_cast<juce::Component*> (matrix.get()),
                     static_cast<juce::Component*> (fieldControl.get()),
                     static_cast<juce::Component*> (tabs.get()),
                     static_cast<juce::Component*> (filterStepper.get()),
                     static_cast<juce::Component*> (fftTabs.get()),
                     static_cast<juce::Component*> (foldButton.get()),
                     static_cast<juce::Component*> (chebButton.get()),
                     static_cast<juce::Component*> (combButton.get()),
                     static_cast<juce::Component*> (smearButton.get()),
                     static_cast<juce::Component*> (whisperButton.get()),
                     static_cast<juce::Component*> (freezeButton.get()),
                     static_cast<juce::Component*> (mirrorButton.get()),
                     static_cast<juce::Component*> (blurButton.get()),
                     static_cast<juce::Component*> (lfoRateSlider.get()),
                     static_cast<juce::Component*> (lfoDepthSlider.get()),
                     static_cast<juce::Component*> (lfoScope.get()),
                     static_cast<juce::Component*> (lfoTargetStepper.get()) })
        addAndMakeVisible (c);

    timeBuffer.assign (1024, 0.0f);
    magnitudes.assign ((size_t) PixelSpectrum::kBins, 0.0f);
    smoothed.assign ((size_t) PixelSpectrum::kBins, 0.0f);

    setSize (kWidth, kHeight);
    startTimerHz (30);
}

BitrotEditor::~BitrotEditor()
{
    stopTimer();
}

//==============================================================================
void BitrotEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    const bool blinkOn = ((tick / 15) % 2) == 0;

    const int mode = juce::jlimit (0, 3, juce::roundToInt (processor.apvts.getRawParameterValue ("mode")->load()));
    const juce::Colour tints[4] = { green, green, cyan, pink };
    const juce::Colour tint = tints[mode];
    const float noiseScale[4] = { 0.35f, 1.0f, 1.9f, 3.0f };

    const float loss = juce::jlimit (0.0f, 1.0f,
                                     processor.apvts.getRawParameterValue ("amount")->load() * 0.01f);

    // CLEAN gets a stripped-back skin: no crumbs, no stickers, hairline frame
    const bool minimal = (mode == 0);
    const bool hideDots = noiseButton != nullptr && noiseButton->getToggleState();
    const int crumbs = (minimal || hideDots) ? 0 : (int) ((14.0f + loss * 58.0f) * noiseScale[mode]);

    const juce::Colour palette[] = {
        green,
        green.interpolatedWith (cyan, 0.33f),
        green.interpolatedWith (cyan, 0.66f),
        cyan,
        cyan.darker (0.35f),
        green.brighter (0.45f),
        pink
    };
    constexpr int paletteSize = 7;

    // ---- pixel crumbs: more loss, and rougher modes, more of them ----------
    scatterNoiseEx (g, { 0, vs (2), getWidth(), vs (30) }, 0x51a1u, crumbs / 3 + 4, palette, paletteSize, 0.55f);
    scatterNoiseEx (g, { 0, vs (34), getWidth(), vs (150) }, 0x51a2u, crumbs, palette, paletteSize, 0.45f);
    scatterNoiseEx (g, { 0, vs (196), getWidth(), vs (24) }, 0x51a3u, crumbs / 2 + 4, palette, paletteSize, 0.50f);
    scatterNoiseEx (g, { 0, vs (222), getWidth(), vs (80) }, 0x51a4u, crumbs + 10, palette, paletteSize, 0.50f);
    scatterNoiseEx (g, { 0, vs (302), getWidth(), vs (18) }, 0x51a6u, crumbs / 2 + 4, palette, paletteSize, 0.55f);

    // ---- header ------------------------------------------------------------
    g.setColour (tint);
    g.fillRect (14, 8, 24, 22);
    drawTextCentred (g, "B", { 14, 8, 24, 22 }, panel, 3);
    drawTitle (g, "ADHERTS", 42, 8, text, 3);

    drawTitle (g, "Losser", 172, 14, text, 2);

    // \"999\" sits in a filled badge so the title has one thing that pops
    const juce::Rectangle<int> badge (248, 10, 56, 18);
    g.setColour (blue);
    g.fillRect (badge);
    drawTextCentred (g, "\"999\"", badge, panel, 2);

    if (! minimal && blinkOn)
        drawIcon (g, "heart", badge.getRight() + 8, 13, pink, 2);

    {
        juce::Font credit ("Microsoft YaHei", 16.0f, juce::Font::plain);
        g.setFont (credit);
        g.setColour (dim);
        g.drawText (juce::String::fromUTF8 ("by Evanessie999  and  最爱999的大肥鱼"),
                    12, 40, 238, 22, juce::Justification::centredLeft, false);
    }

    drawIcon (g, "left", 299, 42, dim, 1);
    drawText (g, "FIELD", 310, 42, dim, 1);

    drawHairline (g, 12, getWidth() - 12, 70, hair, 0x11a2u);

    // ---- rules that separate the sections ---------------------------------
    drawHairline (g, 12, getWidth() - 12, 384, hair, 0x22b1u);
    drawHairline (g, 12, getWidth() - 12, 586, hair, 0x22b2u);

    for (int boundary : { 4, 6 })
    {
        g.setColour (hair);
        g.fillRect (10 + 69 * boundary, 394, 1, 186);
    }

    // ---- mode frame: each character gets its own outline -------------------
    drawFrame (g, getLocalBounds(), minimal ? hair : tint.withAlpha (0.55f), minimal ? 1 : kScale);

    if (mode == 3)
    {
        juce::Random cracks (0xc0de);
        g.setColour (pink.withAlpha (0.45f));

        for (int i = 0; i < 3; ++i)
        {
            const int x = vs (20) + (int) (cracks.nextFloat() * (float) (getWidth() - vs (40)));
            const int y = vs (36) + (int) (cracks.nextFloat() * vs (2));

            for (int k = 0; k < 10; ++k)
                g.fillRect (x + k * kScale, y + k, kScale, 1);
        }
    }

    // ---- footer ------------------------------------------------------------
    drawText (g, "SHIFT = FINE   /   2X CLICK = OFF", vs (6), vs (309), dim, 1);
    drawTextRight (g, "VST3 64BIT", getWidth() - vs (6), vs (309), dim, 1);

    if (! minimal)
    {
        drawIcon (g, "heart",   vs (192), vs (310), pink, 1);
        drawIcon (g, "sparkle", vs (202), vs (310), cyan, 1);
        drawIcon (g, "star",    vs (212), vs (310), green, 1);
        drawIcon (g, "check",   vs (222), vs (310), dim, 1);
    }
}

void BitrotEditor::resized()
{
    if (amountSlider == nullptr || spectrum == nullptr)
        return; // resized() can fire before the children exist

    spectrum->setBounds (10, 78, getWidth() - 20, 246);
    matrix->setBounds (342, 12, 48, 48);
    fieldControl->setBounds (254, 34, 44, 30);

    lfoScope->setBounds (10, 330, 72, 24);
    lfoRateSlider->setBounds (90, 330, 206, 24);
    lfoDepthSlider->setBounds (304, 330, 206, 24);
    lfoTargetStepper->setBounds (getWidth() - 10 - 112, 330, 112, 24);

    filterStepper->setBounds (10, 356, 112, 20);

    PixelButton* fx[] = { foldButton.get(), chebButton.get(), combButton.get(), smearButton.get(),
                          whisperButton.get(), freezeButton.get(), mirrorButton.get(),
                          blurButton.get() };

    for (int i = 0; i < 8; ++i)
        fx[i]->setBounds (130 + i * 47, 356, 47, 20);

    fftTabs->setBounds (getWidth() - 10 - 112, 356, 112, 20);

    randomButton->setBounds (528, 42, 104, 20);
    noiseButton->setBounds (416, 42, 104, 20);

    PixelSlider* faders[] = { amountSlider.get(), speedSlider.get(), textureSlider.get(), pmixSlider.get(),
                              preLowSlider.get(), preHighSlider.get(), weirdSlider.get(),
                              mixSlider.get(), outSlider.get() };

    const int count = 9;
    const int top = 390;
    const int height = 190;

    for (int i = 0; i < count; ++i)
        faders[i]->setBounds (10 + i * 69 + 4, top, 61, height);

    tabs->setBounds (416, 6, 212, 32);
}

//==============================================================================
void BitrotEditor::randomiseAll()
{
    juce::StringArray locked;

    for (auto* fader : { amountSlider.get(), speedSlider.get(), textureSlider.get(), pmixSlider.get(),
                         preLowSlider.get(), preHighSlider.get(), weirdSlider.get(),
                         mixSlider.get(), outSlider.get() })
        if (fader != nullptr && fader->isLocked())
            locked.add (fader->getParamID());

    processor.randomiseParameters (juce::Random::getSystemRandom(), locked);
}

//==============================================================================
void BitrotEditor::timerCallback()
{
    const float loss = processor.apvts.getRawParameterValue ("amount")->load();

    if (std::abs (loss - lastLoss) > 1.0f)
    {
        lastLoss = loss;
        repaint(); // the noise crumbs follow the loss amount
    }

    const int mode = juce::roundToInt (processor.apvts.getRawParameterValue ("mode")->load());

    if (mode != lastMode)
    {
        lastMode = mode;
        repaint(); // the frame colour, noise density and cracks follow the mode
    }

    // NO DOTS also strips the matrix and the fader ticks, not just the crumbs
    const bool hideDots = noiseButton != nullptr && noiseButton->getToggleState();

    if (hideDots != lastHideDots)
    {
        lastHideDots = hideDots;
        matrix->setVisible (! hideDots);

        for (auto* fader : { amountSlider.get(), speedSlider.get(), textureSlider.get(), pmixSlider.get(),
                             preLowSlider.get(), preHighSlider.get(), weirdSlider.get(),
                             mixSlider.get(), outSlider.get() })
            fader->setQuiet (hideDots);

        repaint();
    }

    if (++tick % 15 == 0)
        repaint(); // slow blink for the decorations

    updateScopeNow();
}

void BitrotEditor::updateScopeNow()
{
    if (spectrum == nullptr)
        return;

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("mode")))
        tabs->setActive (choice->getIndex());

    const std::atomic<float>* data = processor.getScopeData();
    const int writePos = processor.getScopeWritePosition();
    const int n = (int) timeBuffer.size();

    for (int i = 0; i < n; ++i)
        timeBuffer[(size_t) i] = data[(writePos - n + i + BitrotProcessor::kScopeSize)
                                       % BitrotProcessor::kScopeSize]
                                     .load (std::memory_order_relaxed);

    computeMagnitudes (timeBuffer, processor.getSampleRate(), magnitudes);

    const float decay = 0.82f;

    for (int i = 0; i < (int) smoothed.size(); ++i)
        smoothed[(size_t) i] = juce::jmax (magnitudes[(size_t) i], smoothed[(size_t) i] * decay);

    spectrum->setMagnitudes (smoothed);

    // 5 band mini meter for the matrix
    constexpr int bands = 8;
    const int perBand = juce::jmax (1, (int) smoothed.size() / bands);

    for (int b = 0; b < bands; ++b)
    {
        float sum = 0.0f;
        int count = 0;

        for (int i = b * perBand; i < juce::jmin ((int) smoothed.size(), (b + 1) * perBand); ++i)
        {
            sum += smoothed[(size_t) i];
            ++count;
        }

        const float mag = count > 0 ? sum / (float) count : 0.0f;
        const float db = juce::Decibels::gainToDecibels (mag, -60.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);

        matrixLevels[b] = juce::jmax (norm, matrixLevels[b] * 0.86f);
    }

    matrix->setBands (matrixLevels);
}
