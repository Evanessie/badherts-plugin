#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>
#include <vector>

#include "PixelFont.h"
#include "PixelIcons.h"

namespace tag::pixel
{
//==============================================================================
inline const juce::Colour bg      { 0xfff7f8f4 };   // paper
inline const juce::Colour panel   { 0xffffffff };
inline const juce::Colour panelHi { 0xffe7eee8 };
inline const juce::Colour edge    { 0xffccdbd2 };
inline const juce::Colour hair    { 0xffd9e4dc };
inline const juce::Colour text    { 0xff152e1e };   // ink
inline const juce::Colour dim     { 0xff7d9a88 };
inline const juce::Colour green   { 0xff16a34a };
inline const juce::Colour cyan    { 0xff3aa8dd };   // light blue
inline const juce::Colour blue    { 0xff3aa8dd };
inline const juce::Colour magenta { 0xff16a34a };
inline const juce::Colour amber   { 0xff3aa8dd };
inline const juce::Colour yellow { 0xfff2c14e };
inline const juce::Colour sky    { 0xff6fc9ea };
inline const juce::Colour leaf   { 0xff8fd694 };
inline const juce::Colour pink    { 0xfff48fb1 };   // y2k accent, used sparingly

/** Everything is drawn on a 2x virtual grid so the "pixels" read as pixels. */
constexpr int kScale = 2;

inline int vs (int virtualPixels) { return virtualPixels * kScale; }

//==============================================================================
inline void drawFrame (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour c, int thickness = kScale)
{
    g.setColour (c);
    g.fillRect (r.getX(), r.getY(), r.getWidth(), thickness);
    g.fillRect (r.getX(), r.getBottom() - thickness, r.getWidth(), thickness);
    g.fillRect (r.getX(), r.getY(), thickness, r.getHeight());
    g.fillRect (r.getRight() - thickness, r.getY(), thickness, r.getHeight());
}

/** A hairline with a few random bites taken out of it - rules that look printed,
    not mouse-drawn. */
inline void drawHairline (juce::Graphics& g, int x0, int x1, int y, juce::Colour c,
                          juce::uint32 seed, float gapChance = 0.10f)
{
    juce::Random rng ((juce::int64) seed);
    g.setColour (c);

    int x = x0;

    while (x < x1)
    {
        const int run = kScale * (1 + (int) (rng.nextFloat() * 5.0f));

        if (rng.nextFloat() > gapChance)
            g.fillRect (x, y, juce::jmin (run, x1 - x), 1);

        x += run;
    }
}

/** Same idea, vertical. */
inline void drawHairlineV (juce::Graphics& g, int x, int y0, int y1, juce::Colour c,
                           juce::uint32 seed, float gapChance = 0.10f)
{
    juce::Random rng ((juce::int64) seed);
    g.setColour (c);

    int y = y0;

    while (y < y1)
    {
        const int run = kScale * (1 + (int) (rng.nextFloat() * 5.0f));

        if (rng.nextFloat() > gapChance)
            g.fillRect (x, y, 1, juce::jmin (run, y1 - y));

        y += run;
    }
}

/** Broken digital noise strip, used as a decoration. */inline void drawGlitchStrip (juce::Graphics& g, juce::Rectangle<int> area, juce::uint32 seed,
                             bool sparse = false)
{
    juce::Random rng ((juce::int64) seed);
    const juce::Colour palette[] = { green, cyan, edge, panelHi, green, cyan };

    for (int x = area.getX(); x < area.getRight(); x += kScale)
    {
        if (rng.nextFloat() < (sparse ? 0.72f : 0.42f))
            continue;

        g.setColour (palette[(int) (rng.nextFloat() * 5.99f)]);
        g.fillRect (x, area.getY(), kScale, area.getHeight());
    }
}


/** Random pixel crumbs, so no module ever reads as a clean printed grid. */
inline void scatterNoise (juce::Graphics& g, juce::Rectangle<int> area, juce::uint32 seed,
                          int count, juce::Colour colour)
{
    juce::Random rng ((juce::int64) seed);
    g.setColour (colour);

    for (int i = 0; i < count; ++i)
    {
        const bool big = rng.nextFloat() < 0.22f;
        const int size = big ? kScale * 2 : kScale;
        const int x = area.getX() + (int) (rng.nextFloat() * (float) juce::jmax (1, area.getWidth() - size));
        const int y = area.getY() + (int) (rng.nextFloat() * (float) juce::jmax (1, area.getHeight() - size));
        g.fillRect (x, y, size, size);
    }
}

/** Pixel crumbs in a palette of colours - the lossier the sound, the more of them. */
inline void scatterNoiseEx (juce::Graphics& g, juce::Rectangle<int> area, juce::uint32 seed,
                            int count, const juce::Colour* palette, int paletteSize, float alpha)
{
    juce::Random rng ((juce::int64) seed);

    for (int i = 0; i < count; ++i)
    {
        const bool big = rng.nextFloat() < 0.2f;
        const int size = big ? kScale * 2 : kScale;
        const int x = area.getX() + (int) (rng.nextFloat() * (float) juce::jmax (1, area.getWidth() - size));
        const int y = area.getY() + (int) (rng.nextFloat() * (float) juce::jmax (1, area.getHeight() - size));

        g.setColour (palette[juce::jlimit (0, paletteSize - 1, (int) (rng.nextFloat() * (float) paletteSize))]
                         .withAlpha (alpha));
        g.fillRect (x, y, size, size);
    }
}

/** Classic pixel drop shadow: the text is drawn twice, one step apart. */
inline void drawTitle (juce::Graphics& g, const juce::String& text, int x, int y,
                       juce::Colour c, int scale)
{
    drawText (g, text, x + kScale, y + kScale, c.withAlpha (0.18f), scale);
    drawText (g, text, x, y, c, scale);
}

//==============================================================================
/** A juce::Slider with all the mouse/gesture handling of JUCE, painted as a thin
    pixel fader: hairline track, chunky cap, value above and name below. */
class PixelSlider final : public juce::Slider
{
public:
    PixelSlider (juce::AudioProcessorValueTreeState& state,
                 const juce::String& paramID,
                 juce::String labelText,
                 juce::Colour accentColour,
                 std::function<juce::String (double)> formatter,
                 bool vertical = false,
                 const char* stickerName = "dot")
        : label (std::move (labelText)),
          accent (accentColour),
          format (std::move (formatter)),
          verticalLayout (vertical),
          sticker (stickerName)
    {
        setSliderStyle (vertical ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setVelocityBasedMode (false);
        setMouseDragSensitivity (180);
        setWantsKeyboardFocus (false);

        paramId = paramID;
        attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, *this);
    }

    const juce::String& getParamID() const { return paramId; }
    bool isLocked() const { return locked; }
    void setQuiet (bool q) { quiet = q; repaint(); }
    void setLocked (bool shouldLock) { locked = shouldLock; repaint(); }
    std::function<void()> onLockChanged;

    float proportion()
    {
        return (float) juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        const juce::String valueText = format ? format (getValue()) : juce::String (getValue(), 2);

        if (verticalLayout)
            paintVertical (g, r, valueText);
        else
            paintHorizontal (g, r, valueText);
    }

    void valueChanged() override { repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;

        if (lockArea().expanded (vs (2)).contains (e.getPosition()))
        {
            locked = ! locked;
            repaint();

            if (onLockChanged != nullptr)
                onLockChanged();

            return;
        }

        dragStartValue = getValue();
        juce::Slider::mouseDown (e);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isShiftDown())
        {
            // fine mode: relative, a full sweep takes ~1200 px of travel
            const double range = getMaximum() - getMinimum();
            const double delta = -(double) e.getDistanceFromDragStartY() * range / 1200.0;
            setValue (juce::jlimit (getMinimum(), getMaximum(), dragStartValue + delta),
                      juce::sendNotificationSync);
            return;
        }

        juce::Slider::mouseDrag (e);
    }

private:
    void paintVertical (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& valueText)
    {
        auto valueArea = r.removeFromTop (vs (10));
        auto labelArea = r.removeFromBottom (vs (9));

        // ---- value sticker --------------------------------------------------
        const int tw = textWidth (valueText, 1);
        const int lockSpace = vs (7);
        const int availW = juce::jmax (vs (10), r.getWidth() - lockSpace);
        const int bw = juce::jmin (tw + vs (6), availW - vs (2));
        juce::Rectangle<int> box (availW / 2 - bw / 2, valueArea.getY() + vs (1), bw, vs (8));

        g.setColour (edge.withAlpha (0.55f));
        g.fillRect (box.translated (kScale, kScale));
        g.setColour (accent);
        g.fillRect (box);
        drawTextCentred (g, valueText, box, panel, 1);

        // ---- track ----------------------------------------------------------
        auto track = r.reduced (0, vs (7));
        const int cx = track.getCentreX();
        const float p = proportion();
        const int handleY = track.getBottom() - (int) std::round (p * (float) track.getHeight());

        g.setColour (edge.withAlpha (0.55f));

        for (int y = track.getY(); y < track.getBottom(); y += vs (3))
            g.fillRect (cx, y, 1, vs (2));

        g.setColour (accent);
        g.fillRect (cx - 1, handleY, kScale, track.getBottom() - handleY);

        // uneven little ticks, one length per fader
        juce::Random rng ((juce::int64) label.hashCode());
        g.setColour (edge.withAlpha (0.9f));

        for (int i = 0; ! quiet && i <= 4; ++i)
        {
            const int y = track.getBottom() - i * track.getHeight() / 4;
            g.fillRect (cx + vs (4), juce::jlimit (track.getY(), track.getBottom() - 1, y),
                        vs (2) + (int) (rng.nextFloat() * vs (3)), 1);
        }

        // ---- sticker head ---------------------------------------------------
        constexpr int iconPixels = 4;
        const int iconSize = 7 * iconPixels;
        const int ix = cx - iconSize / 2;
        const int iy = handleY - iconSize / 2;

        g.setColour (panel);
        g.fillRect (ix - kScale, iy - kScale, iconSize + kScale * 2, iconSize + kScale * 2);

        drawIcon (g, sticker, ix + kScale, iy + kScale, edge.withAlpha (0.45f), iconPixels);
        drawIcon (g, sticker, ix, iy, accent, iconPixels);

        drawTextCentred (g, label, labelArea, dim, 1);

        // little lock dot in the top right corner of the sticker
        const auto dot = lockArea();

        if (locked)
        {
            g.setColour (pink);
            g.fillRect (dot);
        }
        else
        {
            drawFrame (g, dot, edge, 1);
        }
    }

    juce::Rectangle<int> lockArea() const
    {
        return { getWidth() - vs (6), vs (2), vs (4), vs (4) };
    }

    void paintHorizontal (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& valueText)
    {
        auto header = r.removeFromTop (vs (7));

        drawText (g, label, header.getX(), header.getY() + 2, dim, 1);
        drawTextRight (g, valueText, header.getRight(), header.getY() + 2, accent, 1);

        auto track = r.reduced (0, juce::jmax (0, r.getHeight() / 2 - vs (2)));
        const int cy = track.getCentreY();
        const float p = proportion();
        const int handleX = track.getX() + (int) std::round (p * (float) (track.getWidth() - 1));

        g.setColour (hair);

        for (int x = track.getX(); x < track.getRight(); x += vs (3))
            g.fillRect (x, cy, vs (2), 1);

        g.setColour (accent);
        g.fillRect (track.getX(), cy - 1, juce::jmax (1, handleX - track.getX()), kScale);

        const int capW = vs (3);
        const int capH = juce::jmin (vs (6), juce::jmax (vs (4), r.getHeight() + vs (2)));

        g.setColour (edge.withAlpha (0.5f));
        g.fillRect (handleX - capW / 2 + kScale, cy - capH / 2 + kScale, capW, capH);
        g.setColour (accent);
        g.fillRect (handleX - capW / 2, cy - capH / 2, capW, capH);
    }

    juce::String label;
    juce::Colour accent;
    std::function<juce::String (double)> format;
    bool verticalLayout;
    bool locked = false;
    bool quiet = false;
    juce::String paramId;
    const char* sticker;
    double dragStartValue = 0.0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
};

//==============================================================================
/** A chunky pixel button - used for the randomiser. */
class PixelButton final : public juce::Button
{
public:
    PixelButton (const juce::String& text, juce::Colour accentColour)
        : juce::Button (text), accent (accentColour)
    {
        setTriggeredOnMouseDown (false);
    }

    void setIcon (const char* iconName) { icon = iconName; repaint(); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds();
        const bool lit = getToggleState() || down;

        g.setColour (lit ? accent : panel);
        g.fillRect (r);
        drawFrame (g, r, lit || over ? accent : edge, kScale);

        const auto ink = lit ? panel : accent;

        if (icon != nullptr)
        {
            const int glyph = 14;
            const int textW = textWidth (getButtonText(), 1);
            const int total = glyph + vs (2) + textW;
            const int x0 = r.getCentreX() - total / 2;

            drawIcon (g, icon, x0, r.getCentreY() - glyph / 2, ink, 2);
            drawText (g, getButtonText(), x0 + glyph + vs (2), r.getCentreY() - 3, ink, 1);
        }
        else
        {
            drawTextCentred (g, getButtonText(), r, ink, 1);
        }
    }

private:
    juce::Colour accent;
    const char* icon = nullptr;
};

//==============================================================================
/** Choice parameter rendered as outlined pixel tabs. */
class PixelTabs final : public juce::Component
{
public:
    PixelTabs (juce::AudioProcessorValueTreeState& state,
               const juce::String& paramID,
               juce::StringArray labels,
               juce::Colour accentColour)
        : items (std::move (labels)), accent (accentColour)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID)))
            choice = p;

        jassert (choice != nullptr);

        if (choice != nullptr)
            active = choice->getIndex();
    }

    void setActive (int index)
    {
        if (index != active)
        {
            active = index;
            repaint();
        }
    }

    void setIcons (juce::StringArray names) { icons = std::move (names); repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int index = hitTest (e.x);

        if (index >= 0 && choice != nullptr)
        {
            choice->setValueNotifyingHost (choice->convertTo0to1 ((float) index));
            setActive (index);
        }
    }

    void paint (juce::Graphics& g) override
    {
        const int n = juce::jmax (1, items.size());
        const int w = getWidth() / n;

        for (int i = 0; i < n; ++i)
        {
            auto cell = juce::Rectangle<int> (i * w, 0, w, getHeight()).reduced (vs (1), vs (1));
            const bool on = (i == active);

            g.setColour (on ? accent : panel);
            g.fillRect (cell);

            if (! on)
                drawFrame (g, cell, edge, 1);

            const auto ink = on ? panel : dim;
            const bool hasIcon = i < icons.size() && icons[i].isNotEmpty() && icons[i] != "-";
            const int iconW = hasIcon ? 9 : 0;
            const int textW = textWidth (items[i], 1);
            const int total = iconW + (hasIcon ? vs (1) : 0) + textW;
            const int x0 = cell.getCentreX() - total / 2;

            if (hasIcon)
                drawIcon (g, icons[i].toRawUTF8(), x0, cell.getCentreY() - 4, ink, 1);

            drawText (g, items[i], x0 + iconW + (hasIcon ? vs (1) : 0),
                      cell.getCentreY() - 3, ink, 1);
        }
    }

private:
    int hitTest (int x) const
    {
        const int n = juce::jmax (1, items.size());

        for (int i = 0; i < n; ++i)
            if (x >= i * (getWidth() / n) && x < (i + 1) * (getWidth() / n))
                return i;

        return -1;
    }

    juce::StringArray items, icons;
    juce::Colour accent;
    juce::AudioParameterChoice* choice = nullptr;
    int active = 0;
};

//==============================================================================
/** Choice selector you swipe through left/right instead of clicking one by one. */
class PixelStepper final : public juce::Component,
                           private juce::Timer
{
public:
    PixelStepper (juce::AudioProcessorValueTreeState& state,
                  const juce::String& paramID,
                  juce::Colour accentColour,
                  juce::String prefixText = {})
        : accent (accentColour), prefix (std::move (prefixText))
    {
        choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID));
        jassert (choice != nullptr);

        setInterceptsMouseClicks (true, true);
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        startTimerHz (20);
    }

    void setIcons (juce::StringArray names) { icons = std::move (names); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();

        g.setColour (panel);
        g.fillRect (r);
        drawFrame (g, r, edge, kScale);

        const int cy = r.getCentreY();

        // drag hints
        g.setColour (dim);

        for (int i = 0; i < 3; ++i)
        {
            g.fillRect (r.getX() + vs (2) + i * kScale, cy - kScale + i * kScale, kScale, kScale);
            g.fillRect (r.getRight() - vs (2) - (i + 1) * kScale, cy - kScale + i * kScale, kScale, kScale);
        }

        const auto name = iconName();
        const auto label = text();

        if (name.isNotEmpty() && name != "-")
        {
            const int textW = textWidth (label, 1);
            const int total = 14 + vs (2) + textW;
            const int x0 = r.getCentreX() - total / 2;

            drawIcon (g, name.toRawUTF8(), x0, r.getCentreY() - 7, accent, 2);
            drawText (g, label, x0 + 14 + vs (2), r.getCentreY() - 3, accent, 1);
        }
        else
        {
            drawTextCentred (g, label, r, accent, 1);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragAnchor = e.x;
        dragged = false;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int dx = e.x - dragAnchor;

        if (std::abs (dx) >= 14)
        {
            step (dx > 0 ? 1 : -1);
            dragAnchor = e.x;
            dragged = true;
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! dragged)
            step (e.x < getWidth() / 2 ? -1 : 1);
    }

private:
    juce::String text() const
    {
        if (choice == nullptr)
            return prefix;

        return prefix + choice->choices[juce::jlimit (0, choice->choices.size() - 1, choice->getIndex())];
    }

    juce::String iconName() const
    {
        if (choice == nullptr || icons.isEmpty())
            return {};

        const int i = juce::jlimit (0, icons.size() - 1, choice->getIndex());
        return icons[i];
    }

    void step (int delta)
    {
        if (choice == nullptr)
            return;

        const int n = juce::jmax (1, choice->choices.size());
        const int next = (choice->getIndex() + delta + n) % n;
        choice->setValueNotifyingHost (choice->convertTo0to1 ((float) next));
        repaint();
    }

    void timerCallback() override
    {
        const juce::String now = text();

        if (now != lastText)
        {
            lastText = now;
            repaint();
        }
    }

    juce::AudioParameterChoice* choice = nullptr;
    juce::Colour accent;
    juce::StringArray icons;
    juce::String prefix, lastText;
    int dragAnchor = 0;
    bool dragged = false;
};

//==============================================================================
/** Tiny lfo scope: a sine drawn as pixels with a sticker dot running along it. */
class PixelLfo final : public juce::Component,
                       private juce::Timer
{
public:
    explicit PixelLfo (juce::AudioProcessorValueTreeState& state)
    {
        rate = state.getRawParameterValue ("lfoRate");
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();

        g.setColour (panel);
        g.fillRect (r);

        const float rateHz = rate != nullptr ? rate->load() : 1.5f;
        const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float phase = (float) std::fmod (t * (double) rateHz, 1.0);

        g.setColour (hair);

        for (int x = 0; x < r.getWidth(); x += kScale)
        {
            const float p = (float) x / (float) juce::jmax (1, r.getWidth());
            const float y = 0.5f - 0.42f * std::sin (2.0f * juce::MathConstants<float>::pi * p);
            g.fillRect (r.getX() + x, r.getY() + (int) (y * (float) r.getHeight()), kScale, kScale);
        }

        const int dotX = r.getX() + (int) (phase * (float) r.getWidth());
        const float dotY = 0.5f - 0.42f * std::sin (2.0f * juce::MathConstants<float>::pi * phase);

        g.setColour (pink);
        g.fillRect (juce::jlimit (r.getX(), r.getRight() - vs (2), dotX - vs (1)),
                    r.getY() + (int) (dotY * (float) r.getHeight()) - vs (1), vs (2), vs (2));

        drawFrame (g, r, edge, 1);
    }

private:
    void timerCallback() override { repaint(); }

    std::atomic<float>* rate = nullptr;
};

//==============================================================================
/** Stereo field control drawn as a big blue H whose legs spread with the width. */
class PixelField final : public juce::Component,
                         private juce::Timer
{
public:
    explicit PixelField (juce::AudioProcessorValueTreeState& state)
    {
        param = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter ("field"));
        jassert (param != nullptr);

        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const float v = value();
        const float t = juce::jlimit (0.0f, 1.0f, v / 200.0f);

        // the H fades as the field narrows, and flies away when left alone
        const float fieldAlpha = juce::jlimit (0.22f, 1.0f, v / 100.0f);
        const float alpha = fieldAlpha * fade;

        if (alpha <= 0.02f)
            return;

        auto r = getLocalBounds();
        auto glyphArea = r;
        glyphArea.removeFromBottom (vs (5));

        const int cy = glyphArea.getCentreY() - (int) std::round (flyOffset);
        const int stemH = juce::jmin (glyphArea.getHeight() - vs (2), vs (12));
        const int stemW = juce::jmax (2, vs (2));
        const int spread = juce::roundToInt (juce::jmap (t, 2.0f, 11.0f));
        const int cx = glyphArea.getCentreX() - vs (1);

        g.setColour (blue.withAlpha (0.9f * alpha));
        g.fillRect (cx - spread - stemW / 2, cy - stemH / 2, stemW, stemH);
        g.fillRect (cx + spread - stemW / 2, cy - stemH / 2, stemW, stemH);
        g.fillRect (cx - spread, cy - stemW / 2, spread * 2, stemW);

        // subscript 2: the control reads as H2, hydrogen
        drawText (g, "2", cx + spread + stemW / 2 + 2, cy + stemH / 2 - vs (3), blue.withAlpha (alpha), 1);

        // value only while you touch it, so the idle state stays clean
        if (isMouseOverOrDragging() || dragging)
            drawTextCentred (g, juce::String (juce::roundToInt (v)),
                             r.removeFromBottom (vs (5)), blue.withAlpha (alpha), 1);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        touch();

        if (hidden)
        {
            restore();          // first click just brings the H back
            return;
        }

        dragging = true;
        setFromY (e.y);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        touch();

        if (! hidden)
            setFromY (e.y);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = false;
        touch();
        repaint();
    }

    void mouseEnter (const juce::MouseEvent&) override { touch(); }

    void mouseDoubleClick (const juce::MouseEvent&) override { setValue (100.0f); }

private:
    float value() const
    {
        if (param == nullptr)
            return 100.0f;

        return param->convertFrom0to1 (param->getValue());
    }

    void setValue (float v)
    {
        if (param == nullptr)
            return;

        param->setValueNotifyingHost (param->convertTo0to1 (juce::jlimit (0.0f, 200.0f, v)));
        repaint();
    }

    void setFromY (int y)
    {
        const float t = 1.0f - juce::jlimit (0.0f, 1.0f, (float) y / (float) juce::jmax (1, getHeight()));
        setValue (t * 200.0f);
    }

    void touch()
    {
        lastTouchMs = juce::Time::getMillisecondCounter();

        if (hidden)
            restore();
    }

    void restore()
    {
        hidden = false;
        fade = 1.0f;
        flyOffset = 0.0f;
        lastTouchMs = juce::Time::getMillisecondCounter();
        repaint();
    }

    void timerCallback() override
    {
        bool needsRepaint = false;

        if (std::abs (value() - lastValue) > 0.5f)
        {
            lastValue = value();
            needsRepaint = true;
        }

        const juce::uint32 now = juce::Time::getMillisecondCounter();

        if (! hidden && ! dragging && now - lastTouchMs > 10000)
        {
            fade -= 0.035f;
            flyOffset += 2.2f;
            needsRepaint = true;

            if (fade <= 0.0f)
            {
                fade = 0.0f;
                hidden = true;
            }
        }

        if (needsRepaint)
            repaint();
    }

    juce::RangedAudioParameter* param = nullptr;
    float lastValue = -1.0f;
    float fade = 1.0f;
    float flyOffset = 0.0f;
    juce::uint32 lastTouchMs = 0;
    bool dragging = false;
    bool hidden = false;
};

/** 8x8 pixel matrix used as a tiny music visualiser: a ripple that follows the
    spectrum, green in the middle fading to blue at the edges. */
class PixelMatrix final : public juce::Component
{
public:
    static constexpr int kSize = 8;

    PixelMatrix()
    {
        setInterceptsMouseClicks (false, false);

        for (auto& v : levels)
            v = 0.0f;
    }

    void setBands (const float* newLevels)
    {
        for (int i = 0; i < kSize; ++i)
            levels[i] = newLevels[i];

        repaint(); // the ripple moves even when the levels are stable
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        const int gap = 1;
        const int cell = (juce::jmin (r.getWidth(), r.getHeight()) - (kSize - 1) * gap) / kSize;

        const double seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float phase = (float) std::fmod (seconds * 0.45, 1.0);

        for (int y = 0; y < kSize; ++y)
        {
            for (int x = 0; x < kSize; ++x)
            {
                const float dx = ((float) x - 3.5f) / 3.5f;
                const float dy = ((float) y - 3.5f) / 3.5f;
                const float dist = std::sqrt (dx * dx + dy * dy) / 1.4142f;

                const int band = juce::jlimit (0, kSize - 1, (int) (dist * (float) kSize));
                const float level = levels[band];

                const float wave = std::sin ((dist * 8.0f - phase * 2.2f) * juce::MathConstants<float>::pi);
                const float ripple = 0.35f + 0.65f * (0.5f + 0.5f * wave);
                const float bright = level * ripple;

                const int px = r.getX() + x * (cell + gap);
                const int py = r.getY() + y * (cell + gap);

                if (bright < 0.10f)
                {
                    g.setColour (edge.withAlpha (0.4f));
                    g.fillRect (px, py, cell, cell);
                }
                else
                {
                    // heat ramp: leaf -> sky -> yellow, brighter with the level
                    auto base = dist < 0.5f
                                    ? leaf.interpolatedWith (sky, dist * 2.0f)
                                    : sky.interpolatedWith (yellow, (dist - 0.5f) * 2.0f);

                    base = base.withMultipliedBrightness (0.72f + 0.55f * juce::jmin (1.0f, bright));

                    g.setColour (base);
                    g.fillRect (px, py, cell, cell);

                    if (cell > 3)
                    {
                        g.setColour (base.brighter (0.45f));
                        g.fillRect (px, py, cell, 1);
                        g.fillRect (px, py, 1, cell);

                        g.setColour (base.darker (0.45f));
                        g.fillRect (px, py + cell - 1, cell, 1);
                        g.fillRect (px + cell - 1, py, 1, cell);
                    }
                }
            }
        }

        drawFrame (g, getLocalBounds(), edge, 1);
    }

private:
    float levels[kSize] { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
};

/** Waveform view drawn as chunky vertical bars. */
class PixelScope final : public juce::Component
{
public:
    PixelScope() { setInterceptsMouseClicks (false, false); }

    void setSamples (const std::vector<float>& newSamples)
    {
        samples = newSamples;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (panel);
        g.fillRect (r);

        g.setColour (juce::Colour (0xffeef4f0));

        for (int x = r.getX(); x < r.getRight(); x += vs (10))
            g.fillRect (x, r.getY(), 1, r.getHeight());

        const int midY = r.getCentreY();
        g.setColour (juce::Colour (0xffdfe9e2));
        g.fillRect (r.getX(), midY, r.getWidth(), 1);

        if (! samples.empty())
        {
            const int columns = juce::jmax (1, r.getWidth() / vs (2));
            const int perColumn = juce::jmax (1, (int) samples.size() / columns);
            const float halfHeight = (float) (r.getHeight() / 2 - vs (2));
            constexpr float displayGain = 0.62f;

            g.setColour (green);

            for (int c = 0; c < columns; ++c)
            {
                const int start = c * perColumn;
                const int end = juce::jmin ((int) samples.size(), start + perColumn);

                if (start >= end)
                    break;

                float lo = 1.0e9f, hi = -1.0e9f;

                for (int i = start; i < end; ++i)
                {
                    if (! std::isfinite (samples[(size_t) i]))
                        continue;

                    lo = juce::jmin (lo, samples[(size_t) i]);
                    hi = juce::jmax (hi, samples[(size_t) i]);
                }

                if (lo > hi)
                    continue;

                const int y0 = midY - (int) std::round (hi * displayGain * halfHeight);
                const int y1 = midY - (int) std::round (lo * displayGain * halfHeight);
                const int h = juce::jmax (kScale, y1 - y0);

                g.fillRect (r.getX() + c * vs (2), juce::jlimit (r.getY(), r.getBottom() - kScale, y0),
                            vs (2) - 1, juce::jmin (h, r.getHeight()));
            }
        }
    }

private:
    std::vector<float> samples;
};
} // namespace tag::pixel
