#include "PanelArt.h"
#include "BlockFont.h"
#include "BinaryData.h"
#include <thread>

namespace hv::ui
{
using namespace layout;
using Rc = juce::Rectangle<float>;
using Pt = juce::Point<float>;

namespace
{
/** Square, power-of-two, tiling texture held as floats (1 or 3 channels). */
struct Tex
{
    int n = 0, ch = 1;
    std::vector<float> d;

    void load (const char* data, int size, int channels)
    {
        auto img = juce::ImageFileFormat::loadFrom (data, (size_t) size);
        ch = channels;
        n = juce::nextPowerOfTwo (std::max (1, img.getWidth()));
        if (img.getWidth() != n || img.getHeight() != n) img = img.rescaled (n, n, juce::Graphics::highResamplingQuality);
        d.assign ((size_t) (n * n * ch), 0.0f);
        img = img.convertedToFormat (juce::Image::ARGB);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
            {
                const auto* px = reinterpret_cast<const juce::PixelARGB*> (bd.getPixelPointer (x, y));
                float* o = &d[(size_t) ((y * n + x) * ch)];
                o[0] = px->getRed() * (1.0f / 255.0f);
                if (ch == 3) { o[1] = px->getGreen() * (1.0f / 255.0f); o[2] = px->getBlue() * (1.0f / 255.0f); }
            }
    }
    /** Bilinear sample; u, v in logical units, `tile` = logical size of one repeat. */
    inline void sample (float u, float v, float tile, float* out) const noexcept
    {
        const float fu = u / tile * (float) n, fv = v / tile * (float) n;
        const int x0 = (int) std::floor (fu), y0 = (int) std::floor (fv);
        const float ax = fu - (float) x0, ay = fv - (float) y0;
        const int m = n - 1;
        const float* p00 = &d[(size_t) ((((y0) & m) * n + ((x0) & m)) * ch)];
        const float* p10 = &d[(size_t) ((((y0) & m) * n + ((x0 + 1) & m)) * ch)];
        const float* p01 = &d[(size_t) ((((y0 + 1) & m) * n + ((x0) & m)) * ch)];
        const float* p11 = &d[(size_t) ((((y0 + 1) & m) * n + ((x0 + 1) & m)) * ch)];
        for (int c = 0; c < ch; ++c)
            out[c] = (p00[c] * (1 - ax) + p10[c] * ax) * (1 - ay) + (p01[c] * (1 - ax) + p11[c] * ax) * ay;
    }
    inline float sample1 (float u, float v, float tile) const noexcept { float o[3]; sample (u, v, tile, o); return o[0]; }
};

struct Textures
{
    Tex normal, chips, chipsBlur, streaks, specks, scratches, prints, rust;
    Textures()
    {
        normal.load (BinaryData::relief_normal_jpg, BinaryData::relief_normal_jpgSize, 3);
        chips.load (BinaryData::chips_jpg, BinaryData::chips_jpgSize, 1);
        chipsBlur.load (BinaryData::chips_blur_jpg, BinaryData::chips_blur_jpgSize, 1);
        streaks.load (BinaryData::streaks_jpg, BinaryData::streaks_jpgSize, 1);
        specks.load (BinaryData::specks_jpg, BinaryData::specks_jpgSize, 1);
        scratches.load (BinaryData::scratches_jpg, BinaryData::scratches_jpgSize, 1);
        prints.load (BinaryData::fingerprints_jpg, BinaryData::fingerprints_jpgSize, 1);
        rust.load (BinaryData::rust_jpg, BinaryData::rust_jpgSize, 3);
    }
    static const Textures& get() { static const Textures t; return t; }
};

uint32_t hash (uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
float hash01 (uint32_t x) { return (float) (hash (x) & 0xffffff) / (float) 0xffffff; }
float clamp01 (float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
juce::Colour C (float r, float g, float b, float a = 1.0f) { return juce::Colour::fromFloatRGBA (r, g, b, a); }

Rc centred (Pt c, float w, float h) { return { c.x - w * 0.5f, c.y - h * 0.5f, w, h }; }

/** Black label tag centred on c (scope captions). */
void captionTag (juce::Graphics& g, Pt c, const juce::String& s)
{
    auto f = stencilFont (9.0f).withExtraKerningFactor (0.06f);
    const auto r = centred (c, juce::GlyphArrangement::getStringWidth (f, s) + 26.0f, 17.0f);
    mil::softShadow (g, r, 2.0f, 3, 0.10f, 0.8f, 1.2f);
    g.setGradientFill (juce::ColourGradient (C (0.12f, 0.13f, 0.15f), 0, r.getY(), C (0.04f, 0.045f, 0.055f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (C (1, 1, 1, 0.16f));
    g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 0.7f);
    g.setFont (f);
    g.setColour (palette::tagText);
    g.drawText (s, r, juce::Justification::centred, false);
}
} // namespace

//==============================================================================
std::vector<PanelArt::ScrewSpot> PanelArt::screws (uint8_t occupied)
{
    std::vector<ScrewSpot> s;
    auto add = [&s] (Pt c, float r)
    {
        const auto i = (uint32_t) s.size() + 17u;
        s.push_back ({ c, r, hash01 (i * 31u) * juce::MathConstants<float>::pi, false });
    };
    for (auto& r : plates)
        for (auto c : { r.getTopLeft().translated (9, 9), r.getTopRight().translated (-9, 9), r.getBottomLeft().translated (9, -9), r.getBottomRight().translated (-9, -9) })
            add (c, 4.8f);
    for (int i = 0; i < 8; ++i)
        if ((occupied & (1u << i)) == 0)
        {
            const auto b = blankPlate (bay (i));
            for (auto c : { b.getTopLeft().translated (11, 11), b.getTopRight().translated (-11, 11), b.getBottomLeft().translated (11, -11), b.getBottomRight().translated (-11, -11) })
                add (c, 4.4f);
        }
    const auto& d = dataPlate;
    for (auto c : { d.getTopLeft().translated (10, 9), d.getTopRight().translated (-10, 9), d.getBottomLeft().translated (10, -15), d.getBottomRight().translated (-10, -15) })
        add (c, 3.8f);
    return s;
}

//==============================================================================
void PanelArt::finish (juce::Image& img, float k, uint8_t occupied, const std::vector<ScrewSpot>& spots)
{
    const auto& T = Textures::get();
    const int w = img.getWidth(), h = img.getHeight();
    const int x0 = 0, y0 = std::max (0, (int) std::ceil (panel.getY() * k));
    const int x1 = std::min (w, (int) std::ceil (panel.getRight() * k)), y1 = std::min (h, (int) std::ceil (panel.getBottom() * k));
    const float inv = 1.0f / k;
    // masks stored as bytes (0..255) to keep memory low at high pixel scales
    std::vector<uint8_t> wear ((size_t) (w * h), 0), hands ((size_t) (w * h), 0), rust ((size_t) (w * h), 0);
    auto put = [] (uint8_t& o, float v) { const auto q = (uint8_t) std::min (255.0f, v * 255.0f + 0.5f); if (q > o) o = q; };
    auto idx = [w] (int x, int y) { return (size_t) (y * w + x); };

    // ---- wear map: paint rubbed off along edges (scatter over each edge band only)
    auto edgeBand = [&] (Rc r, float strength, float width)
    {
        const float band = width * 6.0f;
        const int rx0 = std::max (0, (int) (r.getX() * k)), ry0 = std::max (0, (int) (r.getY() * k));
        const int rx1 = std::min (w, (int) (r.getRight() * k)), ry1 = std::min (h, (int) (r.getBottom() * k));
        for (int py = ry0; py < ry1; ++py)
        {
            const float ly = ((float) py + 0.5f) * inv;
            const float dy = std::min (ly - r.getY(), r.getBottom() - ly);
            auto run = [&] (int a, int b)
            {
                for (int px = a; px < b; ++px)
                {
                    const float lx = ((float) px + 0.5f) * inv;
                    const float d = std::min ({ dy, lx - r.getX(), r.getRight() - lx });
                    if (d < 0.0f || d > band) continue;
                    put (wear[idx (px, py)], strength * std::exp (-d / width));
                }
            };
            if (dy <= band) run (rx0, rx1);
            else
            {
                run (rx0, std::min (rx1, (int) ((r.getX() + band) * k) + 1));
                run (std::max (rx0, (int) ((r.getRight() - band) * k) - 1), rx1);
            }
        }
    };
    for (auto& r : plates) edgeBand (r, 0.9f, 5.0f);
    for (int i = 0; i < 8; ++i) edgeBand (moduleFace (bay (i)), 0.8f, 3.5f);
    juce::ignoreUnused (occupied);

    // ---- hand wear: rings round the big, much-used knobs
    for (auto& ring : wornKnobs())
    {
        const float R = ring.r, ext = R * 3.0f, amp = R > 19.0f ? 1.0f : 0.6f;
        const int bx0 = std::max (0, (int) ((ring.c.x - ext) * k)), bx1 = std::min (w, (int) ((ring.c.x + ext) * k));
        const int by0 = std::max (0, (int) ((ring.c.y - ext) * k)), by1 = std::min (h, (int) ((ring.c.y + ext) * k));
        for (int py = by0; py < by1; ++py)
            for (int px = bx0; px < bx1; ++px)
            {
                const float dx = ((float) px + 0.5f) * inv - ring.c.x, dy = ((float) py + 0.5f) * inv - ring.c.y;
                const float q = (std::sqrt (dx * dx + dy * dy) - 1.55f * R) / (0.45f * R);
                put (hands[idx (px, py)], amp * std::exp (-q * q));
            }
    }

    // ---- rust: blooms round some screws + streaks running down
    for (size_t si = 0; si < spots.size(); ++si)
    {
        const auto& s = spots[si];
        if (! s.rusty) continue;
        const float L = 15.0f + 30.0f * hash01 ((uint32_t) si * 13u + 5u);
        const float bloomR = s.r * 2.2f, ext = bloomR * 2.2f;
        const int bx0 = std::max (0, (int) ((s.c.x - ext) * k)), bx1 = std::min (w, (int) ((s.c.x + ext) * k));
        const int by0 = std::max (0, (int) ((s.c.y - ext) * k)), by1 = std::min (h, (int) ((s.c.y + std::max (ext, L * 4.0f)) * k));
        for (int py = by0; py < by1; ++py)
            for (int px = bx0; px < bx1; ++px)
            {
                const float lx = ((float) px + 0.5f) * inv, ly = ((float) py + 0.5f) * inv;
                const float dx = lx - s.c.x, dy = ly - s.c.y;
                const float q = std::sqrt (dx * dx + dy * dy) / bloomR;
                float v = 0.9f * std::exp (-q * q);
                if (dy > 0.0f && std::abs (dx) < s.r * 0.9f) v += 0.55f * std::exp (-dy / L);
                put (rust[idx (px, py)], v * (1.0f / 1.45f));
            }
    }

    // ---- shade every panel pixel
    const float Lx = -0.45f, Ly = -0.55f, Lz = 0.70f, Ln = std::sqrt (Lx * Lx + Ly * Ly + Lz * Lz);
    const float lx_ = Lx / Ln, ly_ = Ly / Ln, lz_ = Lz / Ln;
    juce::Image::BitmapData bd (img, x0, y0, x1 - x0, y1 - y0, juce::Image::BitmapData::readWrite);
    // light falloff is separable: precompute the column terms once
    std::vector<float> colLin ((size_t) w), colHot ((size_t) w);
    for (int px = x0; px < x1; ++px)
    {
        const float u = ((float) px + 0.5f) * inv / kW, du = (u - 0.22f) / 0.3f;
        colLin[(size_t) px] = 0.55f * u;
        colHot[(size_t) px] = std::exp (-du * du);
    }
    auto shadeRows = [&] (int ya, int yb)
    {
    for (int py = ya; py < yb; ++py)
    {
        const float ly = ((float) py + 0.5f) * inv, v = ly / kH;
        const float dv = (v - 0.18f) / 0.3f, rowHot = std::exp (-dv * dv), rowLin = 0.45f * v, rowDust = 1.0f - 0.06f * v * v;
        for (int px = x0; px < x1; ++px)
        {
            auto* pix = reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (px - x0, py - y0));
            if (pix->getAlpha() == 0) continue;
            const float lx = ((float) px + 0.5f) * inv, u = lx / kW;
            float r = pix->getRed() / 255.0f, g = pix->getGreen() / 255.0f, b = pix->getBlue() / 255.0f;

            // 1. relief from a real painted-metal normal map, lit top-left
            float nm[3];
            T.normal.sample (lx, ly, 427.0f, nm);
            const float nx = nm[0] * 2 - 1, ny = -(nm[1] * 2 - 1), nz = std::max (0.2f, nm[2] * 2 - 1);
            const float shade = (nx * lx_ + ny * ly_ + nz * lz_) / std::sqrt (nx * nx + ny * ny + nz * nz) / lz_;
            float mult = 1.0f + (shade - 1.0f) * 0.45f;
            // 2-3. weathering streaks, dirt specks
            const float st = T.streaks.sample1 (lx, ly, 600.0f), sp = T.specks.sample1 (lx, ly, 467.0f);
            mult *= 1.0f - 0.24f * clamp01 ((st - 0.15f) * 1.6f) - 0.20f * sp;
            // uneven light; dust settles toward the bottom
            mult *= (1.08f - 0.18f * (colLin[(size_t) px] + rowLin) + 0.06f * colHot[(size_t) px] * rowHot) * rowDust;
            r *= mult; g *= mult; b *= mult;

            const size_t i = idx (px, py);
            const float handW = (float) hands[i] * (1.0f / 255.0f);
            const float wr = clamp01 ((float) wear[i] * (1.0f / 255.0f) + 0.35f * handW);
            // 4. chips through to bare metal, with darker rims
            const float ch = T.chips.sample1 (lx, ly, 373.0f), cb = T.chipsBlur.sample1 (lx, ly, 373.0f);
            const float f = 0.35f + 1.4f * wr;
            const float chip = clamp01 ((ch * f - 0.50f) * 3.0f);
            const float rim = clamp01 (clamp01 ((cb * f - 0.50f) * 3.0f) - chip);
            const float bareK = 0.85f + 0.25f * ch;
            r = r * (1 - chip) + 0.60f * bareK * chip;
            g = g * (1 - chip) + 0.62f * bareK * chip;
            b = b * (1 - chip) + 0.65f * bareK * chip;
            const float rimK = 1.0f - 0.55f * rim;
            r *= rimK; g *= rimK; b *= rimK;
            // 5. hairline scratches
            const float sc = 0.26f * clamp01 (T.scratches.sample1 (lx, ly, 533.0f) * 1.4f);
            r += (0.62f - r) * sc; g += (0.65f - g) * sc; b += (0.69f - b) * sc;
            // keep the enamel's blue (not the bare chips)
            const float lum = 0.299f * r + 0.587f * g + 0.114f * b, sat = 1.0f + 0.15f * (1.0f - chip);
            r = lum + (r - lum) * sat; g = lum + (g - lum) * sat; b = lum + (b - lum) * sat;
            // 6. fingerprint smudges round the worn knobs
            if (handW > 0.01f)
            {
                const float fp = 1.0f - 0.10f * T.prints.sample1 (lx, ly, 280.0f) * handW;
                r *= fp; g *= fp; b *= fp;
            }
            // 7. rust
            if (rust[i] > 0)
            {
                const float rv = (float) rust[i] * (1.45f / 255.0f);
                const float rm = std::min (0.9f, rv * (0.55f + 0.9f * T.specks.sample1 (lx, ly, 200.0f)));
                float rc[3];
                T.rust.sample (lx, ly, 341.0f, rc);
                r = r * (1 - rm) + rc[0] * 0.85f * rm; g = g * (1 - rm) + rc[1] * 0.85f * rm; b = b * (1 - rm) + rc[2] * 0.85f * rm;
            }
            // grain + vignette
            const float gr = 1.0f + 0.012f * (hash01 ((uint32_t) (px * 73856093) ^ (uint32_t) (py * 19349663)) * 2.0f - 1.0f);
            const float vx = (u - 0.5f) * 1.6f, vy = (v - 0.5f) * 1.4f;
            const float vg = gr * (1.0f - 0.20f * (vx * vx + vy * vy));
            pix->setARGB (255, (juce::uint8) (clamp01 (r * vg) * 255.0f + 0.5f), (juce::uint8) (clamp01 (g * vg) * 255.0f + 0.5f),
                          (juce::uint8) (clamp01 (b * vg) * 255.0f + 0.5f));
        }
    }
    };
    // rows are independent: split across cores
    const int nThreads = juce::jlimit (1, 8, (int) std::thread::hardware_concurrency());
    std::vector<std::thread> pool;
    const int rows = y1 - y0;
    for (int t = 1; t < nThreads; ++t)
        pool.emplace_back (shadeRows, y0 + rows * t / nThreads, y0 + rows * (t + 1) / nThreads);
    shadeRows (y0, y0 + rows / nThreads);
    for (auto& th : pool) th.join();
}

//==============================================================================
void PanelArt::drawCrisp (juce::Graphics& g, uint8_t occupied, const std::vector<ScrewSpot>& spots)
{
    using namespace mil;
    juce::ignoreUnused (occupied);
    for (auto& s : spots) screw (g, s.c, s.r, s.angle);

    // title bar
    {
        g.setGradientFill (juce::ColourGradient (C (0.17f, 0.17f, 0.175f), 0, 0, C (0.11f, 0.11f, 0.115f), 0, headerH, false));
        g.fillRect (header);
        g.setColour (C (0, 0, 0));
        g.fillRect (0.0f, headerH - 1.2f, kW, 1.2f);
        g.setColour (C (1, 1, 1, 0.06f));
        g.fillRect (0.0f, 0.0f, kW, 1.0f);
        // flask
        const Pt f (31.0f, 19.0f);
        juce::Path flask;
        flask.startNewSubPath (f.x - 3.5f, f.y - 10.0f);
        flask.lineTo (f.x - 3.5f, f.y - 3.0f);
        flask.lineTo (f.x - 10.0f, f.y + 8.0f);
        flask.quadraticTo (f.x - 11.0f, f.y + 10.5f, f.x - 8.0f, f.y + 10.5f);
        flask.lineTo (f.x + 8.0f, f.y + 10.5f);
        flask.quadraticTo (f.x + 11.0f, f.y + 10.5f, f.x + 10.0f, f.y + 8.0f);
        flask.lineTo (f.x + 3.5f, f.y - 3.0f);
        flask.lineTo (f.x + 3.5f, f.y - 10.0f);
        juce::Path liquid;
        liquid.startNewSubPath (f.x - 6.5f, f.y + 2.5f);
        liquid.lineTo (f.x + 6.5f, f.y + 2.5f);
        liquid.lineTo (f.x + 9.0f, f.y + 7.5f);
        liquid.quadraticTo (f.x + 9.5f, f.y + 9.0f, f.x + 7.5f, f.y + 9.0f);
        liquid.lineTo (f.x - 7.5f, f.y + 9.0f);
        liquid.quadraticTo (f.x - 9.5f, f.y + 9.0f, f.x - 9.0f, f.y + 7.5f);
        liquid.closeSubPath();
        g.setColour (palette::ink.withAlpha (0.85f));
        g.fillPath (liquid);
        g.setColour (palette::ink);
        g.strokePath (flask, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (f.x - 5.5f, f.y - 10.0f, f.x + 5.5f, f.y - 10.0f, 1.6f);
        for (auto [dx, dy] : { std::pair { -2.0f, 5.5f }, std::pair { 2.5f, 6.5f } })
        {
            g.setColour (C (0.13f, 0.13f, 0.14f));
            g.fillEllipse (f.x + dx - 1.1f, f.y + dy - 1.1f, 2.2f, 2.2f);
        }
        g.setFont (stencilFont (20.0f).withExtraKerningFactor (0.02f));
        g.setColour (palette::ink);
        g.drawText ("HARSH VOX", Rc (60.0f, 4.0f, 200.0f, 30.0f), juce::Justification::centredLeft, false);
    }

    // maker's mark (top-left plate)
    {
        const float cap = 14.0f, lw = staticRuinLogoWidth (cap);
        drawStaticRuinLogo (g, topLeftPlate.getCentreX() - lw * 0.5f, 74.0f - cap * 0.5f - 4.0f, cap, false);
        stencil (g, "SIGNAL CORPS PATTERN  -  HV-13", topLeftPlate.getCentreX(), 101.0f, 7.4f, juce::Justification::centred, palette::ink, 0.8f, 0.08f);
    }

    // program LCD bezel
    {
        const auto w = programWin;
        softShadow (g, w, 6.0f, 6, 0.10f, 1.5f, 2.5f);
        g.setColour (C (0, 0, 0, 0.6f));
        g.fillRoundedRectangle (w.expanded (1.0f), 7.0f);
        g.setGradientFill (juce::ColourGradient (C (0.22f, 0.24f, 0.28f), 0, w.getY(), C (0.07f, 0.08f, 0.10f), 0, w.getBottom(), false));
        g.fillRoundedRectangle (w, 6.0f);
        g.setColour (C (0.85f, 0.9f, 1.0f, 0.22f));
        g.drawRoundedRectangle (w.reduced (0.7f), 5.5f, 1.0f);
        g.setColour (C (0.0f, 0.0f, 0.0f, 0.9f));
        g.fillRoundedRectangle (w.reduced (9.0f, 8.0f), 3.5f);
    }

    // latency counter frame + version
    stencil (g, "LATENCY", latencyBox.getCentreX(), latencyBox.getY() - 21.0f, 9.0f, juce::Justification::centred, palette::ink, 1.0f, 0.06f);
    g.setColour (C (0, 0, 0, 0.55f));
    g.fillRoundedRectangle (latencyBox.expanded (2.0f).translated (0.6f, 1.0f), 3.0f);
    g.setColour (C (0.04f, 0.045f, 0.05f));
    g.fillRoundedRectangle (latencyBox, 2.5f);
    stencil (g, "ms", latencyBox.getRight() + 10.0f, latencyBox.getCentreY(), 9.0f, juce::Justification::left);
    tag (g, { latencyBox.getCentreX(), latencyBox.getBottom() + 23.0f }, "v" HV_VERSION_STRING, 8.0f);

    // scopes
    crtBezel (g, scopeL);
    crtBezel (g, scopeR);
    captionTag (g, { scopeL.getCentreX(), scopeL.getBottom() + 12.0f }, "ENVELOPE  /  PITCH");
    captionTag (g, { scopeR.getCentreX(), scopeR.getBottom() + 12.0f }, "TRACT  (LPC)");
}

//==============================================================================
void PanelArt::steelPlate (juce::Image& img, float k)
{
    const auto r = dataPlate;
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (k));
        mil::softShadow (g, r, 4.0f, 8, 0.10f, 2.0f, 3.0f);
        g.setColour (C (0.02f, 0.025f, 0.03f));
        g.fillRoundedRectangle (r.expanded (2.5f), 5.5f);
        g.setGradientFill (juce::ColourGradient (C (0.80f, 0.81f, 0.82f), r.getX(), r.getY(), C (0.60f, 0.61f, 0.62f), r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.5f);
    }
    // brushed grain, grime and scratches
    const auto& T = Textures::get();
    const float inv = 1.0f / k;
    const int x0 = (int) (r.getX() * k), y0 = (int) (r.getY() * k), x1 = (int) (r.getRight() * k), y1 = (int) (r.getBottom() * k);
    juce::Image::BitmapData bd (img, x0, y0, x1 - x0, y1 - y0, juce::Image::BitmapData::readWrite);
    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px)
        {
            auto* pix = reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (px - x0, py - y0));
            const float lx = ((float) px + 0.5f) * inv, ly = ((float) py + 0.5f) * inv;
            if (pix->getAlpha() < 255) continue;
            float cr = pix->getRed() / 255.0f, cg = pix->getGreen() / 255.0f, cb = pix->getBlue() / 255.0f;
            const float brush = T.specks.sample1 (lx * 0.06f, ly * 3.0f, 467.0f) + 0.5f * T.streaks.sample1 (lx * 0.1f, ly * 2.0f, 600.0f);
            float m = 0.90f + 0.12f * brush;
            m *= 1.0f - 0.35f * clamp01 ((T.streaks.sample1 (lx + 200.0f, ly, 300.0f) - 0.2f) * 1.5f) - 0.30f * T.specks.sample1 (lx, ly, 250.0f);
            const float grime = clamp01 ((T.chipsBlur.sample1 (lx + 90.0f, ly + 40.0f, 260.0f) - 0.5f) * 2.0f);
            m *= 1.0f - 0.22f * grime;
            cr *= m; cg *= m; cb *= m;
            const float sc = 0.35f * clamp01 (T.scratches.sample1 (lx, ly, 300.0f) * 1.5f);
            cr += (0.95f - cr) * sc; cg += (0.95f - cg) * sc; cb += (0.96f - cb) * sc;
            pix->setARGB (255, (juce::uint8) (clamp01 (cr) * 255.0f), (juce::uint8) (clamp01 (cg) * 255.0f), (juce::uint8) (clamp01 (cb) * 255.0f));
        }
}

void PanelArt::plateTitle (juce::Image& img, float k)
{
    const auto r = dataPlate;
    const int ox = (int) std::floor (r.getX() * k), oy = (int) std::floor (r.getY() * k);
    juce::Image layer (juce::Image::ARGB, (int) std::ceil (r.getWidth() * k) + 2, (int) std::ceil (r.getHeight() * k) + 2, true, juce::SoftwareImageType());
    const auto ink = C (0.10f, 0.105f, 0.11f), red = C (0.74f, 0.12f, 0.11f);
    {
        juce::Graphics g (layer);
        g.addTransform (juce::AffineTransform::translation (r.getX() - (float) ox / k, r.getY() - (float) oy / k).scaled (k));
        const float cx = r.getWidth() * 0.5f;
        auto line = [&g] (const juce::String& s, float x, float y, float size, juce::Colour c)
        {
            auto f = stencilFont (size).withExtraKerningFactor (0.04f);
            g.setFont (f);
            g.setColour (c);
            g.drawText (s, Rc (x - 200.0f, y - size, 400.0f, size * 2.0f), juce::Justification::centred, false);
        };
        line ("VOCAL DESTRUCTION SET  No. 13", cx, 12.0f, 10.0f, ink);
        line (juce::String ("STATIC RUIN  ") + juce::String::fromUTF8 ("\xe2\x80\xa2") + "  MK II  " + juce::String::fromUTF8 ("\xe2\x80\xa2")
                  + "  SERIAL No. " HV_VERSION_STRING, cx, 73.0f, 9.4f, ink);
        auto word = [&g] (const juce::String& s, Rc box, juce::Colour c)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (condensedFont (60.0f), s, 0.0f, 0.0f);
            juce::Path p;
            ga.createPath (p);
            p.applyTransform (p.getTransformToScaleToFit (box, false));
            g.setColour (c);
            g.fillPath (p);
        };
        word ("HARSH", { 67.0f, 23.5f, 123.0f, 33.0f }, red);
        word ("VOX", { 211.0f, 23.5f, 72.0f, 33.0f }, ink);
        // little flow arrow under the title
        g.setColour (ink);
        g.drawLine (109.0f, 63.5f, 141.0f, 63.5f, 1.4f);
        juce::Path head;
        head.addTriangle (146.0f, 63.5f, 139.0f, 60.3f, 139.0f, 66.7f);
        g.fillPath (head);
    }
    // wear the print: worn through where the plate is chipped and rubbed
    const auto& T = Textures::get();
    const float inv = 1.0f / k;
    juce::Image::BitmapData bd (layer, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < layer.getHeight(); ++y)
        for (int x = 0; x < layer.getWidth(); ++x)
        {
            auto* pix = reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (x, y));
            if (pix->getAlpha() == 0) continue;
            const float lx = ((float) (x + ox) + 0.5f) * inv, ly = ((float) (y + oy) + 0.5f) * inv;
            const float wear = 1.25f * T.specks.sample1 (lx, ly, 180.0f) + 0.9f * T.chips.sample1 (lx + 37.0f, ly + 11.0f, 220.0f)
                             + 0.6f * T.scratches.sample1 (lx, ly, 260.0f);
            const float rel = ly - r.getY();
            const float amount = rel > 22.0f && rel < 58.0f ? 0.92f : 0.45f;   // the big title is worn hardest
            const float keep = 1.0f - amount * clamp01 ((wear - (amount > 0.5f ? 0.62f : 0.8f)) * 3.0f);
            pix->multiplyAlpha (keep);
        }
    juce::Graphics g (img);
    g.setOpacity (0.94f);
    g.drawImageAt (layer, ox, oy);
}

//==============================================================================
juce::Image PanelArt::render (float k, uint8_t occupied)
{
    k = juce::jlimit (0.25f, 3.0f, k);   // beyond 3x physical the cached art is drawn slightly upscaled
    const int w = (int) std::ceil (kW * k), h = (int) std::ceil (kH * k);
    juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    const auto spots = screws (occupied);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (k));
        mil::case_ (g);
        for (auto& r : plates) mil::raisedPlate (g, r);
        for (int i = 0; i < 8; ++i)
        {
            const auto b = bay (i);
            if (occupied & (1u << i)) mil::moduleBody (g, moduleFace (b));
            else mil::blankingPlate (g, blankPlate (b));
        }
    }
    finish (img, k, occupied, spots);
    steelPlate (img, k);
    plateTitle (img, k);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (k));
        drawCrisp (g, occupied, spots);
    }
    return img;
}
} // namespace hv::ui
