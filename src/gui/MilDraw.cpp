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
Colour paintTone (float d) { return C (0.305f + d, 0.345f + d, 0.405f + d); }
Colour paintDkTone (float d) { return C (0.235f + d, 0.27f + d, 0.33f + d); }

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
Pt polar (Pt c, float a, float r) { return { c.x + std::cos (a) * r, c.y + std::sin (a) * r }; }
} // namespace

void softShadow (Graphics& g, Rc r, float rad, int spread, float alpha, float dx, float dy)
{
    // stacked translucent layers, 2 px apart (alpha compensated) to halve the fill count
    const float a2 = 1.0f - (1.0f - alpha) * (1.0f - alpha);
    g.setColour (C (0.01f, 0.015f, 0.025f, a2));
    for (int i = spread; i > 0; i -= 2)
        g.fillRoundedRectangle (r.expanded ((float) i - 0.5f).translated (dx, dy), rad + (float) i);
}

void aoHalo (Graphics& g, Pt c, float r, float strength)
{
    const Pt o = c.translated (1.0f, 2.0f);
    const float R = r * 1.9f;
    g.setGradientFill (radial (o, R, { { 0.0f, C (0.01f, 0.02f, 0.03f, strength) }, { 0.447f, C (0.01f, 0.02f, 0.03f, strength) },
                                       { 1.0f, C (0.01f, 0.02f, 0.03f, 0.0f) } }));
    fillCircle (g, o, R);
}

void screw (Graphics& g, Pt c, float r, float ang)
{
    g.setColour (C (0, 0, 0, 0.45f));
    fillCircle (g, c.translated (0.8f, 1.1f), r + 0.7f);
    g.setGradientFill (radial (c.translated (-r * 0.4f, -r * 0.45f), r * 1.5f,
                               { { 0.0f, C (0.88f, 0.89f, 0.90f) }, { 0.55f, C (0.55f, 0.56f, 0.58f) }, { 1.0f, C (0.24f, 0.25f, 0.27f) } }));
    fillCircle (g, c, r);
    g.setColour (C (0.10f, 0.10f, 0.11f));
    Path slot;
    slot.addRectangle (-r * 0.85f, -r * 0.13f, r * 1.7f, r * 0.26f);
    g.fillPath (slot, juce::AffineTransform::rotation (ang).translated (c));
    g.setColour (C (1, 1, 1, 0.25f));
    Path lip;
    lip.addRectangle (-r * 0.8f, r * 0.13f, r * 1.6f, r * 0.09f);
    g.fillPath (lip, juce::AffineTransform::rotation (ang).translated (c));
}

void knobScale (Graphics& g, Pt c, float r, bool nums)
{
    const float a0 = juce::degreesToRadians (135.0f), a1 = juce::degreesToRadians (405.0f);
    const float rs = r + (r < 13.0f ? 5.0f : 7.0f);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / 10.0f;
        const auto p = polar (c, a, rs);
        g.setColour (C (0, 0, 0, 0.35f));
        fillCircle (g, p.translated (0.4f, 0.6f), 1.25f);
        g.setColour (palette::ink.withAlpha (0.92f));
        fillCircle (g, p, (i % 5 == 0 ? 1.35f : 1.05f) * (r < 13.0f ? 0.8f : 1.0f));
    }
    if (! nums) return;
    g.setColour (palette::ink.withAlpha (0.92f));
    g.drawLine (c.x, c.y - r - 4.5f, c.x, c.y - r - 10.0f, 1.3f);
    g.setFont (stencilFont (7.0f));
    g.setColour (palette::ink.withAlpha (0.85f));
    g.drawText ("0", Rc (14.0f, 9.0f).withCentre (polar (c, a0, rs + 6.0f).translated (-1.0f, 5.0f)), juce::Justification::centred, false);
    g.drawText ("10", Rc (14.0f, 9.0f).withCentre (polar (c, a1, rs + 6.0f).translated (1.0f, 5.0f)), juce::Justification::centred, false);
}

void creamKnob (Graphics& g, Pt c, float r, float value)
{
    aoHalo (g, c, r * 1.05f, 0.35f);
    g.setColour (C (0, 0, 0, 0.5f));
    fillCircle (g, c.translated (1.6f, 2.6f), r * 1.02f);
    // knurled skirt
    const int n = std::max (24, (int) (r * 2.4f));
    Path body;
    const int steps = n * 4;
    for (int i = 0; i <= steps; ++i)
    {
        const float a = (float) i * 2.0f * kPi / (float) steps;
        const float rr = r * (1.0f - 0.03f * (0.5f + 0.5f * std::cos (a * (float) n)));
        const Pt p = polar (c, a, rr);
        if (i == 0) body.startNewSubPath (p); else body.lineTo (p);
    }
    body.closeSubPath();
    g.setGradientFill (linear (c.translated (-r, -r), c.translated (r, r),
                               { { 0.0f, C (0.90f, 0.88f, 0.83f) }, { 0.5f, C (0.62f, 0.60f, 0.55f) }, { 1.0f, C (0.30f, 0.29f, 0.27f) } }));
    g.fillPath (body);
    g.setColour (C (0, 0, 0, 0.45f));
    g.strokePath (body, PathStrokeType (0.6f));
    // domed cap
    const float rc = r * 0.80f;
    g.setGradientFill (radial (c.translated (-rc * 0.35f, -rc * 0.45f), rc * 1.55f,
                               { { 0.0f, C (0.97f, 0.96f, 0.92f) }, { 0.45f, C (0.80f, 0.78f, 0.72f) }, { 1.0f, C (0.50f, 0.48f, 0.44f) } }));
    fillCircle (g, c, rc);
    g.setColour (C (0.25f, 0.23f, 0.20f, 0.45f));
    g.drawEllipse (c.x - rc, c.y - rc, 2 * rc, 2 * rc, 0.7f);
    g.setGradientFill (radial (c.translated (-rc * 0.3f, -rc * 0.5f), rc * 0.6f, { { 0.0f, C (1, 1, 1, 0.45f) }, { 1.0f, C (1, 1, 1, 0.0f) } }));
    fillCircle (g, c, rc);
    const float a = juce::degreesToRadians (135.0f + 270.0f * juce::jlimit (0.0f, 1.0f, value));
    g.setColour (C (0.08f, 0.08f, 0.08f, 0.9f));
    fillCircle (g, polar (c, a, rc * 0.66f), std::max (1.5f, r * 0.085f));
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
        const Pt p = polar (c, a, rr);
        if (i == 0) body.startNewSubPath (p); else body.lineTo (p);
    }
    body.closeSubPath();
    g.setGradientFill (radial (c.translated (-r * 0.35f, -r * 0.45f), r * 1.5f,
                               { { 0.0f, C (0.30f, 0.30f, 0.31f) }, { 0.45f, C (0.10f, 0.10f, 0.11f) }, { 1.0f, C (0.02f, 0.02f, 0.03f) } }));
    g.fillPath (body);
    g.setColour (C (0, 0, 0, 0.6f));
    g.strokePath (body, PathStrokeType (0.6f));
    g.setGradientFill (linear (c.translated (-r, -r), c.translated (r, r), { { 0.0f, C (0.24f, 0.24f, 0.25f) }, { 1.0f, C (0.04f, 0.04f, 0.05f) } }));
    fillCircle (g, c, r * 0.66f);
    if (! pointer) return;
    const float a = juce::degreesToRadians (135.0f + 270.0f * juce::jlimit (0.0f, 1.0f, value));
    g.setColour (palette::ink);
    Path pt;
    pt.startNewSubPath (polar (c, a, r * 0.25f));
    pt.lineTo (polar (c, a, r * 0.92f));
    g.strokePath (pt, PathStrokeType (std::max (1.6f, r * 0.11f), PathStrokeType::curved, PathStrokeType::rounded));
}

void toggle (Graphics& g, Pt c, bool up, float s)
{
    aoHalo (g, c, 9.0f * s, 0.28f);
    g.setColour (C (0, 0, 0, 0.45f));
    fillCircle (g, c.translated (1.2f, 1.8f), 10.0f * s);
    // chrome hex nut on a round washer
    g.setGradientFill (linear (c.translated (-10 * s, -10 * s), c.translated (10 * s, 10 * s), { { 0.0f, C (0.78f, 0.79f, 0.80f) }, { 1.0f, C (0.25f, 0.26f, 0.28f) } }));
    fillCircle (g, c, 10.5f * s);
    Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const Pt p = polar (c, (float) i * kPi / 3.0f + kPi / 6.0f, 8.5f * s);
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setGradientFill (linear (c.translated (-9 * s, -9 * s), c.translated (9 * s, 9 * s), { { 0.0f, C (0.92f, 0.93f, 0.94f) }, { 1.0f, C (0.36f, 0.37f, 0.39f) } }));
    g.fillPath (hex);
    g.setColour (C (0, 0, 0, 0.35f));
    g.strokePath (hex, PathStrokeType (0.6f));
    g.setColour (C (0.12f, 0.12f, 0.13f));
    fillCircle (g, c, 4.2f * s);
    const float d = up ? -1.0f : 1.0f, L = 19.0f * s;
    const Pt tip (c.x + 1.5f * s, c.y + d * L);
    auto bat = [&] (float ox, float oy)
    {
        Path p;
        p.startNewSubPath (c.x - 2.4f * s + ox, c.y + oy);
        p.lineTo (tip.x - 1.6f * s + ox, tip.y + oy);
        p.lineTo (tip.x + 1.6f * s + ox, tip.y + oy);
        p.lineTo (c.x + 2.4f * s + ox, c.y + oy);
        p.closeSubPath();
        return p;
    };
    g.setColour (C (0, 0, 0, 0.35f));
    g.fillPath (bat (3.0f, 3.0f));
    g.setGradientFill (linear ({ c.x - 3.0f * s, 0 }, { c.x + 3.0f * s, 0 },
                               { { 0.0f, C (0.50f, 0.51f, 0.53f) }, { 0.45f, C (0.97f, 0.97f, 0.98f) }, { 1.0f, C (0.34f, 0.35f, 0.37f) } }));
    g.fillPath (bat (0, 0));
    g.setGradientFill (radial (tip.translated (-1, -1), 3.6f * s, { { 0.0f, C (1, 1, 1) }, { 1.0f, C (0.42f, 0.43f, 0.45f) } }));
    fillCircle (g, tip, 3.3f * s);
}

void jewel (Graphics& g, Pt c, float r, juce::Colour col, bool lit)
{
    aoHalo (g, c, r + 3.0f, 0.3f);
    g.setColour (C (0, 0, 0, 0.45f));
    fillCircle (g, c.translated (1.0f, 1.6f), r + 3.0f);
    g.setGradientFill (linear (c.translated (-r, -r), c.translated (r, r), { { 0.0f, C (0.90f, 0.91f, 0.92f) }, { 1.0f, C (0.26f, 0.27f, 0.29f) } }));
    fillCircle (g, c, r + 2.8f);
    g.setColour (C (0.05f, 0.05f, 0.06f));
    fillCircle (g, c, r + 0.9f);
    if (lit)
    {
        g.setGradientFill (radial (c, r * 3.2f, { { 0.0f, col.withAlpha (0.38f) }, { 1.0f, col.withAlpha (0.0f) } }));
        fillCircle (g, c, r * 3.2f);
    }
    const float k = lit ? 1.0f : 0.38f;
    const auto hi = C (std::min (1.0f, col.getFloatRed() * k + 0.45f * k), std::min (1.0f, col.getFloatGreen() * k + 0.40f * k), std::min (1.0f, col.getFloatBlue() * k + 0.35f * k));
    const auto lo = C (col.getFloatRed() * k * 0.42f, col.getFloatGreen() * k * 0.42f, col.getFloatBlue() * k * 0.42f);
    g.setGradientFill (radial (c.translated (-r * 0.3f, -r * 0.35f), r * 1.3f, { { 0.0f, hi }, { 1.0f, lo } }));
    fillCircle (g, c, r);
    g.setColour (C (1, 1, 1, lit ? 0.8f : 0.4f));
    fillCircle (g, c.translated (-r * 0.35f, -r * 0.4f), r * 0.24f);
}

void pushButton (Graphics& g, Pt c, float r, juce::Colour cap, bool down)
{
    aoHalo (g, c, r + 4.0f, 0.32f);
    g.setColour (C (0, 0, 0, 0.45f));
    fillCircle (g, c.translated (1.5f, 2.2f), r + 4.0f);
    g.setGradientFill (linear (c.translated (-r, -r), c.translated (r, r), { { 0.0f, C (0.92f, 0.93f, 0.94f) }, { 1.0f, C (0.24f, 0.25f, 0.27f) } }));
    fillCircle (g, c, r + 3.5f);
    g.setColour (C (0.06f, 0.06f, 0.07f));
    fillCircle (g, c, r + 1.0f);
    const float k = down ? 0.75f : 1.0f;
    const auto hi = C (std::min (1.0f, cap.getFloatRed() * k + 0.3f), std::min (1.0f, cap.getFloatGreen() * k + 0.3f), std::min (1.0f, cap.getFloatBlue() * k + 0.3f));
    const Pt cc = c.translated (down ? 0.4f : 0.0f, down ? 0.6f : 0.0f);
    const float rr = r * (down ? 0.94f : 1.0f);
    g.setGradientFill (radial (cc.translated (-r * 0.3f, -r * 0.35f), r * 1.35f, { { 0.0f, hi }, { 1.0f, cap.darker (down ? 1.4f : 1.0f) } }));
    fillCircle (g, cc, rr);
    g.setGradientFill (radial (cc.translated (-r * 0.3f, -r * 0.45f), r * 0.55f, { { 0.0f, C (1, 1, 1, 0.55f) }, { 1.0f, C (1, 1, 1, 0.0f) } }));
    fillCircle (g, cc, rr);
}

void thumbscrew (Graphics& g, Pt c, float r)
{
    aoHalo (g, c, r + 1.0f, 0.35f);
    g.setColour (C (0, 0, 0, 0.45f));
    fillCircle (g, c.translated (1.6f, 2.2f), r + 0.6f);
    g.setGradientFill (linear (c.translated (-r, -r), c.translated (r, r), { { 0.0f, C (0.92f, 0.92f, 0.93f) }, { 0.5f, C (0.60f, 0.61f, 0.62f) }, { 1.0f, C (0.26f, 0.27f, 0.28f) } }));
    fillCircle (g, c, r);
    g.setColour (C (0.12f, 0.12f, 0.13f, 0.55f));
    for (int i = 0; i < 28; ++i)
    {
        const float a = (float) i * 2.0f * kPi / 28.0f;
        g.drawLine ({ polar (c, a, r * 0.78f), polar (c, a, r) }, 0.5f);
    }
    g.setGradientFill (radial (c.translated (-r * 0.3f, -r * 0.3f), r, { { 0.0f, C (0.98f, 0.98f, 0.99f) }, { 1.0f, C (0.48f, 0.49f, 0.50f) } }));
    fillCircle (g, c, r * 0.66f);
    Path slot;
    slot.addRectangle (-r * 0.5f, -r * 0.09f, r, r * 0.18f);
    g.setColour (C (0.12f, 0.12f, 0.13f));
    g.fillPath (slot, juce::AffineTransform::rotation (0.6f).translated (c));
}

void pullHandle (Graphics& g, float x0, float x1, float y)
{
    for (float px : { x0, x1 })
    {
        aoHalo (g, { px, y }, 3.5f, 0.35f);
        g.setGradientFill (radial ({ px - 1.0f, y - 1.0f }, 3.2f, { { 0.0f, C (0.95f, 0.95f, 0.96f) }, { 1.0f, C (0.33f, 0.34f, 0.35f) } }));
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
    g.setGradientFill (linear ({ 0, y - 3.5f }, { 0, y + 1.5f }, { { 0.0f, C (0.98f, 0.98f, 0.99f) }, { 0.5f, C (0.68f, 0.69f, 0.70f) }, { 1.0f, C (0.28f, 0.29f, 0.30f) } }));
    g.strokePath (bar, PathStrokeType (3.4f, PathStrokeType::curved, PathStrokeType::rounded));
}

void stencil (Graphics& g, const juce::String& s, float x, float y, float size, juce::Justification j, juce::Colour col, float alpha, float tracking)
{
    auto f = stencilFont (size).withExtraKerningFactor (tracking);
    g.setFont (f);
    const float w = juce::GlyphArrangement::getStringWidth (f, s) + 4.0f;
    Rc r (0, y - size, w, size * 2.0f);
    if (j.testFlags (juce::Justification::horizontallyCentred)) r.setX (x - w * 0.5f);
    else if (j.testFlags (juce::Justification::right)) r.setX (x - w + 2.0f);
    else r.setX (x - 2.0f);
    g.setColour (C (0, 0, 0, 0.35f * alpha * col.getFloatAlpha()));
    g.drawText (s, r.translated (0.5f, 0.7f), juce::Justification::centred, false);
    g.setColour (col.withMultipliedAlpha (alpha));
    g.drawText (s, r, juce::Justification::centred, false);
}

void tag (Graphics& g, Pt c, const juce::String& s, float size)
{
    auto f = tagFont (size);
    const float w = juce::GlyphArrangement::getStringWidth (f, s) + 10.0f, h = size + 5.6f;
    const Rc r (c.x - w * 0.5f, c.y - h * 0.5f, w, h);
    g.setColour (C (1, 1, 1, 0.10f));
    g.fillRoundedRectangle (r.translated (0.0f, 0.8f), 2.0f);
    g.setColour (C (0.035f, 0.04f, 0.05f));
    g.fillRoundedRectangle (r, 2.0f);
    g.setFont (f);
    g.setColour (palette::tagText);
    g.drawText (s, r.translated (0.0f, -0.2f), juce::Justification::centred, false);
}

void sectionTag (Graphics& g, float x, float y, const juce::String& title)
{
    auto f = stencilFont (9.0f).withExtraKerningFactor (0.06f);
    const float tw = juce::GlyphArrangement::getStringWidth (f, title) + 30.0f;
    const Rc r (x, y - 8.5f, tw, 17.0f);
    softShadow (g, r, 2.0f, 3, 0.10f, 0.8f, 1.2f);
    g.setGradientFill (linear (r.getTopLeft(), r.getBottomLeft(), { { 0.0f, C (0.12f, 0.13f, 0.15f) }, { 1.0f, C (0.04f, 0.045f, 0.055f) } }));
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (C (1, 1, 1, 0.14f));
    g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 0.7f);
    screw (g, { r.getX() + 9.0f, r.getCentreY() }, 2.6f, 1.2f);
    g.setFont (f);
    g.setColour (palette::tagText);
    g.drawText (title, r.withTrimmedLeft (18.0f).withTrimmedRight (6.0f), juce::Justification::centredLeft, false);
}

void raisedPlate (Graphics& g, Rc r)
{
    softShadow (g, r, 3.0f, 6, 0.10f, 1.0f, 2.0f);
    g.setGradientFill (linear (r.getTopLeft(), { r.getX() + r.getWidth() * 0.25f, r.getBottom() }, { { 0.0f, paintTone (0.03f) }, { 1.0f, paintTone (-0.03f) } }));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (C (0.85f, 0.9f, 1.0f, 0.30f));
    g.drawLine (r.getX() + 3, r.getY() + 0.8f, r.getRight() - 3, r.getY() + 0.8f, 1.2f);
    g.drawLine (r.getX() + 0.8f, r.getY() + 3, r.getX() + 0.8f, r.getBottom() - 3, 1.2f);
    g.setColour (C (0.0f, 0.01f, 0.02f, 0.55f));
    g.drawLine (r.getX() + 3, r.getBottom() - 0.7f, r.getRight() - 3, r.getBottom() - 0.7f, 1.4f);
    g.drawLine (r.getRight() - 0.7f, r.getY() + 3, r.getRight() - 0.7f, r.getBottom() - 3, 1.4f);
}

void moduleBody (Graphics& g, Rc f)
{
    softShadow (g, f.withSizeKeepingCentre (f.getWidth() + 2.0f, f.getHeight() + 2.0f), 4.0f, 8, 0.07f, 3.0f, 4.0f);
    const float T = 2.5f, B = 3.5f;
    Path bottom, right;
    bottom.startNewSubPath (f.getX() + 3, f.getBottom());
    bottom.lineTo (f.getRight(), f.getBottom());
    bottom.lineTo (f.getRight() + T, f.getBottom() + B);
    bottom.lineTo (f.getX() + 3 + T, f.getBottom() + B);
    bottom.closeSubPath();
    g.setGradientFill (linear ({ 0, f.getBottom() }, { 0, f.getBottom() + B }, { { 0.0f, paintDkTone (-0.08f) }, { 1.0f, paintDkTone (-0.16f) } }));
    g.fillPath (bottom);
    right.startNewSubPath (f.getRight(), f.getY() + 3);
    right.lineTo (f.getRight() + T, f.getY() + 3 + B);
    right.lineTo (f.getRight() + T, f.getBottom() + B);
    right.lineTo (f.getRight(), f.getBottom());
    right.closeSubPath();
    g.setGradientFill (linear ({ f.getRight(), 0 }, { f.getRight() + T, 0 }, { { 0.0f, paintDkTone (-0.05f) }, { 1.0f, paintDkTone (-0.12f) } }));
    g.fillPath (right);
    g.setGradientFill (linear (f.getTopLeft(), { f.getX() + f.getWidth() * 0.3f, f.getBottom() }, { { 0.0f, paintTone (0.05f) }, { 1.0f, paintTone (-0.01f) } }));
    g.fillRoundedRectangle (f, 3.0f);
    const float c = 2.6f;
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
    quad ({ { x0, y0 }, { x1, y0 }, { x1 - c, y0 + c }, { x0 + c, y0 + c } }, C (0.9f, 0.95f, 1.0f, 0.30f));
    quad ({ { x0, y0 }, { x0 + c, y0 + c }, { x0 + c, y1 - c }, { x0, y1 } }, C (0.9f, 0.95f, 1.0f, 0.18f));
    quad ({ { x0, y1 }, { x0 + c, y1 - c }, { x1 - c, y1 - c }, { x1, y1 } }, C (0.0f, 0.01f, 0.02f, 0.40f));
    quad ({ { x1, y0 }, { x1, y1 }, { x1 - c, y1 - c }, { x1 - c, y0 + c } }, C (0.0f, 0.01f, 0.02f, 0.28f));
}

void blankingPlate (Graphics& g, Rc p)
{
    softShadow (g, p, 3.0f, 6, 0.09f, 2.0f, 3.0f);
    g.setColour (C (0.0f, 0.01f, 0.02f, 0.6f));
    g.fillRoundedRectangle (p.expanded (1.2f), 3.6f);
    g.setGradientFill (linear (p.getTopLeft(), { p.getX() + p.getWidth() * 0.3f, p.getBottom() }, { { 0.0f, paintTone (0.0f) }, { 1.0f, paintTone (-0.045f) } }));
    g.fillRoundedRectangle (p, 3.0f);
    // light rolled rim
    g.setColour (C (0.85f, 0.9f, 1.0f, 0.32f));
    g.drawRoundedRectangle (p.reduced (0.8f), 2.6f, 1.0f);
    g.setColour (C (0.0f, 0.01f, 0.02f, 0.35f));
    g.drawRoundedRectangle (p.reduced (2.2f), 2.0f, 0.8f);
}

void seedDial (Graphics& g, Pt c, float r, float turn)
{
    // fine tick ring painted on the panel
    for (int i = 0; i < 48; ++i)
    {
        const float a = (float) i * 2.0f * kPi / 48.0f;
        const bool major = i % 4 == 0;
        g.setColour (palette::ink.withAlpha (major ? 0.95f : 0.7f));
        g.drawLine ({ polar (c, a, r + 3.5f), polar (c, a, r + (major ? 9.0f : 7.0f)) }, major ? 1.2f : 0.8f);
    }
    aoHalo (g, c, r * 0.85f, 0.4f);
    g.setColour (C (0, 0, 0, 0.5f));
    fillCircle (g, c.translated (2.0f, 3.0f), r);
    g.setGradientFill (linear (c.translated (-r, -r), c.translated (r, r), { { 0.0f, C (0.22f, 0.22f, 0.23f) }, { 1.0f, C (0.03f, 0.03f, 0.035f) } }));
    fillCircle (g, c, r);
    g.setColour (C (0.32f, 0.32f, 0.34f, 0.7f));
    for (int i = 0; i < 90; ++i)
    {
        const float a = (float) i * 2.0f * kPi / 90.0f;
        g.drawLine ({ polar (c, a, r - 3.0f), polar (c, a, r) }, 0.8f);
    }
    const float rc = r * 0.78f;
    g.setGradientFill (radial (c.translated (-rc * 0.35f, -rc * 0.45f), rc * 1.5f,
                               { { 0.0f, C (0.26f, 0.26f, 0.27f) }, { 0.5f, C (0.08f, 0.08f, 0.09f) }, { 1.0f, C (0.02f, 0.02f, 0.025f) } }));
    fillCircle (g, c, rc);
    g.setGradientFill (radial (c.translated (-rc * 0.3f, -rc * 0.5f), rc * 0.6f, { { 0.0f, C (1, 1, 1, 0.16f) }, { 1.0f, C (1, 1, 1, 0.0f) } }));
    fillCircle (g, c, rc);
    const float a = turn * 2.0f * kPi - kPi * 0.5f;
    Path pt;
    pt.startNewSubPath (polar (c, a, rc * 0.45f));
    pt.lineTo (polar (c, a, rc * 0.92f));
    g.setColour (palette::ink);
    g.strokePath (pt, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    Path tri;
    tri.addTriangle (c.x, c.y - r - 11.0f, c.x - 5.0f, c.y - r - 18.0f, c.x + 5.0f, c.y - r - 18.0f);
    g.setColour (palette::red);
    g.fillPath (tri);
}

void selectorDial (Graphics& g, Pt c, float r, float turn, int marks)
{
    aoHalo (g, c, r * 0.72f, 0.4f);
    g.setColour (C (0, 0, 0, 0.5f));
    fillCircle (g, c.translated (2.5f, 3.5f), r);
    g.setGradientFill (radial (c.translated (-r * 0.3f, -r * 0.3f), r * 1.3f, { { 0.0f, C (0.16f, 0.16f, 0.17f) }, { 1.0f, C (0.03f, 0.03f, 0.035f) } }));
    fillCircle (g, c, r);
    g.setColour (C (0.30f, 0.30f, 0.32f, 0.8f));
    for (int i = 0; i < 96; ++i)
    {
        const float a = (float) i * 2.0f * kPi / 96.0f;
        g.drawLine ({ polar (c, a, r - 2.5f), polar (c, a, r) }, 0.8f);
    }
    const float rot = turn * 2.0f * kPi;
    g.setColour (palette::ink);
    for (int i = 0; i < marks; ++i)
    {
        const float a = rot + (float) i * 2.0f * kPi / (float) marks - kPi * 0.5f;
        g.drawLine ({ polar (c, a, r - 5.0f), polar (c, a, r - 11.0f) }, i == 0 ? 2.2f : 1.4f);
    }
    creamKnob (g, c, r * 0.62f, 0.5f);
    Path tri;
    tri.addTriangle (c.x, c.y - r + 1.0f, c.x - 5.0f, c.y - r - 6.0f, c.x + 5.0f, c.y - r - 6.0f);
    g.setColour (palette::red);
    g.fillPath (tri);
}

void vuMeter (Graphics& g, Rc r, float vu, const juce::String& title)
{
    softShadow (g, r, 5.0f, 8, 0.08f, 2.0f, 3.0f);
    g.setColour (C (0.0f, 0.0f, 0.0f, 0.6f));
    g.fillRoundedRectangle (r.expanded (1.0f), 5.5f);
    g.setGradientFill (linear (r.getTopLeft(), r.getBottomRight(), { { 0.0f, C (0.20f, 0.23f, 0.28f) }, { 1.0f, C (0.08f, 0.09f, 0.11f) } }));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (C (0.85f, 0.9f, 1.0f, 0.22f));
    g.drawRoundedRectangle (r.reduced (0.7f), 4.5f, 1.0f);
    for (auto p : { r.getTopLeft().translated (8, 8), r.getTopRight().translated (-8, 8), r.getBottomLeft().translated (8, -8), r.getBottomRight().translated (-8, -8) })
        screw (g, p, 3.2f, 0.7f);
    stencil (g, title, r.getCentreX(), r.getY() + 12.0f, 9.0f, juce::Justification::centred, palette::ink, 1.0f, 0.06f);
    const Rc f (r.getX() + 14.0f, r.getY() + 24.0f, r.getWidth() - 28.0f, r.getHeight() - 38.0f);
    g.setColour (C (0, 0, 0, 0.8f));
    g.fillRoundedRectangle (f.expanded (2.0f), 3.5f);
    g.setGradientFill (linear (f.getTopLeft(), f.getBottomLeft(), { { 0.0f, C (0.93f, 0.90f, 0.80f) }, { 1.0f, C (0.82f, 0.78f, 0.66f) } }));
    g.fillRoundedRectangle (f, 2.5f);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (roundRect (f, 2.5f));
    const Pt pv (f.getCentreX(), f.getBottom() + 22.0f);
    const float R = f.getHeight() + 6.0f;
    auto ang = [] (float db) { const float t = std::pow (juce::jlimit (0.0f, 1.0f, (db + 20.0f) / 23.0f), 1.5f); return juce::degreesToRadians (-138.0f + 96.0f * t); };
    auto onArc = [&] (float a, float rad) { return Pt (pv.x + std::cos (a) * rad, pv.y + std::sin (a) * rad); };
    Path redArc;
    for (int i = 0; i <= 12; ++i) { const auto p = onArc (ang (3.0f * (float) i / 12.0f), R - 4.0f); if (i == 0) redArc.startNewSubPath (p); else redArc.lineTo (p); }
    g.setColour (palette::red);
    g.strokePath (redArc, PathStrokeType (3.0f));
    Path blackArc;
    for (int i = 0; i <= 24; ++i) { const auto p = onArc (ang (-20.0f + 20.0f * (float) i / 24.0f), R - 5.5f); if (i == 0) blackArc.startNewSubPath (p); else blackArc.lineTo (p); }
    g.setColour (palette::black);
    g.strokePath (blackArc, PathStrokeType (0.8f));
    const struct { float db; const char* t; } marks[] = { { -20, "20" }, { -10, "10" }, { -7, "7" }, { -5, "5" }, { -3, "3" }, { -2, "2" }, { -1, "1" }, { 0, "0" }, { 1, "1" }, { 2, "2" }, { 3, "3" } };
    g.setFont (tagFont (6.2f));
    for (auto& m : marks)
    {
        const float a = ang (m.db);
        const auto col = m.db > 0 ? palette::red : palette::black;
        g.setColour (col);
        g.drawLine ({ onArc (a, R - 6.0f), onArc (a, R + 0.5f) }, 1.0f);
        if (m.db == -20.0f || m.db == -10.0f || m.db == -5.0f || m.db == -3.0f || m.db == 0.0f || m.db == 3.0f)
            g.drawText (m.t, Rc (12.0f, 8.0f).withCentre (onArc (a, R + 5.5f)), juce::Justification::centred, false);
    }
    g.setFont (stencilFont (12.0f));
    g.setColour (palette::black);
    g.drawText ("VU", Rc (30, 14).withCentre ({ pv.x, f.getBottom() - 15.0f }), juce::Justification::centred, false);
    const float a = ang (vu);
    g.setColour (C (0, 0, 0, 0.22f));
    g.drawLine ({ pv.translated (2, 0), onArc (a, R + 1.0f).translated (2, 1) }, 1.1f);
    g.setColour (palette::black);
    g.drawLine ({ pv, onArc (a, R + 1.0f) }, 1.1f);
    g.setGradientFill (linear (f.getTopLeft(), f.getBottomRight(), { { 0.0f, C (1, 1, 1, 0.30f) }, { 0.45f, C (1, 1, 1, 0.04f) }, { 1.0f, C (1, 1, 1, 0.0f) } }));
    g.fillRect (f);
}

void crtBezel (Graphics& g, Rc r)
{
    softShadow (g, r, 6.0f, 8, 0.08f, 2.0f, 3.0f);
    g.setColour (C (0, 0, 0, 0.6f));
    g.fillRoundedRectangle (r.expanded (1.0f), 7.0f);
    g.setGradientFill (linear (r.getTopLeft(), r.getBottomLeft(), { { 0.0f, C (0.17f, 0.19f, 0.23f) }, { 1.0f, C (0.06f, 0.07f, 0.09f) } }));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (C (0.85f, 0.9f, 1.0f, 0.18f));
    g.drawRoundedRectangle (r.reduced (0.7f), 5.5f, 1.0f);
    for (auto p : { r.getTopLeft().translated (8, 8), r.getTopRight().translated (-8, 8), r.getBottomLeft().translated (8, -8), r.getBottomRight().translated (-8, -8) })
        screw (g, p, 3.0f, 0.4f);
    const auto gl = layout::glass (r);
    g.setColour (C (0.02f, 0.035f, 0.04f));
    g.fillRoundedRectangle (gl, 9.0f);
    g.setColour (C (0, 0, 0, 0.85f));
    g.drawRoundedRectangle (gl, 9.0f, 2.0f);
}

void case_ (Graphics& g)
{
    using namespace layout;
    g.fillAll (C (0.09f, 0.10f, 0.12f));
    // painted frame the plates are screwed to
    g.setGradientFill (linear (panel.getTopLeft(), { panel.getX() + panel.getWidth() * 0.3f, panel.getBottom() },
                               { { 0.0f, paintDkTone (-0.04f) }, { 1.0f, paintDkTone (-0.09f) } }));
    g.fillRect (panel);
    // side rails
    for (float x : { 0.0f, kW - 10.0f })
    {
        const Rc rail (x, panel.getY(), 10.0f, panel.getHeight());
        g.setGradientFill (linear (rail.getTopLeft(), rail.getTopRight(), { { 0.0f, paintTone (-0.06f) }, { 0.4f, paintTone (0.02f) }, { 1.0f, paintDkTone (-0.10f) } }));
        g.fillRect (rail);
        g.setColour (C (0.85f, 0.9f, 1.0f, 0.18f));
        g.fillRect (rail.getX() + 2.5f, rail.getY(), 1.0f, rail.getHeight());
    }
}
} // namespace hv::ui::mil
