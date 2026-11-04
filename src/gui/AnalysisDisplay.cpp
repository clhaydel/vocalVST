#include "AnalysisDisplay.h"

namespace hv::ui
{
using namespace params;

AnalysisDisplay::AnalysisDisplay (HarshVoxProcessor& p) : proc (p)
{
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
    using namespace palette;
    auto r = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 5.0f);
    r.reduce (10.0f, 8.0f);
    auto right = r.removeFromRight (std::min (360.0f, r.getWidth() * 0.36f));
    r.removeFromRight (14.0f);
    paintTimeline (g, r);
    paintTract (g, right);
}

void AnalysisDisplay::paintTimeline (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace palette;
    auto head2 = r.removeFromTop (16.0f);
    BlockFont::draw (g, "ENVELOPE / PITCH", head2.getX(), head2.getY() + 2.0f, 7.0f, textDim);
    const auto& last = hist[(size_t) ((head - 1 + kLen) % kLen)];
    // V / F / P indicators
    {
        auto ind = head2.removeFromRight (150.0f);
        const struct { const char* n; float v; juce::Colour c; } cls[] = { { "V", last.voiced, text }, { "F", last.fricative, amber }, { "P", last.plosive, brand } };
        for (int i = 0; i < 3; ++i)
        {
            auto b = ind.removeFromLeft (46.0f).reduced (2.0f, 0.0f);
            g.setColour (cls[i].c.withAlpha (0.12f + 0.88f * juce::jlimit (0.0f, 1.0f, cls[i].v)));
            g.fillRect (b);
            BlockFont::drawCentred (g, cls[i].n, b, 7.0f, bg0);
        }
        g.setFont (monoFont (11.0f));
        g.setColour (text);
        const juce::String f0txt = last.f0 > 0.0f ? juce::String (juce::roundToInt (last.f0)) + " Hz" : "--- Hz";
        g.drawText ("f0 " + f0txt + "   conf " + juce::String (last.conf, 2), head2.withTrimmedRight (10.0f), juce::Justification::centredRight);
    }
    r.removeFromTop (4.0f);
    auto strip = r.removeFromBottom (6.0f);
    r.removeFromBottom (3.0f);
    g.setColour (bg0);
    g.fillRect (r);
    g.fillRect (strip);
    // grid
    g.setColour (line);
    for (float db : { -12.0f, -24.0f, -36.0f, -48.0f })
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (-db / 60.0f)), r.getX(), r.getRight());

    const int W = (int) r.getWidth();
    const int pts = kLen;
    auto pointAt = [&] (int col) -> const AnalysisTelemetry::Point& {
        const int idx = (int) ((float) col / (float) std::max (1, W - 1) * (float) (pts - 1));
        return hist[(size_t) ((head + idx) % kLen)];
    };
    // envelope fill
    juce::Path env;
    env.startNewSubPath (r.getX(), r.getBottom());
    for (int c = 0; c < W; ++c)
    {
        const float db = juce::jlimit (-60.0f, 0.0f, pointAt (c).envDb);
        env.lineTo (r.getX() + (float) c, r.getBottom() - r.getHeight() * (db + 60.0f) / 60.0f);
    }
    env.lineTo (r.getRight(), r.getBottom());
    env.closeSubPath();
    g.setColour (textDim.withAlpha (0.35f));
    g.fillPath (env);
    // f0: log 40..1000 Hz, alpha = confidence
    auto f0y = [&r] (float f) { return r.getBottom() - r.getHeight() * std::log (f / 40.0f) / std::log (25.0f); };
    for (int c = 1; c < W; ++c)
    {
        const auto& a = pointAt (c - 1);
        const auto& b = pointAt (c);
        if (a.f0 <= 0.0f || b.f0 <= 0.0f) continue;
        const float conf = juce::jlimit (0.0f, 1.0f, b.conf);
        g.setColour (accent.withAlpha (0.15f + 0.85f * conf * conf));
        g.drawLine (r.getX() + (float) (c - 1), f0y (a.f0), r.getX() + (float) c, f0y (b.f0), 1.6f);
    }
    // class strip
    for (int c = 0; c < W; ++c)
    {
        const auto& p = pointAt (c);
        juce::Colour col = bg0;
        float a = 0.0f;
        if (p.plosive > 0.3f) { col = brand; a = p.plosive; }
        else if (p.fricative > p.voiced) { col = amber; a = p.fricative; }
        else { col = text; a = p.voiced; }
        if (a > 0.05f) { g.setColour (col.withAlpha (a)); g.fillRect (strip.getX() + (float) c, strip.getY(), 1.0f, strip.getHeight()); }
    }
    g.setFont (monoFont (9.5f));
    g.setColour (textFaint);
    for (float f : { 100.0f, 250.0f, 500.0f })
        g.drawText (juce::String ((int) f), juce::Rectangle<float> (r.getRight() - 30.0f, f0y (f) - 6.0f, 28.0f, 12.0f), juce::Justification::centredRight);
}

void AnalysisDisplay::paintTract (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace palette;
    auto head2 = r.removeFromTop (16.0f);
    BlockFont::draw (g, "TRACT (LPC)", head2.getX(), head2.getY() + 2.0f, 7.0f, textDim);
    if (targetSlot >= 0)
    {
        const float w = BlockFont::width ("TARGET S" + juce::String (slotLabel (targetSlot)), 7.0f);
        BlockFont::draw (g, "TARGET S" + juce::String (slotLabel (targetSlot)), head2.getRight() - w, head2.getY() + 2.0f, 7.0f, accent);
    }
    r.removeFromTop (4.0f);
    g.setColour (bg0);
    g.fillRect (r);
    g.setColour (line);
    for (float f : { 1000.0f, 2000.0f, 3000.0f, 4000.0f, 5000.0f, 6000.0f, 7000.0f })
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * f / kEnvMaxHz), r.getY(), r.getBottom());

    float mx = -200.0f;
    for (float v : curve) mx = std::max (mx, v);
    if (mx < -100.0f) return;
    const float range = 50.0f;
    auto yOf = [&] (float db) { return r.getY() + r.getHeight() * juce::jlimit (0.0f, 1.0f, (mx + 3.0f - db) / range); };
    juce::Path p;
    for (int i = 0; i < kEnvPoints; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (kEnvPoints - 1);
        if (i == 0) p.startNewSubPath (x, yOf (curve[(size_t) i])); else p.lineTo (x, yOf (curve[(size_t) i]));
    }
    g.setColour (text);
    g.strokePath (p, juce::PathStrokeType (1.5f));
    // live formant markers
    for (int i = 0; i < live.count; ++i)
    {
        const float x = r.getX() + r.getWidth() * live.freq[i] / kEnvMaxHz;
        g.setColour (text.withAlpha (0.5f));
        g.fillRect (x - 1.0f, r.getBottom() - 6.0f, 2.0f, 6.0f);
    }
    // target overlay: magnitude of the 5 target resonators, normalised to the live peak
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
            const double m = std::sqrt (re * re + im * im);
            // same (pre-emphasised) domain as the live LPC curve; normalise on the formant region only
            tdb[(size_t) i] = (float) (20.0 * std::log10 (std::max (1e-9, m)));
            if (f > 150.0) tmx = std::max (tmx, tdb[(size_t) i]);
        }
        juce::Path tp;
        for (int i = 0; i < kEnvPoints; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (kEnvPoints - 1);
            const float y = yOf (tdb[(size_t) i] - tmx + mx);
            if (i == 0) tp.startNewSubPath (x, y); else tp.lineTo (x, y);
        }
        g.setColour (accent);
        juce::Path dashed;
        const float dash[] = { 5.0f, 3.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, tp, dash, 2);
        g.fillPath (dashed);
    }
    g.setFont (monoFont (9.5f));
    g.setColour (textFaint);
    for (int k : { 2, 4, 6 })
        g.drawText (juce::String (k) + "k", juce::Rectangle<float> (r.getX() + r.getWidth() * (float) k * 1000.0f / kEnvMaxHz + 2.0f, r.getBottom() - 13.0f, 24.0f, 12.0f), juce::Justification::centredLeft);
}
} // namespace hv::ui
