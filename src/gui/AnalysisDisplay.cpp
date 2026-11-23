#include "AnalysisDisplay.h"

namespace hv::ui
{
using namespace params;
using Rc = juce::Rectangle<float>;

namespace
{
/** Phosphor trace: wide faint glow + mid glow + bright core. */
void phosphor (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w = 1.2f, const float* dash = nullptr)
{
    const struct { float w, a; } passes[] = { { w * 5.0f, 0.07f }, { w * 2.6f, 0.18f }, { w, 0.95f } };
    for (auto& ps : passes)
    {
        g.setColour (c.withAlpha (ps.a));
        if (dash != nullptr)
        {
            juce::Path d;
            juce::PathStrokeType (ps.w).createDashedStroke (d, p, dash, 2);
            g.fillPath (d);
        }
        else g.strokePath (p, juce::PathStrokeType (ps.w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void graticule (juce::Graphics& g, Rc r, int nx, int ny)
{
    g.setColour (palette::phosphor.withAlpha (0.12f));
    for (int i = 1; i < nx; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / (float) nx), r.getY(), r.getBottom());
    for (int i = 1; i < ny; ++i) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / (float) ny), r.getX(), r.getRight());
}

void glassSheen (juce::Graphics& g, Rc r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), r.getX(), r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.3f, r.getY() + r.getHeight(), false));
    g.fillRect (r);
}
} // namespace

AnalysisDisplay::AnalysisDisplay (HarshVoxProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    for (auto& h : hist) h = { -120.0f, 0, 0, 0, 0, 0 };
    lastRead = proc.engine().analysis().telemetry().writeIndex.load();
    curve.fill (-120.0f);
}

void AnalysisDisplay::tick (int selectedSlot)
{
    auto& t = proc.engine().analysis().telemetry();
    const int w = t.writeIndex.load (std::memory_order_acquire);
    int from = std::max (lastRead, w - AnalysisTelemetry::kHistory + 16);
    for (int i = from; i < w; ++i)
    {
        hist[(size_t) head] = t.hist[(size_t) (i % AnalysisTelemetry::kHistory)];
        head = (head + 1) % kLen;
    }
    lastRead = w;
    for (int i = 0; i < kEnvPoints; ++i) curve[(size_t) i] = t.lpcCurve[(size_t) i].load (std::memory_order_relaxed);
    live.count = t.numFormants.load();
    for (int i = 0; i < kMaxFormants; ++i)
    {
        live.freq[i] = t.formants[(size_t) i].load();
        live.bw[i]   = t.formants[(size_t) (i + kMaxFormants)].load();
    }
    // overlay the target of the selected Formant Freeze slot, else the first active one
    targetSlot = -1;
    auto isFormant = [this] (int s) { return proc.slotType (s) == (int) ModuleType::FormantFreeze && proc.paramReal (slotIndex (s, SOn)) >= 0.5f; };
    if (selectedSlot >= 0 && isFormant (selectedSlot)) targetSlot = selectedSlot;
    else for (int s = 0; s < kNumSlots && targetSlot < 0; ++s) if (isFormant (s)) targetSlot = s;
    if (targetSlot >= 0)
    {
        auto& sh = proc.engine().slotShared (targetSlot);
        for (int i = 0; i < kMaxFormants; ++i)
        {
            targetF[i] = sh.targetFormants[(size_t) i].load();
            targetB[i] = sh.targetFormants[(size_t) (i + kMaxFormants)].load();
        }
    }
    repaint();
}

void AnalysisDisplay::paint (juce::Graphics& g)
{
    const auto o = getPosition().toFloat();
    for (auto [bezel, left] : { std::pair { layout::scopeL, true }, std::pair { layout::scopeR, false } })
    {
        const auto gl = layout::glass (bezel).translated (-o.x, -o.y);
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (gl, 9.0f);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (juce::Colour::fromFloatRGBA (0.05f, 0.12f, 0.10f, 1.0f), gl.getCentreX(), gl.getCentreY(),
                                                 juce::Colour::fromFloatRGBA (0.01f, 0.03f, 0.03f, 1.0f), gl.getCentreX() + gl.getWidth() * 0.65f, gl.getCentreY(), true));
        g.fillRect (gl);
        graticule (g, gl, 10, 6);
        if (left) paintTimeline (g, gl.reduced (6.0f, 4.0f));
        else paintTract (g, gl.reduced (6.0f, 4.0f));
        glassSheen (g, gl);
    }
}

void AnalysisDisplay::paintTimeline (juce::Graphics& g, Rc r)
{
    auto strip = r.removeFromBottom (3.0f);
    const int W = (int) r.getWidth();
    auto pointAt = [&] (int col) -> const AnalysisTelemetry::Point& {
        const int idx = (int) ((float) col / (float) std::max (1, W - 1) * (float) (kLen - 1));
        return hist[(size_t) ((head + idx) % kLen)];
    };
    juce::Path env, fill;
    fill.startNewSubPath (r.getX(), r.getBottom());
    for (int c = 0; c < W; ++c)
    {
        const float db = juce::jlimit (-60.0f, 0.0f, pointAt (c).envDb);
        const float y = r.getBottom() - r.getHeight() * 0.92f * (db + 60.0f) / 60.0f;
        if (c == 0) env.startNewSubPath (r.getX(), y); else env.lineTo (r.getX() + (float) c, y);
        fill.lineTo (r.getX() + (float) c, y);
    }
    fill.lineTo (r.getRight(), r.getBottom());
    fill.closeSubPath();
    g.setColour (palette::phosphor.withAlpha (0.10f));
    g.fillPath (fill);
    phosphor (g, env, palette::phosphor, 1.1f);
    // f0: log 40..1000 Hz, segments where confident
    auto f0y = [&r] (float f) { return r.getBottom() - r.getHeight() * std::log (f / 40.0f) / std::log (25.0f); };
    juce::Path pitch;
    bool open = false;
    for (int c = 0; c < W; ++c)
    {
        const auto& p = pointAt (c);
        if (p.f0 <= 0.0f || p.conf < 0.5f) { open = false; continue; }
        const float y = f0y (p.f0);
        if (! open) { pitch.startNewSubPath (r.getX() + (float) c, y); open = true; }
        else pitch.lineTo (r.getX() + (float) c, y);
    }
    phosphor (g, pitch, juce::Colour::fromFloatRGBA (0.9f, 1.0f, 0.55f, 1.0f), 1.2f);
    for (int c = 0; c < W; ++c)
    {
        const auto& p = pointAt (c);
        juce::Colour col;
        float a = 0.0f;
        if (p.plosive > 0.3f) { col = palette::red.brighter (0.4f); a = p.plosive; }
        else if (p.fricative > p.voiced) { col = palette::amber; a = p.fricative; }
        else { col = palette::phosphor; a = p.voiced; }
        if (a > 0.05f) { g.setColour (col.withAlpha (a * 0.8f)); g.fillRect (strip.getX() + (float) c, strip.getY(), 1.0f, strip.getHeight()); }
    }
    g.setFont (tagFont (7.0f));
    g.setColour (palette::phosphor.withAlpha (0.45f));
    for (float f : { 100.0f, 250.0f, 500.0f })
        g.drawText (juce::String ((int) f), Rc (r.getRight() - 30.0f, f0y (f) - 6.0f, 28.0f, 12.0f), juce::Justification::centredRight);
}

void AnalysisDisplay::paintTract (juce::Graphics& g, Rc r)
{
    float mx = -200.0f;
    for (float v : curve) mx = std::max (mx, v);
    g.setFont (tagFont (7.0f));
    g.setColour (palette::phosphor.withAlpha (0.45f));
    for (int k : { 2, 4, 6 })
        g.drawText (juce::String (k) + "k", Rc (r.getX() + r.getWidth() * (float) k * 1000.0f / kEnvMaxHz + 2.0f, r.getBottom() - 11.0f, 24.0f, 10.0f), juce::Justification::centredLeft);
    if (targetSlot >= 0)
    {
        g.setColour (palette::amber);
        g.drawText ("TARGET S" + juce::String (slotLabel (targetSlot)), r.removeFromTop (10.0f), juce::Justification::centredRight);
    }
    if (mx < -100.0f) return;
    const float range = 50.0f;
    auto yOf = [&] (float db) { return r.getY() + r.getHeight() * juce::jlimit (0.0f, 1.0f, (mx + 3.0f - db) / range); };
    juce::Path p;
    for (int i = 0; i < kEnvPoints; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (kEnvPoints - 1);
        if (i == 0) p.startNewSubPath (x, yOf (curve[(size_t) i])); else p.lineTo (x, yOf (curve[(size_t) i]));
    }
    phosphor (g, p, palette::phosphor, 1.2f);
    for (int i = 0; i < live.count; ++i)
    {
        const float x = r.getX() + r.getWidth() * live.freq[i] / kEnvMaxHz;
        g.setColour (palette::phosphor.withAlpha (0.5f));
        g.fillRect (x - 1.0f, r.getBottom() - 5.0f, 2.0f, 5.0f);
    }
    if (targetSlot >= 0)
    {
        std::array<Biquad, kMaxFormants> res;
        const double sr = 48000.0;
        static constexpr float gains[5] = { 1.0f, 0.71f, 0.5f, 0.35f, 0.25f };   // mirrors FormantFreeze's bank
        for (int i = 0; i < kMaxFormants; ++i) res[(size_t) i].formantBand (sr, std::max (60.0f, targetF[i]), std::max (20.0f, targetB[i]));
        std::array<float, kEnvPoints> tdb {};
        float tmx = -200.0f;
        for (int i = 0; i < kEnvPoints; ++i)
        {
            const double f = std::max (20.0, (double) i * kEnvMaxHz / (kEnvPoints - 1));
            double re = 0.06, im = 0.0;
            for (int k = 0; k < kMaxFormants; ++k)
            {
                const auto h = res[(size_t) k].response (sr, f);
                re += gains[k] * h[0];
                im += gains[k] * h[1];
            }
            tdb[(size_t) i] = (float) (20.0 * std::log10 (std::max (1e-9, std::sqrt (re * re + im * im))));
            if (f > 150.0) tmx = std::max (tmx, tdb[(size_t) i]);
        }
        juce::Path tp;
        for (int i = 0; i < kEnvPoints; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (kEnvPoints - 1);
            const float y = yOf (tdb[(size_t) i] - tmx + mx);
            if (i == 0) tp.startNewSubPath (x, y); else tp.lineTo (x, y);
        }
        const float dash[] = { 4.0f, 3.0f };
        phosphor (g, tp, juce::Colour::fromFloatRGBA (1.0f, 0.5f, 0.35f, 1.0f), 1.0f, dash);
    }
}

//==============================================================================
void ClassifierPanel::update (const AnalysisTelemetry::Point& p)
{
    const bool nv = p.voiced > 0.5f, nf = p.fricative > 0.5f, np = p.plosive > 0.3f;
    const juce::String nf0 = p.f0 > 0.0f && p.conf > 0.3f ? juce::String (juce::roundToInt (p.f0)) : juce::String ("---");
    const juce::String nc = juce::String (p.conf, 2);
    if (nv != v || nf != f || np != pl || nf0 != f0Text || nc != confText)
    {
        v = nv; f = nf; pl = np; f0Text = nf0; confText = nc;
        repaint();
    }
}

void ClassifierPanel::paint (juce::Graphics& g)
{
    const float cx = (float) getWidth() * 0.5f;
    mil::stencil (g, "CLASSIFIER", cx, 8.0f, 9.0f, juce::Justification::centred, palette::ink, 1.0f, 0.06f);
    const struct { const char* n; juce::Colour c; bool lit; } lamps[] = { { "V", palette::green, v }, { "F", palette::amber, f }, { "P", palette::red.brighter (0.3f), pl } };
    for (int i = 0; i < 3; ++i)
    {
        const juce::Point<float> c (cx + (float) (i - 1) * 36.0f, 36.0f);
        mil::jewel (g, c, 7.5f, lamps[i].c, lamps[i].lit);
        mil::stencil (g, lamps[i].n, c.x, c.y + 20.0f, 8.4f, juce::Justification::centred);
    }
    g.setFont (tagFont (9.0f));
    g.setColour (palette::ink);
    g.drawText ("f0  " + f0Text.paddedLeft (' ', 4) + " Hz", juce::Rectangle<float> (0.0f, 79.0f, (float) getWidth(), 12.0f), juce::Justification::centred, false);
    g.drawText ("CONF  " + confText, juce::Rectangle<float> (0.0f, 97.0f, (float) getWidth(), 12.0f), juce::Justification::centred, false);
}
} // namespace hv::ui
