#include "BlockFont.h"
#include "Palette.h"
#include <map>

namespace hv::ui
{
namespace
{
struct Glyph { int w; const char* rows[7]; };

const std::map<juce::juce_wchar, Glyph>& glyphs()
{
    static const std::map<juce::juce_wchar, Glyph> g = {
        { 'A', { 5, { "#####", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } } },
        { 'B', { 5, { "####.", "#...#", "#...#", "####.", "#...#", "#...#", "####." } } },
        { 'C', { 5, { "#####", "#....", "#....", "#....", "#....", "#....", "#####" } } },
        { 'D', { 5, { "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####." } } },
        { 'E', { 5, { "#####", "#....", "#....", "####.", "#....", "#....", "#####" } } },
        { 'F', { 5, { "#####", "#....", "#....", "####.", "#....", "#....", "#...." } } },
        { 'G', { 5, { "#####", "#....", "#....", "#..##", "#...#", "#...#", "#####" } } },
        { 'H', { 5, { "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" } } },
        { 'I', { 1, { "#", "#", "#", "#", "#", "#", "#" } } },
        { 'J', { 5, { "....#", "....#", "....#", "....#", "#...#", "#...#", "#####" } } },
        { 'K', { 5, { "#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#" } } },
        { 'L', { 5, { "#....", "#....", "#....", "#....", "#....", "#....", "#####" } } },
        { 'M', { 5, { "#####", "#.#.#", "#.#.#", "#.#.#", "#...#", "#...#", "#...#" } } },
        { 'N', { 5, { "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#" } } },
        { 'O', { 5, { "#####", "#...#", "#...#", "#...#", "#...#", "#...#", "#####" } } },
        { 'P', { 5, { "#####", "#...#", "#...#", "#####", "#....", "#....", "#...." } } },
        { 'Q', { 5, { "#####", "#...#", "#...#", "#...#", "#.#.#", "#..#.", "###.#" } } },
        { 'R', { 5, { "#####", "#...#", "#...#", "#####", "#..#.", "#...#", "#...#" } } },
        { 'S', { 5, { "#####", "#....", "#....", "#####", "....#", "....#", "#####" } } },
        { 'T', { 5, { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." } } },
        { 'U', { 5, { "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", "#####" } } },
        { 'V', { 5, { "#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.." } } },
        { 'W', { 5, { "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", "#####" } } },
        { 'X', { 5, { "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#" } } },
        { 'Y', { 5, { "#...#", "#...#", "#...#", "#####", "..#..", "..#..", "..#.." } } },
        { 'Z', { 5, { "#####", "....#", "...#.", "..#..", ".#...", "#....", "#####" } } },
        { '0', { 5, { "#####", "#...#", "#..##", "#.#.#", "##..#", "#...#", "#####" } } },
        { '1', { 3, { "##.", ".#.", ".#.", ".#.", ".#.", ".#.", "###" } } },
        { '2', { 5, { "#####", "....#", "....#", "#####", "#....", "#....", "#####" } } },
        { '3', { 5, { "#####", "....#", "....#", ".####", "....#", "....#", "#####" } } },
        { '4', { 5, { "#...#", "#...#", "#...#", "#####", "....#", "....#", "....#" } } },
        { '5', { 5, { "#####", "#....", "#....", "#####", "....#", "....#", "#####" } } },
        { '6', { 5, { "#####", "#....", "#....", "#####", "#...#", "#...#", "#####" } } },
        { '7', { 5, { "#####", "....#", "....#", "...#.", "..#..", "..#..", "..#.." } } },
        { '8', { 5, { "#####", "#...#", "#...#", "#####", "#...#", "#...#", "#####" } } },
        { '9', { 5, { "#####", "#...#", "#...#", "#####", "....#", "....#", "#####" } } },
        { '-', { 3, { "...", "...", "...", "###", "...", "...", "..." } } },
        { '+', { 5, { ".....", "..#..", "..#..", "#####", "..#..", "..#..", "....." } } },
        { '/', { 5, { "....#", "....#", "...#.", "..#..", ".#...", "#....", "#...." } } },
        { '.', { 1, { ".", ".", ".", ".", ".", ".", "#" } } },
        { ',', { 1, { ".", ".", ".", ".", ".", "#", "#" } } },
        { ':', { 1, { ".", ".", "#", ".", ".", "#", "." } } },
        { '\'', { 1, { "#", "#", ".", ".", ".", ".", "." } } },
        { '!', { 1, { "#", "#", "#", "#", "#", ".", "#" } } },
        { '?', { 5, { "#####", "....#", "....#", "..###", "..#..", ".....", "..#.." } } },
        { '%', { 5, { "##..#", "##.#.", "...#.", "..#..", ".#...", ".#.##", "#..##" } } },
        { '<', { 4, { "...#", "..#.", ".#..", "#...", ".#..", "..#.", "...#" } } },
        { '>', { 4, { "#...", ".#..", "..#.", "...#", "..#.", ".#..", "#..." } } },
        { '(', { 2, { ".#", "#.", "#.", "#.", "#.", "#.", ".#" } } },
        { ')', { 2, { "#.", ".#", ".#", ".#", ".#", ".#", "#." } } },
        { '&', { 5, { ".###.", "#...#", ".#.#.", "..#..", ".#.##", "#..#.", ".##.#" } } },
        { '#', { 5, { ".#.#.", "#####", ".#.#.", ".#.#.", ".#.#.", "#####", ".#.#." } } },
    };
    return g;
}

const Glyph* glyphFor (juce::juce_wchar c)
{
    const auto& g = glyphs();
    auto it = g.find (juce::CharacterFunctions::toUpperCase (c));
    return it != g.end() ? &it->second : nullptr;
}

/** Adds one glyph's pixels; horizontal runs merged so strokes have no seams. */
void addGlyph (juce::Path& p, const Glyph& gl, float x, float y, float u, int skipRow = -1, float rowShift = 0.0f)
{
    for (int r = 0; r < 7; ++r)
    {
        const char* row = gl.rows[r];
        int c = 0;
        while (c < gl.w)
        {
            if (row[c] != '#') { ++c; continue; }
            int e = c;
            while (e < gl.w && row[e] == '#') ++e;
            if (r != skipRow)
                p.addRectangle (x + (float) c * u + (r == skipRow + 1 ? rowShift : 0.0f), y + (float) r * u, (float) (e - c) * u, u * 1.001f);
            c = e;
        }
    }
}
} // namespace

juce::Path BlockFont::textPath (const juce::String& text, float cap, float tracking)
{
    juce::Path p;
    const float u = cap / 7.0f;
    float x = 0.0f;
    for (auto ch : text)
    {
        if (ch == ' ') { x += 3.0f * u; continue; }
        if (auto* gl = glyphFor (ch))
        {
            addGlyph (p, *gl, x, 0.0f, u);
            x += ((float) gl->w + tracking) * u;
        }
    }
    return p;
}

float BlockFont::width (const juce::String& text, float cap, float tracking)
{
    const float u = cap / 7.0f;
    float x = 0.0f;
    for (auto ch : text)
    {
        if (ch == ' ') { x += 3.0f * u; continue; }
        if (auto* gl = glyphFor (ch)) x += ((float) gl->w + tracking) * u;
    }
    return std::max (0.0f, x - tracking * u);
}

float BlockFont::draw (juce::Graphics& g, const juce::String& text, float x, float y, float cap, juce::Colour c, float tracking)
{
    auto p = textPath (text, cap, tracking);
    g.setColour (c);
    g.fillPath (p, juce::AffineTransform::translation (x, y));
    return width (text, cap, tracking);
}

void BlockFont::drawCentred (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float cap, juce::Colour c, float tracking)
{
    const float w = width (text, cap, tracking);
    draw (g, text, area.getCentreX() - w * 0.5f, area.getCentreY() - cap * 0.5f, cap, c, tracking);
}

//==============================================================================
// Static Ruin: stacked-bar mark (one bar in signal red) + STATIC RUIN with a red dashed fault line.
float staticRuinLogoWidth (float cap)
{
    const float u = cap / 7.0f;
    return 6.0f * u + 2.5f * u + BlockFont::width ("STATIC RUIN", cap);
}

float drawStaticRuinLogo (juce::Graphics& g, float x, float y, float cap, bool dim)
{
    const float u = cap / 7.0f;
    const auto ink = dim ? palette::textDim : palette::text;
    const auto red = dim ? palette::brand.withMultipliedSaturation (0.7f).withMultipliedBrightness (0.8f) : palette::brand;
    // mark: four bars, the second one red and offset
    g.setColour (ink);
    g.fillRect (x, y, 6.0f * u, u);
    g.fillRect (x, y + 4.0f * u, 6.0f * u, u);
    g.fillRect (x, y + 6.0f * u, 2.5f * u, u);
    g.fillRect (x + 3.5f * u, y + 6.0f * u, 2.5f * u, u);
    g.setColour (red);
    g.fillRect (x + 1.0f * u, y + 2.0f * u, 5.0f * u, u);

    float tx = x + 8.5f * u;
    tx += BlockFont::draw (g, "STATIC", tx, y, cap, ink);
    tx += 3.0f * u + u;
    const float ruinX = tx;
    const float rw = BlockFont::draw (g, "RUIN", tx, y, cap, ink);
    // fault line: solid lead-in, then broken dashes across RUIN
    g.setColour (red);
    const float ly = y + 3.0f * u, lh = u * 0.9f;
    g.fillRect (ruinX - 0.5f * u, ly, rw * 0.32f, lh);
    for (float dx = rw * 0.38f; dx < rw + 0.5f * u; dx += 2.2f * u)
        g.fillRect (ruinX + dx, ly + ((int) (dx / u) % 3 == 0 ? 0.0f : 0.0f), 1.2f * u, lh);
    return tx + rw - x;
}

//==============================================================================
// Harsh Vox: "HARSH" in raw crimson, torn by horizontal slice displacement and static pixel damage;
// "VOX" clean off-white. Deterministic (no randomness at draw time beyond a fixed hash).
float harshVoxWordmarkWidth (float cap)
{
    return BlockFont::width ("HARSH", cap) + cap * 3.0f / 7.0f + BlockFont::width ("VOX", cap);
}

float drawHarshVoxWordmark (juce::Graphics& g, float x, float y, float cap)
{
    const float u = cap / 7.0f;
    float tx = x;
    juce::Path harsh;
    const juce::String word ("HARSH");
    // slice displacement: rows 2 and 5 of selected letters shift sideways
    const float shifts[5] = { 0.0f, 0.6f, -0.5f, 0.0f, 0.7f };
    for (int i = 0; i < word.length(); ++i)
    {
        auto* gl = glyphFor (word[i]);
        if (gl == nullptr) continue;
        for (int r = 0; r < 7; ++r)
        {
            const bool torn = (r == 2 && (i % 2 == 1)) || (r == 5 && i % 3 == 0);
            const float sx = torn ? shifts[i] * u : 0.0f;
            const char* row = gl->rows[r];
            int c = 0;
            while (c < gl->w)
            {
                if (row[c] != '#') { ++c; continue; }
                int e = c;
                while (e < gl->w && row[e] == '#') ++e;
                harsh.addRectangle (tx + (float) c * u + sx, y + (float) r * u, (float) (e - c) * u, u * 1.001f);
                c = e;
            }
        }
        tx += ((float) gl->w + 1.0f) * u;
    }
    g.setColour (palette::accent);
    g.fillPath (harsh);

    // static damage: punched-out holes inside strokes and stray pixels drifting off to the right
    uint32_t h = 0x9e3779b9u;
    auto rnd = [&h] { h ^= h << 13; h ^= h >> 17; h ^= h << 5; return (float) (h & 0xffff) / 65535.0f; };
    const float hw = tx - x - u;
    g.setColour (palette::bg1);
    for (int k = 0; k < 9; ++k)
    {
        const float px = x + rnd() * hw, py = y + std::floor (rnd() * 7.0f) * u;
        const float s = u * (0.35f + 0.3f * rnd());
        g.fillRect (px, py + (u - s) * 0.5f, s, s);
    }
    g.setColour (palette::accent.withAlpha (0.85f));
    for (int k = 0; k < 7; ++k)
    {
        const float px = x + hw * (0.55f + 0.5f * rnd()), py = y + cap + u * (0.6f + 1.4f * rnd());
        const float s = u * (0.35f + 0.35f * rnd());
        g.fillRect (px, py, s, s);
    }

    tx += 2.0f * u;
    const float vw = BlockFont::draw (g, "VOX", tx, y, cap, palette::text);
    return tx + vw - x;
}
} // namespace hv::ui
