// Filter / physical-model modules: Cupped Mic, Contact Mic, Membrane Buzz, Modal Rattle.
#include "Modules.h"

namespace hv
{
//==============================================================================
// 4.7 Cupped Mic — proximity shelf, hand-cavity resonances, high cut, capsule saturation, and a
// bounded feedback loop through the same resonances.
class CuppedMic final : public ModuleBase
{
public:
    CuppedMic() : ModuleBase (ModuleType::CuppedMic) {}
    enum { pProx, pHand, pTight, pCapsule, pFb, pFbDelay, pReactAmt, pReactThr };

    void prepareModule() override
    {
        for (auto& c : ch) c.delay.prepare ((int) (0.013 * sr) + kMaxSubBlock);
        fbDelayS.prepare (sr, 30.0f);
    }
    void resetModule() override
    {
        for (auto& c : ch)
        {
            c.shelf.reset(); c.hiShelf.reset(); c.lp.reset(); c.delay.reset(); c.dc.reset();
            for (auto& b : c.peaks) b.reset();
            for (auto& b : c.fbBp) b.reset();
            c.dc.prepare (sr, 25.0);
        }
        fbDelayS.snap (raw (pFbDelay) > 0 ? raw (pFbDelay) : 5.0f);
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame& f) override
    {
        const int n = buf.getNumSamples(), nch = channels (buf);
        const float prox = block (pProx, n), hand = block (pHand, n), tight = block (pTight, n);
        const float capDb = block (pCapsule, n), fbBase = block (pFb, n);
        const float rAmt = block (pReactAmt, n), rThr = raw (pReactThr);
        fbDelayS.setTarget (raw (pFbDelay));

        const double shelfF = 200.0 - 80.0 * hand;          // 120-200 Hz proximity
        const double pf[3] = { 900.0 - 300.0 * hand, 1600.0 - 500.0 * hand, 2500.0 - 700.0 * hand };
        const double pg[3] = { 9.0, 7.0, 5.0 };
        const double Q = 4.0 + 6.0 * tight;
        const double cutDb = -(20.0 + 10.0 * tight);
        const float capG = dbToGain (capDb);
        const float capInv = 1.0f / (std::sqrt (capG) * (1.0f - kCapBias * kCapBias));
        const float makeup = dbToGain (-0.5f * prox - 0.35f * capDb);

        for (int c = 0; c < nch; ++c)
        {
            auto& s = ch[(size_t) c];
            s.shelf.lowShelf (sr, shelfF, prox, 0.8);
            for (int k = 0; k < 3; ++k)
            {
                s.peaks[(size_t) k].peak (sr, pf[k], Q, pg[k]);
                s.fbBp[(size_t) k].bandpass (sr, pf[k], Q);
            }
            s.hiShelf.highShelf (sr, 5000.0, cutDb, 0.9);
            s.lp.lowpass (sr, 9000.0, 0.6);
        }

        float lastFb = fbBase;
        for (int i = 0; i < n; ++i)
        {
            const float fb = clampf (fbBase + rAmt * reactDrive (f.envDb[i], rThr) * 0.3f, 0.0f, 0.995f);
            lastFb = fb;
            const float dSamples = fbDelayS.next() * 0.001f * (float) sr;
            for (int c = 0; c < nch; ++c)
            {
                auto& s = ch[(size_t) c];
                float* x = buf.getWritePointer (c);
                float v = s.shelf.process (x[i]);
                for (auto& p : s.peaks) v = p.process (v);
                v = s.lp.process (s.hiShelf.process (v));
                // feedback path: delayed output through the same resonances, soft-limited in-loop
                const float d = s.delay.tapLinear (dSamples);
                float r = 0.0f;
                for (auto& b : s.fbBp) r += b.process (d);
                r = 0.5f * fastTanh (2.0f * r * 0.6f);
                // normalise by the capsule's small-signal gain so loop gain == fb at the resonances:
                // rings toward howl as fb -> 0.995, but never self-sustains from silence
                const float in = v + fb * r * (1.0f / 0.6f) * capInv;
                // capsule: soft asymmetric clip
                const float y = (fastTanh (capG * in + 0.15f) - kCapBias) / std::sqrt (capG);
                s.delay.push (y);
                x[i] = sanitize (s.dc.process (y) * makeup * 1.1f);
            }
        }
        setLive (pFb, lastFb);
    }

private:
    const float kCapBias = fastTanh (0.15f);   // exact zero for zero input
    struct Ch
    {
        Biquad shelf, hiShelf, lp;
        std::array<Biquad, 3> peaks, fbBp;
        DelayLine delay;
        DcBlocker dc;
    };
    std::array<Ch, 2> ch;
    Smoother fbDelayS;
};

//==============================================================================
// 4.8 Contact / Throat Mic — tissue compression, low-pass, bone resonances, de-esser, flesh IR.
class ContactMic final : public ModuleBase
{
public:
    ContactMic() : ModuleBase (ModuleType::ContactMic) {}
    enum { pTissue, pBone, pClip, pDeess, pBoneFreq };
    static constexpr int kTaps = 72;

    void prepareModule() override
    {
        for (auto& c : ch)
        {
            c.ir.prepare ((int) (0.032 * sr) + 8);
            c.sideHp.highpass (sr, 4000.0, 0.7);
            c.split.setCutoff (sr, 1500.0);
            c.envHi.prepare (sr); c.envHi.setTimes (1.0f, 40.0f);
            c.envAll.prepare (sr); c.envAll.setTimes (1.0f, 40.0f);
        }
        buildIr (lastSeed == ~0ULL ? 11 : lastSeed);
    }
    void resetModule() override
    {
        for (auto& c : ch)
        {
            c.lp1.reset(); c.lp2.reset(); c.bone.reset(); c.mid.reset(); c.ir.reset();
            c.sideHp.reset(); c.split.reset(); c.irLp.reset(); c.envHi.reset(); c.envAll.reset();
        }
    }
    void onSeedChanged (uint64_t s) override { buildIr (s); }

    void buildIr (uint64_t seed)
    {   // velvet-noise approximation of exponentially decaying noise (positions as fraction of length)
        Pcg32 r (seed, 0xf1e5);
        double e = 0.0;
        for (int k = 0; k < kTaps; ++k)
        {
            tapPos[k] = ((float) k + r.uniform()) / (float) kTaps;
            tapGain[k] = (r.uniform() < 0.5f ? -1.0f : 1.0f) * std::exp (-6.9f * tapPos[k]);
            e += (double) tapGain[k] * tapGain[k];
        }
        const float norm = (float) (1.0 / std::sqrt (e));
        for (float& g : tapGain) g *= norm;
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame&) override
    {
        const int n = buf.getNumSamples(), nch = channels (buf);
        const float tissue = block (pTissue, n), bone = block (pBone, n), clipDb = block (pClip, n);
        const float deess = block (pDeess, n) * 0.01f, boneF = block (pBoneFreq, n);
        const double cutoff = 2000.0 - 1000.0 * tissue;
        const float irLenS = (10.0f + 20.0f * tissue) * 0.001f * (float) sr;
        const float g = dbToGain (clipDb);
        const float clipNorm = 1.0f / std::sqrt (g);

        for (int c = 0; c < nch; ++c)
        {
            auto& s = ch[(size_t) c];
            s.lp1.lowpass (sr, cutoff, 0.5412);
            s.lp2.lowpass (sr, cutoff, 1.3066);
            s.bone.peak (sr, boneF, 1.4, bone);
            s.mid.peak (sr, 700.0, 2.0, 4.0);
            s.irLp.setCutoff (sr, 1400.0 - 700.0 * tissue);
            float* x = buf.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float in = x[i];
                float y = fastTanh (g * in) * clipNorm;                // tissue compression
                y = s.lp2.process (s.lp1.process (y));                // 24 dB/oct
                y = s.mid.process (s.bone.process (y));
                // dynamic de-esser keyed from the input's sibilance (consonants vanish into the neck)
                const float hi = s.envHi.process (s.sideHp.process (in));
                const float all = s.envAll.process (in) + 1.0e-6f;
                const float red = deess * smoothstep (0.15f, 0.5f, hi / all);
                const float low = s.split.lp (y);
                y = (low + (y - low) * (1.0f - red)) * (1.0f - 0.75f * red);
                // flesh IR: direct + velvet-noise tail, low-passed
                s.ir.push (y);
                float tail = 0.0f;
                for (int k = 0; k < kTaps; ++k) tail += tapGain[k] * s.ir.tap ((int) (tapPos[k] * irLenS));
                x[i] = sanitize ((y + 0.9f * s.irLp.lp (tail)) * 0.42f);
            }
        }
    }

private:
    struct Ch
    {
        Biquad lp1, lp2, bone, mid, sideHp;
        OnePole split, irLp;
        DelayLine ir;
        EnvelopeFollower envHi, envAll;
    };
    std::array<Ch, 2> ch;
    float tapPos[kTaps] {}, tapGain[kTaps] {};
};

//==============================================================================
// 4.10 Membrane Buzz — voice-driven membrane resonator slapping a one-sided stop.
class MembraneBuzz final : public ModuleBase
{
public:
    MembraneBuzz() : ModuleBase (ModuleType::MembraneBuzz) {}
    enum { pFreq, pTension, pTone, pWidth, pLevel };

    void prepareModule() override
    {
        for (auto& c : ch) { c.env.prepare (sr); c.env.setTimes (0.5f, 15.0f); }
    }
    void resetModule() override
    {
        for (auto& c : ch) { c.mem.reset(); c.bp1.reset(); c.bp2.reset(); c.env.reset(); c.dc.reset(); c.dc.prepare (sr, 40.0); }
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame&) override
    {
        const int n = buf.getNumSamples(), nch = channels (buf);
        const float fm = block (pFreq, n), tension = block (pTension, n), tone = block (pTone, n);
        const float width = block (pWidth, n), level = dbToGain (block (pLevel, n));
        const double bwPow = std::pow (2.0, (double) width);
        const double q = std::sqrt (bwPow) / (bwPow - 1.0);
        const float stopFrac = lerp (0.9f, 0.04f, tension);

        for (int c = 0; c < nch; ++c)
        {
            auto& s = ch[(size_t) c];
            s.mem.bandpass (sr, fm, 2.5);
            s.bp1.bandpass (sr, tone, q);
            s.bp2.bandpass (sr, tone, q);
            float* x = buf.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float in = x[i];
                const float m = s.mem.process (in * 2.0f);
                const float e = s.env.process (m);
                const float stop = stopFrac * e;
                const float slapped = std::min (m, stop);      // only the positive excursion hits the rim
                const float buzz = s.bp2.process (s.bp1.process (s.dc.process (slapped))) * 4.0f;
                x[i] = sanitize (in + buzz * level);
            }
        }
    }

private:
    struct Ch { Biquad mem, bp1, bp2; EnvelopeFollower env; DcBlocker dc; };
    std::array<Ch, 2> ch;
};

//==============================================================================
// 4.9 Modal Rattle — bank of two-pole modal resonators (SoA, auto-vectorised) with collision
// nonlinearity on mode groups; Spring material uses a dispersive allpass loop instead.
class ModalRattle final : public ModuleBase
{
public:
    ModalRattle() : ModuleBase (ModuleType::ModalRattle) {}
    enum { pMaterial, pSize, pDecay, pBright, pGap, pRattle, pSpread, pModes, pDrive };
    static constexpr int kMaxModes = 64, kGroups = 4, kSpringStages = 32;

    bool producesStereo() const override { return true; }

    void prepareModule() override
    {
        static const Tables tables = [] { Tables t; t.build(); return t; }();   // computed once per process
        std::copy (std::begin (tables.plate), std::end (tables.plate), plate);
        std::copy (std::begin (tables.drum), std::end (tables.drum), drum);
        std::copy (std::begin (tables.pipe), std::end (tables.pipe), pipe);
        hp1.highpass (sr, 180.0, 0.7071);
        hp2.highpass (sr, 180.0, 0.7071);
        for (auto& sp : spring) sp.delay.prepare ((int) (0.25 * sr));
        clickLen = std::max (4, (int) (0.001 * sr));
        lastCoeffKey = -1.0f;
    }
    void resetModule() override
    {
        std::fill (std::begin (y1), std::end (y1), 0.0f);
        std::fill (std::begin (y2), std::end (y2), 0.0f);
        for (auto& g : groups) g = {};
        hp1.reset(); hp2.reset();
        for (auto& sp : spring)
        {
            sp.delay.reset(); sp.lp.reset();
            std::fill (std::begin (sp.ap), std::end (sp.ap), 0.0f);
        }
        rng.seedWith (lastSeed == ~0ULL ? 5 : lastSeed, 0x7a771e);
        lastCoeffKey = -1.0f;
    }
    void onSeedChanged (uint64_t s) override { rng.seedWith (s, 0x7a771e); }

    struct Tables
    {
        float plate[kMaxModes] {}, drum[kMaxModes] {}, pipe[kMaxModes] {};
        void build();
    };

    void updateCoefficients (int material, float size, float decay, float bright, int modes)
    {
        const float* ratios = material == 0 ? plate : material == 1 ? drum : pipe;
        const float base = (material == 2 ? 110.0f : 150.0f) / size;
        const float tilt = lerp (1.3f, 0.15f, bright);
        Pcg32 r (lastSeed, 0x30de5);
        numModes = 0;
        for (int i = 0; i < modes && i < kMaxModes; ++i)
        {
            const float fq = base * ratios[i];
            if (fq > 0.45f * (float) sr || fq > 16000.0f) break;
            const float t60 = decay * std::pow (ratios[0] / ratios[i], tilt);
            const float rad = std::exp (-6.9078f / (std::max (0.01f, t60) * (float) sr));
            const float w = kTwoPi * fq / (float) sr;
            c1[i] = 2.0f * rad * std::cos (w);
            c2[i] = -rad * rad;
            // input gain: normalise peak gain (1-r) and give each mode a fixed seeded amplitude
            // (1 - r) * 2 sin(w) gives each mode ~unity peak gain regardless of frequency / decay
            gain[i] = (1.0f - rad) * 2.0f * std::sin (w) * r.range (0.5f, 1.0f) * (r.uniform() < 0.5f ? -1.0f : 1.0f) * 2.5f;
            ++numModes;
        }
        for (int i = numModes; i < kMaxModes; ++i) { c1[i] = c2[i] = gain[i] = 0.0f; }
        // spring: dispersion + loop gain from decay
        for (int k = 0; k < 2; ++k)
        {
            auto& sp = spring[(size_t) k];
            sp.delaySamples = std::clamp ((0.035f + 0.004f * (float) k) * size * (float) sr, 16.0f, 0.24f * (float) sr);
            const float loopT = sp.delaySamples / (float) sr;
            sp.fb = std::pow (10.0f, -3.0f * loopT / std::max (0.05f, decay));
            sp.lp.setCutoff (sr, 1500.0 + 9000.0 * bright);
        }
    }

    inline float collide (int gi, float y, float gap, float k, float clickAmt) noexcept
    {
        auto& g = groups[(size_t) gi];
        const float a = std::abs (y);
        float out = y;
        if (a > gap)
        {
            out = (y > 0 ? 1.0f : -1.0f) * (gap - k * (a - gap));
            if (! g.above) { g.clickLeft = clickLen; g.clickAmp = clickAmt * std::min (1.0f, a); }
            g.above = true;
        }
        else g.above = false;
        if (g.clickLeft > 0)
        {
            const float env = (float) g.clickLeft / (float) clickLen;
            out += rng.bipolar() * g.clickAmp * env * env;
            --g.clickLeft;
        }
        return out;
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame&) override
    {
        const int n = buf.getNumSamples();
        const int material = choice (pMaterial);
        const float size = block (pSize, n), decay = block (pDecay, n), bright = block (pBright, n);
        const float gap = block (pGap, n), rattle = block (pRattle, n), spread = block (pSpread, n);
        const float drive = dbToGain (block (pDrive, n));
        const int modes = (int) raw (pModes);
        const float key = (float) material * 1.0e6f + size * 1.0e4f + decay * 100.0f + bright * 7.0f + (float) modes * 0.37f;
        if (key != lastCoeffKey) { updateCoefficients (material, size, decay, bright, modes); lastCoeffKey = key; }
        const float k = 0.15f + 0.8f * rattle, clickAmt = 0.6f * rattle;

        float* L = buf.getWritePointer (0);
        float* R = buf.getWritePointer (1);
        const bool inStereo = ctx.numChannels > 1;
        const float wMain = 1.0f, wCross = 1.0f - spread;
        const float norm = 3.2f / (1.0f + wCross);

        for (int i = 0; i < n; ++i)
        {
            const float in = (inStereo ? 0.5f * (L[i] + R[i]) : L[i]);
            const float x = hp2.process (hp1.process (in)) * drive;
            float gs[kGroups];
            if (material == 3)
            {
                for (int s = 0; s < 2; ++s)
                {
                    auto& sp = spring[(size_t) s];
                    float v = x + sp.delay.tapLinear (sp.delaySamples) * sp.fb;
                    for (int a = 0; a < kSpringStages; ++a)
                    {   // first-order allpass chain (a = -0.62): strong low-frequency dispersion "chirp"
                        const float yv = -0.62f * v + sp.ap[a];
                        sp.ap[a] = v + 0.62f * yv;
                        v = yv;
                    }
                    v = sp.lp.lp (v);
                    const float c = collide (s, v, gap * 2.0f, k, clickAmt);
                    sp.delay.push (sanitize (c, 4.0f));
                    gs[s] = c; gs[s + 2] = 0.0f;
                }
            }
            else
            {
                float acc[kGroups] = {};
                for (int m = 0; m < kMaxModes; ++m)      // SoA loop: vectorises across modes
                {
                    const float y = c1[m] * y1[m] + c2[m] * y2[m] + gain[m] * x;
                    y2[m] = y1[m]; y1[m] = y;
                }
                for (int m = 0; m < numModes; ++m) acc[m & (kGroups - 1)] += y1[m];
                for (int gi = 0; gi < kGroups; ++gi)
                {
                    gs[gi] = collide (gi, acc[gi], gap, k, clickAmt);
                    if (groups[(size_t) gi].above)
                    {   // contact steals energy from the colliding part
                        const float damp = 1.0f - 0.02f * rattle;
                        for (int m = gi; m < numModes; m += kGroups) { y1[m] *= damp; y2[m] *= damp; }
                    }
                }
            }
            const float even = gs[0] + gs[2], odd = gs[1] + gs[3];
            L[i] = sanitize ((wMain * even + wCross * odd) * norm);
            R[i] = sanitize ((wMain * odd + wCross * even) * norm);
        }
    }

private:
    float plate[kMaxModes] {}, drum[kMaxModes] {}, pipe[kMaxModes] {};
    alignas (32) float c1[kMaxModes] {}, c2[kMaxModes] {}, gain[kMaxModes] {}, y1[kMaxModes] {}, y2[kMaxModes] {};
    int numModes = 0;
    struct Group { bool above = false; int clickLeft = 0; float clickAmp = 0.0f; };
    std::array<Group, kGroups> groups {};
    struct Spring { DelayLine delay; float delaySamples = 100.0f, fb = 0.5f; OnePole lp; float ap[kSpringStages] {}; };
    std::array<Spring, 2> spring;
    Biquad hp1, hp2;
    Pcg32 rng;
    int clickLen = 48;
    float lastCoeffKey = -1.0f;
};

void ModalRattle::Tables::build()
{
    // Plate: given low ratios, then a rectangular-plate (m^2 + g^2 n^2) series continuing upward.
    {
        std::vector<double> v;
        const double gam = 1.37;
        for (int m = 1; m <= 12; ++m)
            for (int k = 1; k <= 12; ++k) v.push_back (m * m + gam * gam * k * k);
        std::sort (v.begin(), v.end());
        const double base = v[0];
        static const double given[5] = { 1.0, 2.16, 3.91, 5.4, 6.27 };
        int idx = 0;
        for (; idx < 5; ++idx) plate[idx] = (float) given[idx];
        for (double r : v)
        {
            if (idx >= kMaxModes) break;
            const double rr = r / base * 1.18;
            if (rr > plate[idx - 1] + 0.05) plate[idx++] = (float) rr;
        }
    }
    // Drum: zeros of Bessel functions J_m (circular membrane), normalised to j_{0,1}.
    {
        std::vector<double> z;
        for (int m = 0; m <= 24; ++m)
        {
            double prev = std::cyl_bessel_j ((double) m, 0.5);
            int found = 0;
            for (double xx = 0.55; xx < 60.0 && found < 8; xx += 0.05)
            {
                const double cur = std::cyl_bessel_j ((double) m, xx);
                if ((prev < 0) != (cur < 0))
                {
                    double a = xx - 0.05, b = xx;
                    for (int it = 0; it < 40; ++it)
                    {
                        const double mid = 0.5 * (a + b);
                        if ((std::cyl_bessel_j ((double) m, a) < 0) != (std::cyl_bessel_j ((double) m, mid) < 0)) b = mid; else a = mid;
                    }
                    z.push_back (0.5 * (a + b));
                    ++found;
                }
                prev = cur;
            }
        }
        std::sort (z.begin(), z.end());
        for (int i = 0; i < kMaxModes; ++i) drum[i] = (float) (z[(size_t) i] / z[0]);
    }
    for (int i = 0; i < kMaxModes; ++i) pipe[i] = (float) (2 * i + 1);
}

std::unique_ptr<VocalModule> makeCuppedMic()    { return std::make_unique<CuppedMic>(); }
std::unique_ptr<VocalModule> makeContactMic()   { return std::make_unique<ContactMic>(); }
std::unique_ptr<VocalModule> makeMembraneBuzz() { return std::make_unique<MembraneBuzz>(); }
std::unique_ptr<VocalModule> makeModalRattle()  { return std::make_unique<ModalRattle>(); }
} // namespace hv
