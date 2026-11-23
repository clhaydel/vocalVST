#include "MilDraw.h"
#include "Layout.h"

namespace hv::ui::mil
{
namespace
{
using juce::Colour;
using juce::ColourGradient;
using juce::Path;
using juce::PathStrokeType;
constexpr float kPi = juce::MathConstants<float>::pi;

Colour C (float r, float g, float b, float a = 1.0f) { return Colour::fromFloatRGBA (r, g, b, a); }
Colour paintTone (float d) { return C (0.60f + d, 0.585f + d, 0.46f + d * 0.8f); }
Colour paintDkTone (float d) { return C (0.47f + d, 0.455f + d, 0.35f + d * 0.8f); }

ColourGradient radial (Pt centre, float radius, std::initializer_list<std::pair<float, Colour>> stops)
{
    auto it = stops.begin();
    const auto first = *it;
    const auto last = *(stops.end() - 1);
    ColourGradient grad (first.second, centre.x, centre.y, last.second, centre.x + radius, centre.y, true);
    for (auto& s : stops) if (s.first > 0.0f && s.first < 1.0f) grad.addColour (s.first, s.second);
    return grad;
}
ColourGradient linear (Pt a, Pt b, std::initializer_list<std::pair<float, Colour>> stops)
{
    const auto first = *stops.begin();
    const auto last = *(stops.end() - 1);
    ColourGradient grad (first.second, a.x, a.y, last.second, b.x, b.y, false);
    for (auto& s : stops) if (s.first > 0.0f && s.first < 1.0f) grad.addColour (s.first, s.second);
    return grad;
}
void fillCircle (Graphics& g, Pt c, float r) { g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r); }
Path roundRect (Rc r, float rad) { Path p; p.addRoundedRectangle (r, rad); return p; }
} // namespace

void softShadow (Graphics& g, Rc r, float rad, int spread, float alpha, float dx, float dy)
{
    // stacked translucent layers, 2 px apart (alpha compensated) to halve the fill count
    const float a2 = 1.0f - (1.0f - alpha) * (1.0f - alpha);
    g.setColour (C (0.03f, 0.025f, 0.015f, a2));
    for (int i = spread; i > 0; i -= 2)
        g.fillRoundedRectangle (r.expanded ((float) i - 0.5f).translated (dx, dy), rad + (float) i);
}

void aoHalo (Graphics& g, Pt c, float r, float strength)
{
    const Pt o = c.translated (1.0f, 2.0f);
    const float R = r * 1.9f;
    g.setGradientFill (radial (o, R, { { 0.0f, C (0.05f, 0.04f, 0.02f, strength) }, { 0.447f, C (0.05f, 0.04f, 0.02f, strength) },
                                       { 1.0f, C (0.05f, 0.04f, 0.02f, 0.0f) } }));
    fillCircle (g, o, R);
}

void screw (Graphics& g, Pt c, float r, float ang)
{
    g.setColour (C (0, 0, 0, 0.35f));
    fillCircle (g, c.translated (0.8f, 1.0f), r + 0.6f);
    g.setGradientFill (radial (c.translated (-r * 0.4f, -r * 0.4f), r * 1.5f,
                               { { 0.0f, C (0.80f, 0.78f, 0.70f) }, { 0.55f, C (0.55f, 0.53f, 0.46f) }, { 1.0f, C (0.30f, 0.29f, 0.25f) } }));
    fillCircle (g, c, r);
    g.setColour (C (0.12f, 0.11f, 0.09f));
    Path slot;
    slot.addRectangle (-r * 0.85f, -r * 0.13f, r * 1.7f, r * 0.26f);
    g.fillPath (slot, juce::AffineTransform::rotation (ang).translated (c));
}

void knobScale (Graphics& g, Pt c, float r, bool nums)
{
    const float a0 = juce::degreesToRadians (135.0f), a1 = juce::degreesToRadians (405.0f);
    const float rs = r + 4.0f;
    g.setFont (stencilFont (7.0f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / 10.0f;
        const float l = i % 5 == 0 ? 5.0f : 3.0f;
        g.setColour (palette::ink.withAlpha (0.9f));
        g.drawLine (c.x + std::cos (a) * rs, c.y + std::sin (a) * rs, c.x + std::cos (a) * (rs + l), c.y + std::sin (a) * (rs + l), 1.1f);
        if (nums && i % 2 == 0 && r >= 13.0f)
        {
            const Pt p (c.x + std::cos (a) * (rs + 10.5f), c.y + std::sin (a) * (rs + 10.5f));
            g.drawText (juce::String (i), Rc (14.0f, 9.0f).withCentre (p), juce::Justification::centred, false);
        }
    }
}

void bakelite (Graphics& g, Pt c, float r, float value, int flutes, bool pointer)
{
    aoHalo (g, c, r * 1.05f);
    g.setColour (C (0, 0, 0, 0.45f));
    fillCircle (g, c.translated (2.0f, 3.0f), r * 1.02f);
    const int n = flutes > 0 ? flutes : std::max (10, (int) (r * 0.9f));
    Path body;
    const int steps = n * 8;
    for (int i = 0; i <= steps; ++i)
    {
        const float a = (float) i * 2.0f * kPi / (float) steps;
        const float rr = r * (1.0f - 0.075f * std::abs (std::sin (a * (float) n * 0.5f)));
        const Pt p (c.x + std::cos (a) * rr, c.y + std::sin (a) * rr);
        if (i == 0) body.startNewSubPath (p); else body.lineTo (p);
    }
    body.closeSubPath();
    g.setGradientFill (radial (c.translated (-r * 0.35f, -r * 0.45f), r * 1.5f,
                               { { 0.0f, C (0.30f, 0.29f, 0.27f) }, { 0.45f, C (0.11f, 0.105f, 0.10f) }, { 1.0f, C (0.03f, 0.03f, 0.03f) } }));
    g.fillPath (body);
    g.setColour (C (0, 0, 0, 0.6f));
    g.strokePath (body, PathStrokeType (0.6f));
    g.setGradientFill (linear (c.translated (-r, -r), c.translated (r, r), { { 0.0f, C (0.24f, 0.23f, 0.22f) }, { 1.0f, C (0.05f, 0.05f, 0.05f) } }));
    fillCircle (g, c, r * 0.66f);
    Path hl;
    hl.addCentredArc (c.x, c.y, r * 0.66f, r * 0.66f, 0.0f, juce::degreesToRadians (-160.0f + 90.0f), juce::degreesToRadians (-70.0f + 90.0f), true);
    g.setColour (C (1, 1, 1, 0.10f));
    g.strokePath (hl, PathStrokeType (0.8f));
    if (! pointer) return;
    const float a = juce::degreesToRadians (135.0f + 270.0f * juce::jlimit (0.0f, 1.0f, value));
    g.setColour (palette::ink);
    Path pt;
    pt.startNewSubPath (c.x + std::cos (a) * r * 0.25f, c.y + std::sin (a) * r * 0.25f);
    pt.lineTo (c.x + std::cos (a) * r * 0.92f, c.y + std::sin (a) * r * 0.92f);
    g.strokePath (pt, PathStrokeType (std::max (1.6f, r * 0.11f), PathStrokeType::curved, PathStrokeType::rounded));
}

void toggle (Graphics& g, Pt c, bool up, float s)
{
    aoHalo (g, c, 9.0f * s, 0.28f);
    g.setColour (C (0, 0, 0, 0.4f));
    fillCircle (g, c.translated (1.2f, 1.6f), 9.0f * s);
    Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = (float) i * kPi / 3.0f + kPi / 6.0f;
        const Pt p (c.x + std::cos (a) * 8.5f * s, c.y + std::sin (a) * 8.5f * s);
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setGradientFill (linear (c.translated (-9 * s, -9 * s), c.translated (9 * s, 9 * s), { { 0.0f, C (0.85f, 0.84f, 0.78f) }, { 1.0f, C (0.35f, 0.34f, 0.30f) } }));
    g.fillPath (hex);
    g.setColour (C (0.15f, 0.15f, 0.13f));
    fillCircle (g, c, 4.2f * s);
    const float d = up ? -1.0f : 1.0f, L = 19.0f * s;
    const Pt tip (c.x + 1.5f * s, c.y + d * L);
    auto bat = [&] (float ox, float oy)
    {
        Path p;
        p.startNewSubPath (c.x - 2.4f * s + ox, c.y + oy);
        p.lineTo (tip.x - 1.4f * s + ox, tip.y + oy);
        p.lineTo (tip.x + 1.4f * s + ox, tip.y + oy);
        p.lineTo (c.x + 2.4f * s + ox, c.y + oy);
        p.closeSubPath();
        return p;
    };
    g.setColour (C (0, 0, 0, 0.35f));
    g.fillPath (bat (3.0f, 3.0f));
    g.setGradientFill (linear ({ c.x - 3.0f * s, 0 }, { c.x + 3.0f * s, 0 },
                               { { 0.0f, C (0.55f, 0.55f, 0.52f) }, { 0.45f, C (0.97f, 0.97f, 0.94f) }, { 1.0f, C (0.38f, 0.38f, 0.35f) } }));
    g.fillPath (bat (0, 0));
    g.setGradientFill (radial (tip.translated (-1, -1), 3.4f * s, { { 0.0f, C (1, 1, 0.97f) }, { 1.0f, C (0.45f, 0.45f, 0.42f) } }));
    fillCircle (g, tip, 3.2f * s);
}

void jewel (Graphics& g, Pt c, float r, juce::Colour col, bool lit)
{
    aoHalo (g, c, r + 3.0f, 0.25f);
    g.setColour (C (0, 0, 0, 0.4f));
    fillCircle (g, c.translated (1.0f, 1.5f), r + 3.0f);
    g.setGradientFill (linear (c.translated (-r, -r), c.translated (r, r), { { 0.0f, C (0.88f, 0.87f, 0.82f) }, { 1.0f, C (0.32f, 0.31f, 0.28f) } }));
    fillCircle (g, c, r + 2.6f);
    if (lit)
    {
        g.setGradientFill (radial (c, r * 3.2f, { { 0.0f, col.withAlpha (0.35f) }, { 1.0f, col.withAlpha (0.0f) } }));
        fillCircle (g, c, r * 3.2f);
    }
    const float k = lit ? 1.0f : 0.35f;
    const auto hi = C (std::min (1.0f, col.getFloatRed() * k + 0.45f), std::min (1.0f, col.getFloatGreen() * k + 0.35f), std::min (1.0f, col.getFloatBlue() * k + 0.3f));
    const auto lo = C (col.getFloatRed() * k * 0.45f, col.getFloatGreen() * k * 0.45f, col.getFloatBlue() * k * 0.45f);
    g.setGradientFill (radial (c.translated (-r * 0.3f, -r * 0.35f), r * 1.3f, { { 0.0f, hi }, { 1.0f, lo } }));
    fillCircle (g, c, r);
    g.setColour (C (1, 1, 1, 0.18f));
    for (int i = 0; i < 6; ++i)
    {
        const float a = (float) i * kPi / 3.0f;
        g.drawLine (c.x, c.y, c.x + std::cos (a) * r, c.y + std::sin (a) * r, 0.5f);
    }
    g.setColour (C (1, 1, 1, 0.75f));
    fillCircle (g, c.translated (-r * 0.35f, -r * 0.38f), r * 0.22f);
}

void pushButton (Graphics& g, Pt c, float r, juce::Colour cap, bool down)
{
    aoHalo (g, c, r + 4.0f, 0.3f);
    g.setColour (C (0, 0, 0, 0.4f));
    fillCircle (g, c.translated (1.5f, 2.0f), r + 4.0f);
    g.setGradientFill (linear (c.translated (-r, -r), c.translated (r, r), { { 0.0f, C (0.9f, 0.89f, 0.84f) }, { 1.0f, C (0.3f, 0.29f, 0.26f) } }));
    fillCircle (g, c, r + 3.5f);
    g.setColour (C (0.1f, 0.1f, 0.09f));
    fillCircle (g, c, r + 1.0f);
    const float k = down ? 0.75f : 1.0f;
    const auto hi = C (std::min (1.0f, cap.getFloatRed() * k + 0.3f), std::min (1.0f, cap.getFloatGreen() * k + 0.25f), std::min (1.0f, cap.getFloatBlue() * k + 0.2f));
    g.setGradientFill (radial (c.translated (-r * 0.3f + (down ? 0.6f : 0.0f), -r * 0.3f + (down ? 0.8f : 0.0f)), r * 1.3f,
                               { { 0.0f, hi }, { 1.0f, cap.darker (down ? 1.2f : 0.8f) } }));
    fillCircle (g, c.translated (down ? 0.4f : 0.0f, down ? 0.6f : 0.0f), r * (down ? 0.94f : 1.0f));
}

void thumbscrew (Graphics& g, Pt c, float r)
{
    aoHalo (g, c, r + 1.0f, 0.35f);
    g.setColour (C (0, 0, 0, 0.45f));
    fillCircle (g, c.translated (1.6f, 2.2f), r + 0.6f);
    g.setGradientFill (linear (c.translated (-r, -r), c.translated (r, r), { { 0.0f, C (0.92f, 0.92f, 0.88f) }, { 0.5f, C (0.62f, 0.62f, 0.58f) }, { 1.0f, C (0.28f, 0.28f, 0.26f) } }));
    fillCircle (g, c, r);
    g.setColour (C (0.15f, 0.15f, 0.14f, 0.55f));
    for (int i = 0; i < 28; ++i)
    {
        const float a = (float) i * 2.0f * kPi / 28.0f;
        g.drawLine (c.x + std::cos (a) * r * 0.78f, c.y + std::sin (a) * r * 0.78f, c.x + std::cos (a) * r, c.y + std::sin (a) * r, 0.5f);
    }
    g.setGradientFill (radial (c.translated (-r * 0.3f, -r * 0.3f), r, { { 0.0f, C (0.98f, 0.98f, 0.95f) }, { 1.0f, C (0.50f, 0.50f, 0.47f) } }));
    fillCircle (g, c, r * 0.66f);
    Path slot;
    slot.addRectangle (-r * 0.5f, -r * 0.09f, r, r * 0.18f);
    g.setColour (C (0.15f, 0.15f, 0.13f));
    g.fillPath (slot, juce::AffineTransform::rotation (0.6f).translated (c));
}

void pullHandle (Graphics& g, float x0, float x1, float y)
{
    for (float px : { x0, x1 })
    {
        aoHalo (g, { px, y }, 3.5f, 0.35f);
        g.setGradientFill (radial ({ px - 1.0f, y - 1.0f }, 3.2f, { { 0.0f, C (0.95f, 0.95f, 0.9f) }, { 1.0f, C (0.35f, 0.35f, 0.32f) } }));
        fillCircle (g, { px, y }, 3.0f);
    }
    Path sh;
    sh.startNewSubPath (x0 + 2.0f, y + 4.0f);
    sh.lineTo (x1 + 2.0f, y + 4.0f);
    g.setColour (C (0, 0, 0, 0.4f));
    g.strokePath (sh, PathStrokeType (3.6f, PathStrokeType::curved, PathStrokeType::rounded));
    Path bar;
    bar.startNewSubPath (x0, y - 1.0f);
    bar.lineTo (x1, y - 1.0f);
    g.setGradientFill (linear ({ 0, y - 3.5f }, { 0, y + 1.5f }, { { 0.0f, C (0.98f, 0.98f, 0.95f) }, { 0.5f, C (0.70f, 0.70f, 0.66f) }, { 1.0f, C (0.30f, 0.30f, 0.28f) } }));
    g.strokePath (bar, PathStrokeType (3.4f, PathStrokeType::curved, PathStrokeType::rounded));
}

void stencil (Graphics& g, const juce::String& s, float x, float y, float size, juce::Justification j, juce::Colour col, float alpha, float tracking)
{
    auto f = stencilFont (size).withExtraKerningFactor (tracking);
    g.setFont (f);
    g.setColour (col.withMultipliedAlpha (alpha));
    const float w = juce::GlyphArrangement::getStringWidth (f, s) + 4.0f;
    Rc r (0, y - size, w, size * 2.0f);
    if (j.testFlags (juce::Justification::horizontallyCentred)) r.setX (x - w * 0.5f);
    else if (j.testFlags (juce::Justification::right)) r.setX (x - w + 2.0f);
    else r.setX (x - 2.0f);
    g.drawText (s, r, juce::Justification::centred, false);
}

void tag (Graphics& g, Pt c, const juce::String& s, float size)
{
    auto f = tagFont (size);
    const float w = juce::GlyphArrangement::getStringWidth (f, s) + 8.0f, h = size + 4.4f;
    const Rc r (c.x - w * 0.5f, c.y - h * 0.5f, w, h);
    g.setColour (C (0, 0, 0, 0.4f));
    g.fillRoundedRectangle (r.translated (0.6f, 0.8f), 1.5f);
    g.setColour (C (0.09f, 0.09f, 0.08f));
    g.fillRoundedRectangle (r, 1.5f);
    g.setFont (f);
    g.setColour (palette::tagText);
    g.drawText (s, r.translated (0.0f, -0.2f), juce::Justification::centred, false);
}

void sectionTag (Graphics& g, float x, float y, const juce::String& title)
{
    auto f = stencilFont (8.0f).withExtraKerningFactor (0.08f);
    const float tw = juce::GlyphArrangement::getStringWidth (f, title) + 16.0f;
    const Rc r (x + 18.0f, y - 7.0f, tw, 14.0f);
    softShadow (g, r, 2.0f, 3, 0.08f, 1.0f, 1.5f);
    g.setColour (C (0.08f, 0.08f, 0.075f));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (C (0.75f, 0.73f, 0.66f, 0.6f));
    g.drawRoundedRectangle (r.reduced (2.0f), 1.5f, 0.6f);
    g.setFont (f);
    g.setColour (palette::tagText);
    g.drawText (title, r, juce::Justification::centred, false);
}

void raisedPlate (Graphics& g, Rc r)
{
    softShadow (g, r, 5.0f);
    g.setGradientFill (linear (r.getTopLeft(), { r.getX() + r.getWidth() * 0.25f, r.getBottom() }, { { 0.0f, paintTone (0.05f) }, { 1.0f, paintTone (-0.03f) } }));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (C (1, 1, 0.92f, 0.28f));
    g.drawLine (r.getX() + 5, r.getY() + 1, r.getRight() - 5, r.getY() + 1, 1.6f);
    g.drawLine (r.getX() + 1, r.getY() + 5, r.getX() + 1, r.getBottom() - 5, 1.6f);
    g.setColour (C (0.05f, 0.04f, 0.02f, 0.45f));
    g.drawLine (r.getX() + 5, r.getBottom() - 1, r.getRight() - 5, r.getBottom() - 1, 1.6f);
    g.drawLine (r.getRight() - 1, r.getY() + 5, r.getRight() - 1, r.getBottom() - 5, 1.6f);
}

void recess (Graphics& g, Rc r)
{
    g.setGradientFill (linear (r.getTopLeft(), r.getBottomLeft(), { { 0.0f, paintDkTone (-0.09f) }, { 0.12f, paintDkTone (-0.03f) }, { 1.0f, paintTone (-0.02f) } }));
    g.fillRoundedRectangle (r, 4.0f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (roundRect (r, 4.0f));
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (C (0.03f, 0.02f, 0.01f, 0.07f));
            g.fillRect (r.getX(), r.getY() + (float) i, r.getWidth(), 1.0f);
            g.fillRect (r.getX() + (float) i, r.getY(), 1.0f, r.getHeight());
        }
    }
    g.setColour (C (1, 1, 0.92f, 0.22f));
    g.drawLine (r.getX() + 3, r.getBottom() + 0.6f, r.getRight() - 3, r.getBottom() + 0.6f, 1.2f);
    g.drawLine (r.getRight() + 0.6f, r.getY() + 3, r.getRight() + 0.6f, r.getBottom() - 3, 1.2f);
}

void moduleBody (Graphics& g, Rc f)
{
    // shadow cast down-right into the well
    softShadow (g, f.withSizeKeepingCentre (f.getWidth() + 4.0f, f.getHeight() + 4.0f), 5.0f, 10, 0.055f, 4.0f, 6.0f);
    const float T = 3.0f, B = 4.5f;
    Path bottom, right;
    bottom.startNewSubPath (f.getX() + 3, f.getBottom());
    bottom.lineTo (f.getRight(), f.getBottom());
    bottom.lineTo (f.getRight() + T, f.getBottom() + B);
    bottom.lineTo (f.getX() + 3 + T, f.getBottom() + B);
    bottom.closeSubPath();
    g.setGradientFill (linear ({ 0, f.getBottom() }, { 0, f.getBottom() + B }, { { 0.0f, paintDkTone (-0.10f) }, { 1.0f, paintDkTone (-0.20f) } }));
    g.fillPath (bottom);
    right.startNewSubPath (f.getRight(), f.getY() + 3);
    right.lineTo (f.getRight() + T, f.getY() + 3 + B);
    right.lineTo (f.getRight() + T, f.getBottom() + B);
    right.lineTo (f.getRight(), f.getBottom());
    right.closeSubPath();
    g.setGradientFill (linear ({ f.getRight(), 0 }, { f.getRight() + T, 0 }, { { 0.0f, paintDkTone (-0.06f) }, { 1.0f, paintDkTone (-0.14f) } }));
    g.fillPath (right);
    g.setGradientFill (linear (f.getTopLeft(), { f.getX() + f.getWidth() * 0.3f, f.getBottom() }, { { 0.0f, paintTone (0.07f) }, { 1.0f, paintTone (-0.01f) } }));
    g.fillRoundedRectangle (f, 3.0f);
    const float c = 3.2f;
    auto quad = [&g] (std::initializer_list<Pt> pts, juce::Colour col)
    {
        Path p;
        bool first = true;
        for (auto q : pts) { if (first) p.startNewSubPath (q); else p.lineTo (q); first = false; }
        p.closeSubPath();
        g.setColour (col);
        g.fillPath (p);
    };
    const float x0 = f.getX(), y0 = f.getY(), x1 = f.getRight(), y1 = f.getBottom();
    quad ({ { x0, y0 }, { x1, y0 }, { x1 - c, y0 + c }, { x0 + c, y0 + c } }, C (1, 1, 0.93f, 0.34f));
    quad ({ { x0, y0 }, { x0 + c, y0 + c }, { x0 + c, y1 - c }, { x0, y1 } }, C (1, 1, 0.93f, 0.20f));
    quad ({ { x0, y1 }, { x0 + c, y1 - c }, { x1 - c, y1 - c }, { x1, y1 } }, C (0.05f, 0.04f, 0.02f, 0.40f));
    quad ({ { x1, y0 }, { x1, y1 }, { x1 - c, y1 - c }, { x1 - c, y0 + c } }, C (0.05f, 0.04f, 0.02f, 0.28f));
    g.setColour (C (1, 1, 0.95f, 0.55f));
    g.drawLine (x0 + 2, y0 + 0.5f, x1 - 2, y0 + 0.5f, 0.8f);
}

void blankingPlate (Graphics& g, Rc p)
{
    softShadow (g, p, 3.0f, 5, 0.07f);
    g.setGradientFill (linear (p.getTopLeft(), { p.getX() + p.getWidth() * 0.3f, p.getBottom() }, { { 0.0f, paintTone (0.03f) }, { 1.0f, paintDkTone (-0.01f) } }));
    g.fillRoundedRectangle (p, 3.0f);
    g.setColour (C (1, 1, 0.92f, 0.25f));
    g.drawLine (p.getX() + 3, p.getY() + 1, p.getRight() - 3, p.getY() + 1, 1.2f);
    g.setColour (C (0.05f, 0.04f, 0.02f, 0.4f));
    g.drawLine (p.getX() + 3, p.getBottom() - 0.5f, p.getRight() - 3, p.getBottom() - 0.5f, 1.2f);
}

void bigDial (Graphics& g, Pt c, float r, float turn, const juce::StringArray& labels, juce::Colour index)
{
    aoHalo (g, c, r * 1.02f, 0.35f);
    g.setColour (C (0, 0, 0, 0.45f));
    fillCircle (g, c.translated (3.0f, 4.0f), r);
    g.setGradientFill (radial (c.translated (-r * 0.3f, -r * 0.3f), r * 1.3f, { { 0.0f, C (0.20f, 0.17f, 0.14f) }, { 1.0f, C (0.06f, 0.05f, 0.045f) } }));
    fillCircle (g, c, r);
    g.setColour (C (0.25f, 0.22f, 0.19f, 0.9f));
    for (int i = 0; i < 120; ++i)
    {
        const float a = (float) i * 2.0f * kPi / 120.0f;
        g.drawLine (c.x + std::cos (a) * (r - 3), c.y + std::sin (a) * (r - 3), c.x + std::cos (a) * r, c.y + std::sin (a) * r, 0.9f);
    }
    const float rot = -turn * 2.0f * kPi;
    const int n = labels.size();
    const auto eng = C (0.92f, 0.88f, 0.78f, 0.9f);
    for (int i = 0; i < n * 4; ++i)
    {
        const float a = rot + (float) i * 2.0f * kPi / (float) (n * 4) - kPi * 0.5f;
        const bool major = i % 4 == 0;
        g.setColour (eng);
        const float l = major ? 11.0f : 8.0f;
        g.drawLine (c.x + std::cos (a) * (r - 5), c.y + std::sin (a) * (r - 5), c.x + std::cos (a) * (r - l), c.y + std::sin (a) * (r - l), major ? 1.0f : 0.6f);
        if (major)
        {
            juce::Graphics::ScopedSaveState ss (g);
            const Pt p (c.x + std::cos (a) * (r - 18.0f), c.y + std::sin (a) * (r - 18.0f));
            g.addTransform (juce::AffineTransform::rotation (a + kPi * 0.5f, p.x, p.y));
            g.setFont (tagFont (6.8f));
            g.drawText (labels[i / 4], Rc (30.0f, 9.0f).withCentre (p), juce::Justification::centred, false);
        }
    }
    bakelite (g, c, r * 0.48f, 0.0f, 8, false);
    Path tri;
    tri.addTriangle (c.x, c.y - r - 2.0f, c.x - 5.0f, c.y - r - 10.0f, c.x + 5.0f, c.y - r - 10.0f);
    g.setColour (index);
    g.fillPath (tri);
}

void vuMeter (Graphics& g, Rc r, float vu, const juce::String& title)
{
    softShadow (g, r, 7.0f, 10, 0.06f, 3.0f, 4.0f);
    g.setColour (C (0, 0, 0, 0.45f));
    g.fillRoundedRectangle (r.translated (3.0f, 4.0f), 7.0f);
    g.setGradientFill (linear (r.getTopLeft(), r.getBottomRight(), { { 0.0f, C (0.20f, 0.19f, 0.18f) }, { 1.0f, C (0.05f, 0.05f, 0.05f) } }));
    g.fillRoundedRectangle (r, 7.0f);
    for (auto p : { r.getTopLeft().translated (7, 7), r.getTopRight().translated (-7, 7), r.getBottomLeft().translated (7, -7), r.getBottomRight().translated (-7, -7) })
        screw (g, p, 2.8f, 0.7f);
    const Rc f (r.getX() + 13.0f, r.getY() + 12.0f, r.getWidth() - 26.0f, r.getHeight() - 26.0f);
    g.setGradientFill (linear (f.getTopLeft(), f.getBottomLeft(), { { 0.0f, C (0.95f, 0.91f, 0.78f) }, { 1.0f, C (0.84f, 0.79f, 0.64f) } }));
    g.fillRoundedRectangle (f, 3.0f);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (roundRect (f, 3.0f));
    const Pt pv (f.getCentreX(), f.getBottom() + 14.0f);
    const float R = f.getHeight() * 0.86f;
    auto ang = [] (float db) { const float t = std::pow (juce::jlimit (0.0f, 1.0f, (db + 20.0f) / 23.0f), 0.6f); return juce::degreesToRadians (-142.0f + 104.0f * t); };
    auto onArc = [&] (float a, float rad) { return Pt (pv.x + std::cos (a) * rad, pv.y + std::sin (a) * rad); };
    Path redArc;
    for (int i = 0; i <= 12; ++i) { const auto p = onArc (ang (3.0f * (float) i / 12.0f), R); if (i == 0) redArc.startNewSubPath (p); else redArc.lineTo (p); }
    g.setColour (palette::red);
    g.strokePath (redArc, PathStrokeType (2.5f));
    Path blackArc;
    for (int i = 0; i <= 24; ++i) { const auto p = onArc (ang (-20.0f + 20.0f * (float) i / 24.0f), R - 1.8f); if (i == 0) blackArc.startNewSubPath (p); else blackArc.lineTo (p); }
    g.setColour (palette::black);
    g.strokePath (blackArc, PathStrokeType (0.8f));
    const struct { float db; const char* t; } marks[] = { { -20, "20" }, { -10, "10" }, { -7, "7" }, { -5, "5" }, { -3, "3" }, { -1, "1" }, { 0, "0" }, { 1, "1" }, { 2, "2" }, { 3, "3" } };
    g.setFont (tagFont (6.4f));
    for (auto& m : marks)
    {
        const float a = ang (m.db);
        const auto col = m.db > 0 ? palette::red : palette::black;
        g.setColour (col);
        g.drawLine ({ onArc (a, R - 2.0f), onArc (a, R + 4.0f) }, 1.0f);
        if (m.db == -20.0f || m.db >= -7.0f) g.drawText (m.t, Rc (12.0f, 8.0f).withCentre (onArc (a, R + 9.5f)), juce::Justification::centred, false);
    }
    g.setFont (stencilFont (10.0f));
    g.setColour (palette::black);
    g.drawText ("VU", Rc (30, 12).withCentre ({ pv.x, f.getBottom() - 16.0f }), juce::Justification::centred, false);
    g.setFont (stencilFont (7.0f).withExtraKerningFactor (0.06f));
    g.drawText (title, Rc (f.getX() + 5.0f, f.getY() + 3.0f, 60.0f, 10.0f), juce::Justification::centredLeft, false);
    const float a = ang (vu);
    g.setColour (C (0, 0, 0, 0.25f));
    g.drawLine ({ pv.translated (2, 0), onArc (a, R + 3.0f).translated (2, 1) }, 1.1f);
    g.setColour (palette::black);
    g.drawLine ({ pv, onArc (a, R + 3.0f) }, 1.1f);
    g.setColour (C (0.1f, 0.1f, 0.1f));
    g.fillEllipse (pv.x - 6.0f, f.getBottom() - 6.0f, 12.0f, 12.0f);
    g.setGradientFill (linear (f.getTopLeft(), f.getBottomRight(), { { 0.0f, C (1, 1, 1, 0.28f) }, { 0.45f, C (1, 1, 1, 0.04f) }, { 1.0f, C (1, 1, 1, 0.0f) } }));
    g.fillRect (f);
}

void crtBezel (Graphics& g, Rc r)
{
    softShadow (g, r, 10.0f, 12, 0.06f, 3.0f, 5.0f);
    g.setColour (C (0, 0, 0, 0.45f));
    g.fillRoundedRectangle (r.translated (3.0f, 4.0f), 10.0f);
    g.setGradientFill (linear (r.getTopLeft(), r.getBottomLeft(), { { 0.0f, C (0.17f, 0.165f, 0.15f) }, { 1.0f, C (0.06f, 0.06f, 0.055f) } }));
    g.fillRoundedRectangle (r, 10.0f);
    for (auto p : { r.getTopLeft().translated (8, 8), r.getTopRight().translated (-8, 8), r.getBottomLeft().translated (8, -8), r.getBottomRight().translated (-8, -8) })
        screw (g, p, 3.0f, 0.4f);
    const auto gl = layout::glass (r);
    g.setColour (C (0.01f, 0.03f, 0.015f));
    g.fillRoundedRectangle (gl, 14.0f);
    g.setColour (C (0, 0, 0, 0.8f));
    g.drawRoundedRectangle (gl, 14.0f, 2.0f);
}

void case_ (Graphics& g)
{
    using namespace layout;
    g.fillAll (C (0.13f, 0.13f, 0.12f));
    const Rc outer (2.0f, 1.0f, kW - 4.0f, kH - 2.0f);
    g.setGradientFill (linear ({ 0, 0 }, { 0, kH }, { { 0.0f, C (0.27f, 0.27f, 0.22f) }, { 1.0f, C (0.15f, 0.15f, 0.12f) } }));
    g.fillRoundedRectangle (outer, 9.0f);
    g.setGradientFill (linear (panel.getTopLeft(), { panel.getX() + panel.getWidth() * 0.3f, panel.getBottom() }, { { 0.0f, paintTone (0.04f) }, { 1.0f, paintDkTone (0.0f) } }));
    g.fillRoundedRectangle (panel, 6.0f);
    g.setColour (C (1, 1, 1, 0.10f));
    g.drawRoundedRectangle (panel.reduced (0.5f), 6.0f, 1.0f);
}
} // namespace hv::ui::mil
