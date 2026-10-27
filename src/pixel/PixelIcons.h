#pragma once

#include <JuceHeader.h>

#include <vector>

#include "PixelFont.h"

/* Tiny 7x7 pixel stickers - hearts, sparkles, and other millennium-era nonsense.
   Same idea as the font: plain text rows so the shapes stay editable. */
namespace tag::pixel
{
struct Icon
{
    const char* name;
    const char* rows[7];
};

inline const std::vector<Icon>& iconTable()
{
    static const std::vector<Icon> table =
    {
        { "heart",   { ".##.##.", "#######", "#######", "#######", ".#####.", "..###..", "...#..." } },
        { "heartOut",{ ".##.##.", "#..#..#", "#.....#", "#.....#", ".#...#.", "..#.#..", "...#..." } },
        { "sparkle", { "...#...", "...#...", "..###..", "#######", "..###..", "...#...", "...#..." } },
        { "star",    { "...#...", "...#...", ".#####.", "#######", ".#####.", "...#...", "...#..." } },
        { "smiley",  { ".#####.", "#.....#", "#.#.#.#", "#.....#", "#.#.#.#", "#..#..#", ".#####." } },
        { "disc",    { ".#####.", "##...##", "#..#..#", "#.#.#.#", "#..#..#", "##...##", ".#####." } },
        { "arrow",   { "##.....", "###....", "####...", "#####..", "####...", "#.#....", "#..#..." } },
        { "dot",     { ".......", ".......", "..###..", "..###..", "..###..", ".......", "......." } },
        { "check",   { "......#", ".....##", "#...##.", "##.##..", ".###...", "..#....", "......." } },
        { "bolt",    { "....#..", "...##..", "..##...", ".#####.", "...##..", "..##...", "..#...." } },
        { "eye",     { ".......", ".#####.", "##...##", "#..#..#", "##...##", ".#####.", "......." } },
        { "skull",   { ".#####.", "#######", "#.#.#.#", "#######", ".#####.", "..#.#..", "...#..." } },
        { "ghost",   { ".#####.", "#######", "##.#.##", "#######", "#######", "#######", "#.#.#.#" } },
        { "lock",    { ".#####.", "#.....#", "#.###.#", "#.#.#.#", "#.#.#.#", "#######", "..#.#.." } },
        { "dice",    { "#######", "#.....#", "#.##..#", "#.....#", "#..##.#", "#.....#", "#######" } },
        { "left",    { "...#...", "..##...", ".###...", "####...", ".###...", "..##...", "...#..." } },
        { "right",   { "...#...", "...##..", "...###.", "...####", "...###.", "...##..", "...#..." } },
    };

    return table;
}

inline void drawIcon (juce::Graphics& g, const char* name, int x, int y,
                      juce::Colour colour, int scale = 1)
{
    g.setColour (colour);

    for (const auto& icon : iconTable())
    {
        if (! juce::String (icon.name).equalsIgnoreCase (name))
            continue;

        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 7; ++col)
                if (icon.rows[row][col] == '#')
                    g.fillRect (x + col * scale, y + row * scale, scale, scale);

        return;
    }
}
} // namespace tag::pixel
