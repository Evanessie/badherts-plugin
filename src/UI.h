#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace tag::ui
{
inline const juce::Colour bg          { 0xff0d0f13 };
inline const juce::Colour panel       { 0xff161a20 };
inline const juce::Colour panelEdge   { 0xff252b34 };
inline const juce::Colour textBright  { 0xffd7dfe8 };
inline const juce::Colour textDim     { 0xff6f7a8a };
inline const juce::Colour trackColour { 0xff272d36 };

/** JUCE 8 moved font construction behind FontOptions; keep one entry point for both. */
inline juce::Font font (float height, int styleFlags = juce::Font::plain)
{
   #if JUCE_MAJOR_VERSION >= 8
    return juce::Font (juce::FontOptions (height, styleFlags));
   #else
    return juce::Font (height, styleFlags);
   #endif
}

//==============================================================================
class Look final : public juce::LookAndFeel_V4
{
public:
    Look() = default;

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float centreX = bounds.getCentreX();
        const float centreY = bounds.getCentreY();
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const bool enabled = slider.isEnabled();

        const float ringRadius = juce::jmax (4.0f, radius - 2.0f);
        const float lineWidth = juce::jmax (2.0f, ringRadius * 0.16f);
        const float arcRadius = ringRadius - lineWidth * 0.5f;

        juce::Path track;
        track.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (trackColour);
        g.strokePath (track, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        if (sliderPosProportional > 0.0005f)
        {
            juce::Path value;
            value.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f,
                                 rotaryStartAngle, angle, true);
            g.setColour (enabled ? fill : fill.withAlpha (0.35f));
            g.strokePath (value, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        const float bodyRadius = juce::jmax (3.0f, ringRadius - lineWidth * 1.9f);
        const auto bodyRect = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f)
                                  .withCentre ({ centreX, centreY });

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a313c), centreX, centreY - bodyRadius,
                                                 juce::Colour (0xff14181e), centreX, centreY + bodyRadius,
                                                 false));
        g.fillEllipse (bodyRect);
        g.setColour (juce::Colour (0xff333b48));
        g.drawEllipse (bodyRect, 1.0f);

        const float pointerThickness = juce::jmax (1.6f, bodyRadius * 0.13f);
        const float pointerLength = bodyRadius * 0.55f;

        juce::Path pointer;
        pointer.addRoundedRectangle (-pointerThickness * 0.5f, -bodyRadius * 0.80f,
                                     pointerThickness, pointerLength,
                                     pointerThickness * 0.5f);

        g.setColour (enabled ? fill : fill.withAlpha (0.4f));
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool over, bool down) override
    {
        auto r = button.getLocalBounds().toFloat().reduced (0.5f);
        auto base = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId)
                                            : juce::Colour (0xff1f242c);

        if (down)
            base = base.darker (0.15f);
        else if (over)
            base = base.brighter (0.12f);

        g.setColour (base);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour (0xff313845).withAlpha (0.9f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setColour (button.getToggleState() ? juce::Colour (0xff0b0d10) : juce::Colour (0xff8b94a2));
        g.setFont (font (10.0f, juce::Font::bold));
        g.drawText (button.getButtonText(), button.getLocalBounds(),
                    juce::Justification::centred, false);
    }
};

//==============================================================================
/** Rotary knob + caption, bound to a parameter. */
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state,
          const juce::String& paramID,
          const juce::String& caption,
          juce::Colour accent,
          std::function<juce::String (float)> formatter)
        : formatFn (std::move (formatter))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                    juce::MathConstants<float>::pi * 2.8f, true);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setColour (juce::Slider::textBoxTextColourId, textDim);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (slider);

        attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);

        // SliderAttachment installs its own text conversion when it is constructed, so our
        // formatter has to go in afterwards or it simply gets overwritten.
        slider.textFromValueFunction = [this] (double v)
        {
            return formatFn != nullptr ? formatFn ((float) v) : juce::String (v, 2);
        };

        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            return text.getDoubleValue();
        };

        slider.updateText();

        captionLabel.setText (caption, juce::dontSendNotification);
        captionLabel.setJustificationType (juce::Justification::centred);
        captionLabel.setColour (juce::Label::textColourId, textDim);
        captionLabel.setFont (font (11.0f));
        captionLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (captionLabel);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        captionLabel.setBounds (r.removeFromTop (13));
        slider.setBounds (r);
    }

private:
    juce::Slider slider;
    juce::Label captionLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    std::function<juce::String (float)> formatFn;
};

//==============================================================================
class Meter final : public juce::Component
{
public:
    Meter (juce::String labelText, juce::Colour accentColour)
        : label (std::move (labelText)), colour (accentColour)
    {
        setInterceptsMouseClicks (false, false);
    }

    void setLevel (float newLevel)
    {
        if (std::abs (newLevel - level) > 0.002f)
        {
            level = newLevel;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        auto labelArea = b.removeFromLeft (30);

        g.setColour (textDim);
        g.setFont (font (10.0f, juce::Font::bold));
        g.drawText (label, labelArea, juce::Justification::centredLeft, false);

        auto bar = b.toFloat().withSizeKeepingCentre ((float) b.getWidth(), 6.0f);

        g.setColour (trackColour);
        g.fillRoundedRectangle (bar, 3.0f);

        const float w = bar.getWidth() * juce::jlimit (0.0f, 1.0f, level);

        if (w > 1.5f)
        {
            g.setColour (colour);
            g.fillRoundedRectangle (bar.withWidth (w), 3.0f);
        }
    }

private:
    juce::String label;
    juce::Colour colour;
    float level = 0.0f;
};

//==============================================================================
/** A titled box holding a power toggle and up to three knobs. */
class ModulePanel final : public juce::Component, private juce::Timer
{
public:
    struct Spec
    {
        juce::String caption;
        juce::String paramID;
        std::function<juce::String (float)> fmt;
    };

    ModulePanel (juce::AudioProcessorValueTreeState& state,
                 juce::String title,
                 juce::Colour accentColour,
                 const juce::String& onParamID,
                 std::vector<Spec> specs)
        : titleText (std::move (title)), accent (accentColour)
    {
        toggle.setButtonText ("ON");
        toggle.setClickingTogglesState (true);
        toggle.setColour (juce::TextButton::buttonOnColourId, accent);
        addAndMakeVisible (toggle);

        toggleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, onParamID, toggle);

        for (auto& s : specs)
        {
            auto knob = std::make_unique<Knob> (state, s.paramID, s.caption, accent, s.fmt);
            addAndMakeVisible (*knob);
            knobs.push_back (std::move (knob));
        }

        startTimerHz (10);
        updateEnabledState();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (9);
        auto header = r.removeFromTop (26);
        toggle.setBounds (header.removeFromRight (34).reduced (0, 4));
        r.removeFromTop (8);

        const int count = juce::jmax (1, (int) knobs.size());
        const int each = r.getHeight() / count;

        for (size_t i = 0; i < knobs.size(); ++i)
            knobs[i]->setBounds (r.removeFromTop (i + 1 == knobs.size() ? r.getHeight() : each));
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();

        g.setColour (panel);
        g.fillRoundedRectangle (r, 9.0f);
        g.setColour (panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);

        g.setColour (accent);
        g.fillRoundedRectangle (14.0f, 29.0f, 16.0f, 2.0f, 1.0f);

        g.setColour (accent);
        g.setFont (font (11.5f, juce::Font::bold));
        g.drawText (titleText, 14, 9, getWidth() - 58, 18, juce::Justification::centredLeft, false);
    }

private:
    void timerCallback() override { updateEnabledState(); }

    void updateEnabledState()
    {
        const bool on = toggle.getToggleState();

        if (on == enabledState)
            return;

        enabledState = on;

        for (auto& k : knobs)
            k->setAlpha (on ? 1.0f : 0.32f);

        repaint();
    }

    juce::String titleText;
    juce::Colour accent;
    juce::TextButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAttach;
    std::vector<std::unique_ptr<Knob>> knobs;
    bool enabledState = true;
};

} // namespace tag::ui
