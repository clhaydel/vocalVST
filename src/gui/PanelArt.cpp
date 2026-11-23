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

const Rc kPlates[] = { masterPlate, chainPlate, detailPlate };
} // namespace

//==============================================================================
std::vector<PanelArt::ScrewSpot> PanelArt::screws (uint8_t occupied)
{
    std::vector<ScrewSpot> s;
    auto add = [&s] (Pt c, float r, bool canRust)
    {
        const auto i = (uint32_t) s.size() + 17u;
        s.push_back ({ c, r, hash01 (i * 31u) * juce::MathConstants<float>::pi, canRust && hash01 (i * 7u + 3u) < 0.55f });
    };
    const auto& p = panel;
    for (auto c : { Pt (p.getX() + 12, p.getY() + 12), Pt (p.getRight() - 12, p.getY() + 12), Pt (p.getX() + 12, p.getBottom() - 12),
                    Pt (p.getRight() - 12, p.getBottom() - 12), Pt (p.getCentreX(), p.getBottom() - 10), Pt (p.getX() + 12, p.getCentreY()),
                    Pt (p.getRight() - 12, p.getCentreY()) })
        add (c, 5.0f, true);
    for (auto& r : kPlates)
        for (auto c : { r.getTopLeft().translated (8, 8), r.getTopRight().translated (-8, 8), r.getBottomLeft().translated (8, -8), r.getBottomRight().translated (-8, -8) })
            add (c, 3.4f, true);
    for (int i = 0; i < 8; ++i)
        if ((occupied & (1u << i)) == 0)
        {
            const auto b = bay (i);
            for (auto c : { Pt (b.getX() + 15, b.getY() + 16), Pt (b.getRight() - 15, b.getY() + 16), Pt (b.getX() + 15, b.getBottom() - 18),
                            Pt (b.getRight() - 15, b.getBottom() - 18) })
                add (c, 3.4f, true);
        }
    add ({ programWin.getX() - 14.0f, programWin.getCentreY() }, 3.6f, false);
    add ({ programWin.getRight() + 14.0f, programWin.getCentreY() }, 3.6f, false);
    return s;
}

//==============================================================================
void PanelArt::finish (juce::Image& img, float k, uint8_t occupied, const std::vector<ScrewSpot>& spots)
{
    const auto& T = Textures::get();
    const int w = img.getWidth(), h = img.getHeight();
    const int x0 = std::max (0, (int) (panel.getX() * k)), y0 = std::max (0, (int) (panel.getY() * k));
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
    edgeBand (panel, 1.0f, 9.0f);
    for (auto& r : kPlates) edgeBand (r, 0.8f, 5.0f);
    for (int i = 0; i < 8; ++i) if (occupied & (1u << i)) edgeBand (moduleFace (bay (i)), 0.9f, 3.5f);

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
            mult *= 1.0f - 0.32f * clamp01 ((st - 0.15f) * 1.6f) - 0.22f * sp;
            // uneven light; dust settles toward the bottom
            mult *= (1.08f - 0.18f * (colLin[(size_t) px] + rowLin) + 0.06f * colHot[(size_t) px] * rowHot) * rowDust;
            r *= mult; g *= mult; b *= mult;

            const size_t i = idx (px, py);
            const float handW = (float) hands[i] * (1.0f / 255.0f);
            const float wr = clamp01 ((float) wear[i] * (1.0f / 255.0f) + handW);
            // 4. chips through to bare metal, with darker rims
            const float ch = T.chips.sample1 (lx, ly, 373.0f), cb = T.chipsBlur.sample1 (lx, ly, 373.0f);
            const float f = 0.35f + 1.4f * wr;
            const float chip = clamp01 ((ch * f - 0.42f) * 3.0f);
            const float rim = clamp01 (clamp01 ((cb * f - 0.42f) * 3.0f) - chip);
            const float bareK = 0.85f + 0.25f * ch;
            r = r * (1 - chip) + 0.66f * bareK * chip;
            g = g * (1 - chip) + 0.65f * bareK * chip;
            b = b * (1 - chip) + 0.61f * bareK * chip;
            const float rimK = 1.0f - 0.55f * rim;
            r *= rimK; g *= rimK; b *= rimK;
            // 5. hairline scratches
            const float sc = 0.30f * clamp01 (T.scratches.sample1 (lx, ly, 533.0f) * 1.4f);
            r += (0.66f - r) * sc; g += (0.65f - g) * sc; b += (0.61f - b) * sc;
            // keep the enamel khaki (not the bare chips)
            const float lum = 0.299f * r + 0.587f * g + 0.114f * b, sat = 1.0f + 0.28f * (1.0f - chip);
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
    // carry handles on the case sides
    for (float hx : { 7.0f, kW - 7.0f })
        for (auto [ya, yb] : { std::pair { 150.0f, 320.0f }, std::pair { 440.0f, 610.0f } })
        {
            g.setColour (C (0, 0, 0, 0.4f));
            g.fillRoundedRectangle (hx - 2.0f, ya + 3.0f, 7.0f, yb - ya, 3.0f);
            g.setGradientFill (juce::ColourGradient (C (0.45f, 0.45f, 0.42f), hx - 3.5f, 0, C (0.30f, 0.30f, 0.28f), hx + 3.5f, 0, false));
            g.fillRoundedRectangle (hx - 3.0f, ya, 6.0f, yb - ya, 3.0f);
            g.setColour (C (0.95f, 0.95f, 0.9f, 0.85f));
            g.fillRect (hx - 1.4f, ya + 2.0f, 1.2f, yb - ya - 4.0f);
            for (float yy : { ya, yb })
            {
                const float bx = hx < kW * 0.5f ? hx - 4.0f : hx - 14.0f;
                g.setGradientFill (juce::ColourGradient (C (0.8f, 0.8f, 0.75f), bx, yy - 6, C (0.3f, 0.3f, 0.27f), bx + 18, yy + 6, false));
                g.fillRoundedRectangle (bx, yy - 6.0f, 18.0f, 12.0f, 3.0f);
                screw (g, { hx < kW * 0.5f ? hx + 8.0f : hx - 8.0f, yy }, 2.6f, 0.9f);
            }
        }
    for (auto& s : spots) screw (g, s.c, s.r, s.angle);
    // signal-flow arrows between bays
    g.setColour (palette::ink.withAlpha (0.8f));
    for (int i = 0; i < 7; ++i)
    {
        const auto b = bay (i);
        const float ax = b.getRight() + bayGap * 0.5f, ay = b.getCentreY();
        juce::Path tri;
        tri.addTriangle (ax - 2.0f, ay - 4.0f, ax + 2.5f, ay, ax - 2.0f, ay + 4.0f);
        g.fillPath (tri);
    }
    juce::ignoreUnused (occupied);

    // data plate (engraved black anodised, riveted)
    {
        const auto r = dataPlate;
        softShadow (g, r, 3.0f, 8, 0.07f);
        g.setColour (C (0, 0, 0, 0.5f));
        g.fillRoundedRectangle (r.translated (2, 3), 3.0f);
        g.setGradientFill (juce::ColourGradient (C (0.12f, 0.12f, 0.11f), 0, r.getY(), C (0.05f, 0.05f, 0.05f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        for (int i = 0; i < (int) r.getHeight(); ++i)
        {
            g.setColour (C (1, 1, 1, 0.018f + 0.015f * hash01 ((uint32_t) i * 977u)));
            g.fillRect (r.getX(), r.getY() + (float) i, r.getWidth(), 0.5f);
        }
        g.setColour (C (0.75f, 0.73f, 0.66f, 0.8f));
        g.drawRoundedRectangle (r.reduced (4.0f), 2.0f, 0.8f);
        for (auto c : { r.getTopLeft().translated (9, 9), r.getTopRight().translated (-9, 9), r.getBottomLeft().translated (9, -9), r.getBottomRight().translated (-9, -9) })
        {
            g.setGradientFill (juce::ColourGradient (C (0.95f, 0.95f, 0.9f), c.x - 1, c.y - 1, C (0.35f, 0.35f, 0.32f), c.x + 2.5f, c.y + 2.5f, true));
            g.fillEllipse (c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
        }
        stencil (g, "VOCAL DESTRUCTION SET  No. 13", r.getCentreX(), r.getY() + 18.0f, 8.6f, juce::Justification::centred, palette::silver, 1.0f, 0.1f);
        const float cap = 22.0f, tw = BlockFont::width ("HARSH VOX", cap);
        float x = r.getCentreX() - tw * 0.5f;
        x += BlockFont::draw (g, "HARSH", x, r.getY() + 30.0f, cap, palette::red);
        BlockFont::draw (g, "VOX", x + cap * 6.0f / 7.0f, r.getY() + 30.0f, cap, palette::silver);
        g.setFont (tagFont (7.6f));
        g.setColour (palette::silver);
        g.drawText ("STATIC RUIN    MK II    SERIAL No " HV_VERSION_STRING, Rc (r.getX(), r.getY() + 66.0f, r.getWidth(), 12.0f), juce::Justification::centred, false);
    }

    // maker's mark
    {
        const float lw = staticRuinLogoWidth (logoCap);
        const float lx = CR - 8.0f - lw;
        drawStaticRuinLogo (g, lx, logoY, logoCap, false);
        stencil (g, "SIGNAL CORPS PATTERN  -  HV-13", lx + lw * 0.5f + 20.0f, 72.0f, 7.4f, juce::Justification::centred, palette::ink, 0.85f, 0.12f);
        stencil (g, "POWER", powerLamp.x + 13.0f, powerLamp.y, 7.2f, juce::Justification::left, palette::ink, 1.0f, 0.06f);
        stencil (g, "LIMIT", limitLamp.x + 13.0f, limitLamp.y, 7.2f, juce::Justification::left, palette::ink, 1.0f, 0.06f);
    }

    // program window bezel
    {
        const auto w = programWin;
        stencil (g, "PROGRAM", w.getCentreX(), w.getY() - 17.0f, 8.8f, juce::Justification::centred, palette::ink, 1.0f, 0.2f);
        g.setColour (C (0, 0, 0, 0.5f));
        g.fillRoundedRectangle (w.expanded (6.0f).translated (2.0f, 3.0f), 6.0f);
        g.setGradientFill (juce::ColourGradient (C (0.86f, 0.85f, 0.80f), w.getX(), w.getY(), C (0.30f, 0.30f, 0.27f), w.getRight(), w.getBottom(), false));
        g.fillRoundedRectangle (w.expanded (6.0f), 6.0f);
        g.setColour (C (0.05f, 0.05f, 0.04f));
        g.fillRoundedRectangle (w.expanded (1.0f), 3.5f);
    }

    sectionTag (g, masterPlate.getX(), masterPlate.getY(), "MASTER CONTROL");
    sectionTag (g, chainPlate.getX(), chainPlate.getY(), "SIGNAL CHAIN  -  MODULE BAYS 01-08");

    // latency counter frame + version
    stencil (g, "LATENCY", latencyBox.getCentreX(), latencyBox.getY() - 10.0f, 7.8f, juce::Justification::centred, palette::ink, 1.0f, 0.1f);
    g.setColour (C (0.05f, 0.05f, 0.05f));
    g.fillRoundedRectangle (latencyBox, 2.0f);
    stencil (g, "ms", latencyBox.getRight() + 4.0f, latencyBox.getCentreY(), 7.8f, juce::Justification::left);
    tag (g, { latencyBox.getCentreX(), latencyBox.getBottom() + 18.0f }, "v" HV_VERSION_STRING, 7.4f);

    // scopes
    crtBezel (g, scopeL);
    crtBezel (g, scopeR);
    stencil (g, "ENVELOPE  /  PITCH", scopeL.getCentreX(), scopeL.getBottom() + 10.0f, 8.2f, juce::Justification::centred, palette::ink, 1.0f, 0.12f);
    stencil (g, "TRACT  (LPC)", scopeR.getCentreX(), scopeR.getBottom() + 10.0f, 8.2f, juce::Justification::centred, palette::ink, 1.0f, 0.12f);
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
        for (auto& r : kPlates) mil::raisedPlate (g, r);
        for (int i = 0; i < 8; ++i)
        {
            const auto b = bay (i);
            mil::recess (g, b);
            if (occupied & (1u << i)) mil::moduleBody (g, moduleFace (b));
            else mil::blankingPlate (g, blankPlate (b));
        }
    }
    finish (img, k, occupied, spots);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (k));
        drawCrisp (g, occupied, spots);
    }
    return img;
}
} // namespace hv::ui
