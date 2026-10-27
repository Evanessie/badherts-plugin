/* Renders the 4:3 promo poster with the same pixel toolkit the plugin uses. */
#include <JuceHeader.h>

#include <cstdio>

#include "pixel/PixelIcons.h"
#include "pixel/PixelWidgets.h"

using namespace tag::pixel;

namespace
{
constexpr int W = 1200;
constexpr int H = 900;
}

int main (int argc, char** argv)
{
    const char* shotPath = argc > 1 ? argv[1] : "bitrot.png";
    const char* outPath  = argc > 2 ? argv[2] : "promo.png";

    juce::ScopedJuceInitialiser_GUI init;

    juce::Image img (juce::Image::ARGB, W, H, true);
    juce::Graphics g (img);

    g.fillAll (bg);

    // ---- background texture ------------------------------------------------
    g.setColour (hair.withAlpha (0.55f));

    for (int y = 18; y < H; y += 36)
        for (int x = 18; x < W; x += 36)
            g.fillRect (x, y, 2, 2);

    const juce::Colour palette[] = { green, green.interpolatedWith (cyan, 0.5f), cyan, pink,
                                     cyan.darker (0.3f), green.brighter (0.4f) };

    scatterNoiseEx (g, { 0, 0, W, 200 }, 0x9a11u, 46, palette, 6, 0.45f);
    scatterNoiseEx (g, { 0, 640, W, 260 }, 0x9a12u, 52, palette, 6, 0.40f);
    scatterNoiseEx (g, { 0, 200, 520, 440 }, 0x9a13u, 55, palette, 6, 0.35f);

    // ---- header -------------------------------------------------------------
    g.setColour (green);
    g.fillRect (60, 54, 92, 92);
    drawTextCentred (g, "B", { 60, 54, 92, 92 }, panel, 6);

    drawTitle (g, "ADHERTS", 168, 70, text, 6);

    drawText (g, "Losser", 60, 168, text, 4);

    const juce::Rectangle<int> badge (216, 162, 150, 44);
    g.setColour (blue);
    g.fillRect (badge);
    drawTextCentred (g, "\"999\"", badge, panel, 4);

    drawIcon (g, "heart", badge.getRight() + 14, 168, pink, 4);

    // top right strap
    drawTextRight (g, "LOSSY ARTIFACT GENERATOR", W - 60, 62, dim, 3);
    drawTextRight (g, "VST3  /  STANDALONE", W - 60, 102, dim, 2);

    g.setColour (pink);
    g.fillRect (W - 60 - 460, 146, 460, 4);

    drawIcon (g, "bolt",  W - 240, 168, green, 3);
    drawIcon (g, "ghost", W - 200, 168, cyan,  3);
    drawIcon (g, "skull", W - 160, 168, pink,  3);
    drawIcon (g, "disc",  W - 120, 168, green, 3);

    // ---- feature column -----------------------------------------------------
    struct Feature { const char* icon; const char* line; juce::Colour colour; };

    const Feature features[] = {
        { "dice",    "SIX LOSS MODES",        green },
        { "bolt",    "STANDARD / JITTER",     blue },
        { "ghost",   "P-LOSS / P-REPEAT",     blue },
        { "heartOut","INVERSE / STD+P",       blue },
        { "sparkle", "8 SPECTRAL WEIRD FX",   green },
        { "lock",    "PER-KNOB RANDOM LOCK",  green },
        { "check",   "640 x 640 PIXEL UI",    blue }
    };

    drawText (g, "LOSSY. BUT WRONG.", 60, 236, text, 3);
    drawText (g, "SIX MODES OF DIGITAL DECAY", 60, 292, dim, 2);

    int y = 340;

    for (const auto& f : features)
    {
        drawIcon (g, f.icon, 60, y, f.colour, 2);
        drawText (g, f.line, 96, y + 1, text, 2);
        y += 42;
    }

    // ---- the price tag ------------------------------------------------------
    {
        const juce::Rectangle<int> tag (56, 652, 486, 130);

        g.setColour (blue.withAlpha (0.85f));
        g.fillRect (tag.translated (10, 10));
        g.setColour (pink);
        g.fillRect (tag);
        drawFrame (g, tag, text, 4);

        // punch hole, like a real swing tag
        g.setColour (panel);
        g.fillRect (tag.getX() + 14, tag.getY() + 14, 12, 12);
        g.setColour (text);
        g.fillRect (tag.getX() + 17, tag.getY() + 17, 6, 6);

        drawText (g, "FREE", tag.getX() + 44, tag.getY() + 12, panel, 6);
        drawIcon (g, "heart", tag.getRight() - 56, tag.getY() + 18, panel, 3);

        juce::Font hz ("Microsoft YaHei", 25.0f, juce::Font::plain);
        g.setFont (hz);
        g.setColour (panel);
        g.drawText (juce::String::fromUTF8 ("此插件免费  ·  比某合成器便宜 999"),
                    tag.getX() + 44, tag.getY() + 72, tag.getWidth() - 60, 34,
                    juce::Justification::centredLeft, false);
    }

    // ---- hero shot ----------------------------------------------------------
    juce::Image shot = juce::ImageFileFormat::loadFrom (juce::File::getCurrentWorkingDirectory()
                                                            .getChildFile (shotPath));

    const int heroSize = 600;
    const juce::Rectangle<int> hero (556, 196, heroSize, heroSize);

    g.setColour (edge.withAlpha (0.5f));
    g.fillRect (hero.translated (10, 10).withSizeKeepingCentre (heroSize, heroSize));
    g.setColour (panel);
    g.fillRect (hero);

    if (shot.isValid())
        g.drawImage (shot, hero.toFloat(), juce::RectanglePlacement::stretchToFit);

    drawFrame (g, hero, edge, 2);

    // corner brackets so the shot reads as a framed card
    g.setColour (green);

    const int arm = 28;
    const int o = 12;

    g.fillRect (hero.getX() - o, hero.getY() - o, arm, 4);
    g.fillRect (hero.getX() - o, hero.getY() - o, 4, arm);
    g.fillRect (hero.getRight() + o - arm + 4, hero.getY() - o, arm, 4);
    g.fillRect (hero.getRight() + o - 4, hero.getY() - o, 4, arm);
    g.fillRect (hero.getX() - o, hero.getBottom() + o - 4, arm, 4);
    g.fillRect (hero.getX() - o, hero.getBottom() + o - arm, 4, arm);
    g.fillRect (hero.getRight() + o - arm + 4, hero.getBottom() + o - 4, arm, 4);
    g.fillRect (hero.getRight() + o - 4, hero.getBottom() + o - arm, 4, arm);

    // ---- bottom bar ---------------------------------------------------------
    drawHairline (g, 60, W - 60, 806, hair, 0x77a1u);

    {
        juce::Font credit ("Microsoft YaHei", 22.0f, juce::Font::plain);
        g.setFont (credit);
        g.setColour (dim);
        g.drawText (juce::String::fromUTF8 ("by Evanessie999  and  最爱999的大肥鱼"),
                    60, 826, 720, 30, juce::Justification::centredLeft, false);
    }

    drawTextRight (g, "BADHERTS SERIES  /  01", W - 60, 834, dim, 2);

    // colour swatch strip
    const juce::Colour strip[] = { green, green.interpolatedWith (cyan, 0.5f), cyan, blue, pink };

    for (int i = 0; i < 5; ++i)
    {
        g.setColour (strip[i]);
        g.fillRect (60 + i * 96, 868, 88, 12);
    }

    // ---- frame --------------------------------------------------------------
    drawFrame (g, { 0, 0, W, H }, edge, 2);
    drawFrame (g, { 12, 12, W - 24, H - 24 }, hair, 2);

    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (outPath));
    out.deleteFile();

    if (auto stream = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
    {
        juce::PNGImageFormat png;
        png.writeImageToStream (img, *stream);
        std::printf ("OK wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), W, H);
        return 0;
    }

    std::printf ("FAILED to write %s\n", out.getFullPathName().toRawUTF8());
    return 1;
}
