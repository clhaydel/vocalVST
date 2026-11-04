// Procedural block lettering in the Static Ruin house style (5x7 square-stroke glyphs).
#pragma once
#include <juce_graphics/juce_graphics.h>

namespace hv::ui
{
struct BlockFont
{
    /** Path for `text` with cap height `cap` (px), origin at top-left. Tracking in glyph units. */
    static juce::Path textPath (const juce::String& text, float cap, float tracking = 1.0f);
    static float width (const juce::String& text, float cap, float tracking = 1.0f);
    /** Draw text; returns the drawn width. */
    static float draw (juce::Graphics& g, const juce::String& text, float x, float y, float cap,
                       juce::Colour c, float tracking = 1.0f);
    static void drawCentred (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float cap,
                             juce::Colour c, float tracking = 1.0f);
};

/** Static Ruin mark + wordmark. `cap` = letter height. Returns width. */
float drawStaticRuinLogo (juce::Graphics& g, float x, float y, float cap, bool dim = false);
float staticRuinLogoWidth (float cap);
/** Harsh Vox wordmark: torn, static-damaged "HARSH" + clean "VOX". Returns width. */
float drawHarshVoxWordmark (juce::Graphics& g, float x, float y, float cap);
float harshVoxWordmarkWidth (float cap);
} // namespace hv::ui
