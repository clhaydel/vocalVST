// LPC-based modules: Source Drive, Formant Freeze / Swap, Saliva Clicks.
#include "Modules.h"

namespace hv
{
namespace
{
/** Per-channel "get the excitation of my input" helper: dry analysis k, or a local analyzer. */
struct ExcitationTap
{
    PreEmphasis pre;
    LatticeAnalysis inv;
    LpcAnalyzer local;
    const float* k = nullptr;

    void prepare (double sr) { local.prepare (sr, true); }
    void reset() { pre.reset(); inv.reset(); local.reset(); }

    /** Returns pre-emphasised residual; afterwards `k` points at the tract used. */
    inline float process (float x, bool useLocal, const AnalysisFrame& f, int i) noexcept
    {
        if (useLocal)
        {
            const float e = local.processSample (x);
            k = local.currentK();
            return e;
        }
        k = f.kForSample (i);
        return inv.process (pre.process (x), k, f.lpcOrder);
    }
};

/** Short RMS tracker for automatic gain matching. */
struct Rms
{
    float c = 0.01f, ms = 0.0f;
    void prepare (double sr, float timeMs) { c = onePoleCoeff (timeMs, sr); }
    void reset() { ms = 0.0f; }
    inline float process (float x) noexcept { ms += c * (x * x - ms); return std::sqrt (std::max (ms, 0.0f)); }
};
} // namespace

//==============================================================================
// 4.1 Source Drive — distort the excitation, not the finished voice, then re-apply the tract.
class SourceDrive final : public ModuleBase
{
public:
    SourceDrive() : ModuleBase (ModuleType::SourceDrive) {}

    enum { pDrive, pAsym, pRect, pFormant, pReactAmt, pReactThr, pAnalyze };

    void prepareModule() override
    {
        for (auto& c : ch)
        {
            c.tap.prepare (sr);
            c.inRms.prepare (sr, 15.0f);
            c.excRms.prepare (sr, 15.0f);
            c.satRms.prepare (sr, 15.0f);
            c.dc.prepare (sr, 20.0);
            c.post.lowpass (sr, std::min (16000.0, sr * 0.45), 0.707);
        }
        compC = onePoleCoeff (8.0f, sr);
    }
    void resetModule() override
    {
        for (auto& c : ch)
        {
            c.tap.reset(); c.synth.reset(); c.deemph.reset(); c.deemphRaw.reset();
            c.inRms.reset(); c.excRms.reset(); c.satRms.reset(); c.dc.reset(); c.post.reset();
            c.comp = 1.0f;
        }
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame& f) override
    {
        const int n = buf.getNumSamples(), nch = channels (buf);
        const bool local = choice (pAnalyze) == 1;
        const int p = local ? ch[0].tap.local.order() : f.lpcOrder;
        float lastDrive = 0.0f;
        for (int c = 0; c < nch; ++c)
        {
            auto& s = ch[(size_t) c];
            float* x = buf.getWritePointer (c);
            // smoothers are shared across channels: rewind by using per-sample values only on channel 0
            for (int i = 0; i < n; ++i)
            {
                const float driveDb = c == 0 ? (dDrive[i] = next (pDrive)) : dDrive[i];
                const float asym    = c == 0 ? (dAsym[i] = next (pAsym)) : dAsym[i];
                const float rect    = c == 0 ? (dRect[i] = next (pRect) * 0.01f) : dRect[i];
                const float fmix    = c == 0 ? (dFmix[i] = next (pFormant) * 0.01f) : dFmix[i];
                const float rAmt    = c == 0 ? (dRAmt[i] = next (pReactAmt)) : dRAmt[i];
                const float rThr    = raw (pReactThr);

                const float effDb = clampf (driveDb + rAmt * reactDrive (f.envDb[i], rThr) * 24.0f, 0.0f, 54.0f);
                lastDrive = effDb;
                const float g = dbToGain (effDb + 12.0f);   // residual sits well below the voice level

                const float in = x[i];
                const float e  = s.tap.process (in, local, f, i);
                const float tb = std::tanh (asym);
                float sat = fastTanh (g * e + asym) - tb;
                sat = lerp (sat, std::abs (sat), rect);   // full-wave rectifier blend (octave-up grit)
                sat = s.dc.process (sat);

                // automatic level compensation: distorted excitation back to the excitation's level
                const float eR = s.excRms.process (e), sR = s.satRms.process (sat);
                const float target = clampf ((eR + 1.0e-7f) / (sR + 1.0e-7f), 0.0f, 4.0f);
                s.comp += compC * (target - s.comp);
                const float exc = sat * s.comp;

                const float voiced = s.deemph.process (s.synth.process (exc, s.tap.k, p));
                // raw excitation path: matched to the input level
                const float inR  = s.inRms.process (in);
                const float rawE = exc * clampf ((inR + 1.0e-7f) / (eR + 1.0e-7f), 0.0f, 30.0f);
                x[i] = s.post.process (sanitize (lerp (rawE, voiced, fmix)));
            }
        }
        setLive (pDrive, lastDrive);
    }

private:
    struct Ch
    {
        ExcitationTap tap;
        LatticeSynthesis synth;
        DeEmphasis deemph, deemphRaw;
        Rms inRms, excRms, satRms;
        DcBlocker dc;
        Biquad post;
        float comp = 1.0f;
    };
    std::array<Ch, 2> ch;
    float compC = 0.01f;
    float dDrive[kMaxSubBlock] {}, dAsym[kMaxSubBlock] {}, dRect[kMaxSubBlock] {}, dFmix[kMaxSubBlock] {}, dRAmt[kMaxSubBlock] {};
};

//==============================================================================
// 4.4 Formant Freeze / Swap — excitation through a fixed / transformed target tract.
class FormantFreeze final : public ModuleBase
{
public:
    FormantFreeze() : ModuleBase (ModuleType::FormantFreeze) {}

    enum { pTarget, pScale, pPivot, pPipe, pBw, pMix, pReactAmt, pReactThr, pAnalyze };
    enum { tCapture, tAh, tUh, tEe, tOh, tMirror, tScale, tPipe };

    void prepareModule() override
    {
        for (auto& c : ch)
        {
            c.tap.prepare (sr);
            c.inRms.prepare (sr, 20.0f);
            c.outRms.prepare (sr, 20.0f);
            c.tube.prepare ((int) (2.0 * 3.2 / 343.0 * sr) + 16);
        }
        fmSmooth = onePoleCoeff (12.0f, sr);
        gainC    = onePoleCoeff (10.0f, sr);
    }
    void resetModule() override
    {
        for (auto& c : ch)
        {
            c.tap.reset(); c.deemph.reset(); c.inRms.reset(); c.outRms.reset(); c.tube.reset(); c.tubeLp.reset();
            for (auto& r : c.res) r.reset();
            c.gain = 1.0f;
        }
        smoothInit = false;
    }

    static void vowelTable (int t, float* fq, float* bw)
    {
        static const float tbl[4][5] = { { 730, 1090, 2440, 3400, 4500 },   // ah
                                         { 520, 1190, 2390, 3400, 4500 },   // uh
                                         { 270, 2290, 3010, 3500, 4500 },   // ee
                                         { 570, 840, 2410, 3400, 4500 } };  // oh
        static const float bws[5] = { 80, 90, 120, 150, 200 };
        for (int i = 0; i < 5; ++i) { fq[i] = tbl[t][i]; bw[i] = bws[i]; }
    }

    void computeTarget (const Formants& live, float* fq, float* bw)
    {
        const int t = choice (pTarget);
        static const float neutral[5] = { 500, 1500, 2500, 3500, 4500 };
        static const float neutralBw[5] = { 80, 100, 130, 160, 200 };
        Formants lv = live;
        for (int i = lv.count; i < 5; ++i) { lv.freq[i] = neutral[i]; lv.bw[i] = neutralBw[i]; }
        switch (t)
        {
            case tCapture:
            {
                Formants cap = ctx.shared != nullptr ? ctx.shared->getCaptured() : Formants {};
                if (cap.count <= 0) { vowelTable (0, fq, bw); break; }
                for (int i = cap.count; i < 5; ++i) { cap.freq[i] = neutral[i]; cap.bw[i] = neutralBw[i]; }
                for (int i = 0; i < 5; ++i) { fq[i] = cap.freq[i]; bw[i] = cap.bw[i]; }
                break;
            }
            case tAh: case tUh: case tEe: case tOh: vowelTable (t - tAh, fq, bw); break;
            case tMirror:
            {
                const float piv = sm[pPivot].current();
                for (int i = 0; i < 5; ++i) { fq[i] = piv * piv / std::max (lv.freq[i], 50.0f); bw[i] = lv.bw[i]; }
                break;
            }
            case tScale:
            {
                const float s = sm[pScale].current();
                for (int i = 0; i < 5; ++i) { fq[i] = lv.freq[i] * s; bw[i] = lv.bw[i] * std::sqrt (s); }
                break;
            }
            case tPipe:
            default:
            {
                const float L = sm[pPipe].current();
                for (int i = 0; i < 5; ++i)
                {
                    fq[i] = (float) (2 * i + 1) * 343.0f / (4.0f * L);
                    bw[i] = 40.0f + 0.03f * fq[i];
                }
                break;
            }
        }
        const float bwm = sm[pBw].current();
        const float nyq = (float) sr * 0.45f;
        for (int i = 0; i < 5; ++i)
        {
            fq[i] = clampf (fq[i], 60.0f, std::min (nyq, 9000.0f));
            bw[i] = clampf (bw[i] * bwm, 20.0f, 2000.0f);
        }
        // keep ascending order (Mirror reverses it)
        for (int a = 0; a < 5; ++a)
            for (int b = a + 1; b < 5; ++b)
                if (fq[b] < fq[a]) { std::swap (fq[a], fq[b]); std::swap (bw[a], bw[b]); }
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame& f) override
    {
        const int n = buf.getNumSamples(), nch = channels (buf);
        const bool local = choice (pAnalyze) == 1;
        const int p = local ? ch[0].tap.local.order() : f.lpcOrder;
        for (int i = 0; i < kParamsPerSlot; ++i) block (i, n);

        if (ctx.shared != nullptr && ctx.shared->captureRequest.exchange (false))
            ctx.shared->setCaptured (local ? ch[0].tap.local.formants() : f.formants);

        float tf[5], tb[5];
        computeTarget (local ? ch[0].tap.local.formants() : f.formants, tf, tb);
        // smooth formant movement in the log domain (block rate, ~12 ms)
        const float a = 1.0f - std::pow (1.0f - fmSmooth, (float) n);
        for (int i = 0; i < 5; ++i)
        {
            if (! smoothInit) { curF[i] = tf[i]; curB[i] = tb[i]; }
            curF[i] = std::exp (std::log (curF[i]) + a * (std::log (tf[i]) - std::log (curF[i])));
            curB[i] += a * (tb[i] - curB[i]);
        }
        smoothInit = true;
        if (ctx.shared != nullptr)
        {
            for (int i = 0; i < 5; ++i)
            {
                ctx.shared->targetFormants[(size_t) i].store (curF[i], std::memory_order_relaxed);
                ctx.shared->targetFormants[(size_t) i + 5].store (curB[i], std::memory_order_relaxed);
            }
            ctx.shared->targetCount.store (5, std::memory_order_relaxed);
        }

        const float mixBase = sm[pMix].current() * 0.01f;
        const float rAmt = sm[pReactAmt].current(), rThr = raw (pReactThr);
        float lastMix = mixBase;
        for (int c = 0; c < nch; ++c)
        {
            auto& s = ch[(size_t) c];
            const bool pipe = choice (pTarget) == tPipe;
            for (int r = 0; r < 5; ++r) s.res[(size_t) r].formantBand (sr, curF[r], curB[r]);
            // closed tube: inverted-feedback waveguide, resonances at (2n-1) c / 4L across the whole band
            const float pipeDelay = clampf (2.0f * sm[pPipe].current() / 343.0f * (float) sr, 4.0f, (float) s.tube.size() - 8.0f);
            const float wallLoss = clampf (0.97f - 0.06f * std::log2 (sm[pBw].current() * 2.0f), 0.6f, 0.985f);
            s.tubeLp.setCutoff (sr, 5500.0 / std::sqrt (sm[pBw].current()));
            float* x = buf.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float in = x[i];
                float e = s.tap.process (in, local, f, i);
                if (pipe)
                {
                    const float yv = e - wallLoss * s.tubeLp.lp (s.tube.tapLinear (pipeDelay));
                    s.tube.push (sanitize (yv, 32.0f));
                    e = yv;
                }
                else
                {   // parallel formant bank in the pre-emphasised (flat) domain, gentle per-formant tilt
                    float acc = 0.06f * e;
                    for (int r = 0; r < 5; ++r) acc += kFormantGain[r] * s.res[(size_t) r].process (e);
                    e = acc;
                }
                float y = s.deemph.process (e) * 4.0f;
                const float iR = s.inRms.process (in), oR = s.outRms.process (y);
                const float tg = clampf ((iR + 1.0e-6f) / (oR + 1.0e-6f), 0.0f, 32.0f);
                s.gain += gainC * (tg - s.gain);
                y = sanitize (y * s.gain);
                const float mix = clampf (mixBase + rAmt * reactDrive (f.envDb[i], rThr), 0.0f, 1.0f);
                lastMix = mix;
                x[i] = lerp (in, y, mix);
            }
            juce::ignoreUnused (p);
        }
        setLive (pMix, lastMix * 100.0f);
    }

private:
    struct Ch
    {
        ExcitationTap tap;
        std::array<Biquad, 5> res;
        DelayLine tube;
        OnePole tubeLp;
        DeEmphasis deemph;
        Rms inRms, outRms;
        float gain = 1.0f;
    };
    std::array<Ch, 2> ch;
public:
    static constexpr float kFormantGain[5] = { 1.0f, 0.71f, 0.5f, 0.35f, 0.25f };
private:
    float curF[5] {}, curB[5] {};
    bool smoothInit = false;
    float fmSmooth = 0.01f, gainC = 0.01f;
};

//==============================================================================
// 4.12 Saliva Clicks — tiny band-passed noise bursts filtered through the live tract.
// Output is the clicks alone (v0.2): Mix 100% = clicks only, Mix 50% + 6 dB = the old "voice + clicks".
class SalivaClicks final : public ModuleBase
{
public:
    SalivaClicks() : ModuleBase (ModuleType::SalivaClicks) {}
    enum { pDensity, pLen, pLo, pHi, pFric, pLevel };

    void onSeedChanged (uint64_t s) override { rng.seedWith (s, 0x5a11a); nextThresh = rng.exponential (1.0f); }
    void prepareModule() override { envGate = onePoleCoeff (5.0f, sr); }
    void resetModule() override
    {
        for (auto& v : voices) v = {};
        synth.reset(); deemph.reset();
        rateAcc = 0.0f;
        rng.seedWith (lastSeed == ~0ULL ? 1 : lastSeed, 0x5a11a);
        nextThresh = rng.exponential (1.0f);
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame& f) override
    {
        const int n = buf.getNumSamples(), nch = channels (buf);
        const float density = block (pDensity, n), lenMs = block (pLen, n);
        const float lo = block (pLo, n), hi = std::max (lo, block (pHi, n));
        const float fw = block (pFric, n), level = dbToGain (block (pLevel, n));
        float lastRate = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float envAmt = smoothstep (-55.0f, -15.0f, f.envDb[i]);
            const float rate = density * envAmt * (1.0f - fw + fw * 3.0f * f.fricative[i]);
            lastRate = rate;
            // inhomogeneous Poisson process via integrated rate (time-rescaling): exact for a moving rate
            rateAcc += rate / (float) sr;
            if (rateAcc >= nextThresh)
            {
                rateAcc -= nextThresh;
                nextThresh = rng.exponential (1.0f);
                trigger (lenMs, lo, hi);
            }

            float click = 0.0f;
            for (auto& v : voices)
            {
                if (v.remaining <= 0) continue;
                const float t = 1.0f - (float) v.remaining / (float) v.length;
                const float env = std::sin (kPi * t);   // smooth burst window
                click += v.bp.process (rng.bipolar()) * env * v.gain;
                --v.remaining;
            }
            // excite the live tract: scale to the residual level so the click sits "inside the mouth"
            const float g = f.gainForSample (i) * 6.0f + 1.0e-4f;
            const float y = deemph.process (synth.process (click * g, f.kForSample (i), f.lpcOrder));
            const float add = sanitize (y * level);
            for (int c = 0; c < nch; ++c) buf.getWritePointer (c)[i] = add;   // wet = clicks only; slot Mix brings the voice back
        }
        setLive (pDensity, std::min (400.0f, lastRate));
    }

private:
    void trigger (float lenMs, float lo, float hi)
    {
        for (auto& v : voices)
        {
            if (v.remaining > 0) continue;
            v.length = v.remaining = std::max (4, (int) (lenMs * 0.001f * (float) sr * rng.range (0.7f, 1.3f)));
            const float fc = lo * std::pow (hi / lo, rng.uniform());
            v.bp.bandpass (sr, fc, rng.range (1.5f, 4.0f));
            v.bp.reset();
            v.gain = rng.range (0.3f, 1.0f) * 3.0f;
            return;
        }
    }
    struct Voice { Biquad bp; int remaining = 0, length = 1; float gain = 0.0f; };
    std::array<Voice, 12> voices;
    LatticeSynthesis synth;
    DeEmphasis deemph;
    Pcg32 rng;
    float rateAcc = 0.0f, nextThresh = 1.0f, envGate = 0.01f;
};

std::unique_ptr<VocalModule> makeSourceDrive()   { return std::make_unique<SourceDrive>(); }
std::unique_ptr<VocalModule> makeFormantFreeze() { return std::make_unique<FormantFreeze>(); }
std::unique_ptr<VocalModule> makeSalivaClicks()  { return std::make_unique<SalivaClicks>(); }
} // namespace hv
