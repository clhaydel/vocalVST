// Mob (static crowd) and Organic Carrier Vocoder.
#include "Modules.h"

namespace hv
{
//==============================================================================
// 4.13 Mob — 4-16 PSOLA copies with fixed seeded delay / pitch / formant / pan and fast micro-jitter.
class Mob final : public ModuleBase
{
public:
    Mob() : ModuleBase (ModuleType::Mob) {}
    enum { pVoices, pSpread, pDelay, pBody, pThr, pGateRange, pSeed, pPitch };
    static constexpr int kMaxVoices = 16, kMarkCap = 4096;

    bool producesStereo() const override { return true; }

    void prepareModule() override
    {
        int sz = 1;
        while (sz < (int) (0.5 * sr) + 4 * kMaxSubBlock) sz <<= 1;
        mask = sz - 1;
        in.assign ((size_t) sz, 0.0f);
        envRing.assign ((size_t) sz, -120.0f);
        accL.assign ((size_t) sz, 0.0f);
        accR.assign ((size_t) sz, 0.0f);
        lookahead = (int) (0.012 * sr);
        maxHalf   = (int) (0.008 * sr);
        pseudoHop = (int) (0.005 * sr);
        gateAtk = 1.0f - std::exp (-1.0f / (0.010f * (float) sr));
        gateRel = 1.0f - std::exp (-1.0f / (0.080f * (float) sr));
    }
    void resetModule() override
    {
        std::fill (in.begin(), in.end(), 0.0f);
        std::fill (envRing.begin(), envRing.end(), -120.0f);
        std::fill (accL.begin(), accL.end(), 0.0f);
        std::fill (accR.begin(), accR.end(), 0.0f);
        markCount = 0; markHead = 0;
        lastMark = std::numeric_limits<int64_t>::min() / 2;
        lastEpochSeen = std::numeric_limits<int64_t>::min();
        started = false;
        configKey = -1.0f;
    }

    void configure (uint64_t seed, int voices, float spread, float delayMax, float body, float thr, float gateRange, float pitchRange)
    {
        Pcg32 r (seed, 0x30b);
        for (int v = 0; v < kMaxVoices; ++v)
        {
            auto& vc = voice[(size_t) v];
            const float uD = r.uniform(), uP = r.uniform(), uS = r.uniform(), uF = r.uniform(), uPan = r.uniform();
            vc.delay = (int) ((30.0f + uD * (delayMax - 30.0f) * std::max (spread, 0.15f)) * 0.001f * (float) sr);
            const float semis = (uS < 0.5f ? -1.0f : 1.0f) * lerp (1.0f, pitchRange, uP) * spread;
            vc.pitch = std::pow (2.0f, semis / 12.0f);
            vc.formant = clampf (1.0f + (uF * 2.0f - 1.0f) * 0.4f * body * spread, 0.7f, 1.4f);
            const float pan = (uPan * 2.0f - 1.0f) * std::min (1.0f, spread * 1.3f);
            vc.gl = std::cos ((pan + 1.0f) * 0.25f * kPi);
            vc.gr = std::sin ((pan + 1.0f) * 0.25f * kPi);
            vc.thr = thr + gateRange * (voices > 1 ? (float) v / (float) (voices - 1) : 0.0f);
            vc.jitRate = r.range (3.0f, 8.0f);
            vc.rng.seedWith (r.next(), (uint64_t) v + 1);
        }
        activeVoices = voices;
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame& f) override
    {
        const int n = buf.getNumSamples();
        const int64_t t0 = f.startTime, tNow = t0 + n - 1;
        const int voices = std::clamp ((int) raw (pVoices), 4, kMaxVoices);
        const float spread = block (pSpread, n), delayMax = block (pDelay, n), body = block (pBody, n);
        const float thr = block (pThr, n), gateRange = block (pGateRange, n), pitchRange = block (pPitch, n);
        const uint64_t seed = mixSeed (ctx.seed, (uint64_t) raw (pSeed));
        const float key = (float) (seed % 100003) + (float) voices * 7.0f + spread * 131.0f + delayMax * 0.37f + body * 17.0f
                          + thr * 0.11f + gateRange * 0.013f + pitchRange * 3.3f;
        if (key != configKey) { configure (seed, voices, spread, delayMax, body, thr, gateRange, pitchRange); configKey = key; }

        const bool inStereo = ctx.numChannels > 1;
        float* L = buf.getWritePointer (0);
        float* R = buf.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const auto h = (size_t) ((t0 + i) & mask);
            in[h] = inStereo ? 0.5f * (L[i] + R[i]) : L[i];
            envRing[h] = f.envDb[i];
        }
        if (! started)
        {
            started = true;
            for (auto& vc : voice) { vc.nextMark = t0 + lookahead; vc.gate = 0.0f; vc.jitCents = 0.0f; vc.jitTarget = 0.0f; vc.jitLeft = 0; }
        }
        updateMarks (f, tNow);

        // place grains for every voice up to tNow + lookahead
        float lastGates = 0.0f;
        for (int v = 0; v < activeVoices; ++v)
        {
            auto& vc = voice[(size_t) v];
            int guard = 0;
            while (vc.nextMark <= tNow + lookahead && guard++ < 64)
            {
                const int64_t s = vc.nextMark - vc.delay;
                Mark m;
                if (! nearestMark (s, tNow, m)) { vc.nextMark += pseudoHop; continue; }
                const float T = std::max (16.0f, m.period);
                const int W = std::clamp ((int) T, 16, maxHalf);
                // fast micro-jitter: +-15 cents, new random target at 3-8 Hz, smoothed per grain
                vc.jitLeft -= (int) T;
                if (vc.jitLeft <= 0) { vc.jitTarget = vc.rng.bipolar() * 15.0f; vc.jitLeft = (int) ((float) sr / vc.jitRate); }
                vc.jitCents += 0.25f * (vc.jitTarget - vc.jitCents);
                const float p = vc.pitch * std::pow (2.0f, vc.jitCents / 1200.0f);
                // per-copy gate from the envelope at the source position
                const float envDb = envRing[(size_t) (m.time & mask)];
                const float gt = envDb > vc.thr ? 1.0f : 0.0f;
                const float coef = gt > vc.gate ? gateAtk : gateRel;
                vc.gate += (1.0f - std::pow (1.0f - coef, T / p)) * (gt - vc.gate);
                lastGates += vc.gate;

                if (vc.gate > 1.0e-3f)
                {
                    const float phi = vc.formant;
                    const int Wout = (int) ((float) W / phi);
                    const float amp = vc.gate * (phi * T) / ((float) W * p) * 0.5f;
                    for (int k = -Wout; k <= Wout; ++k)
                    {
                        const int64_t to = vc.nextMark + k;
                        if (to < t0) continue;
                        const float srcOff = (float) k * phi;
                        const float w = 0.5f + 0.5f * std::cos (kPi * srcOff / (float) (W + 1));
                        const float sp = (float) m.time + srcOff;
                        const auto i0 = (int64_t) std::floor (sp);
                        const float fr = sp - (float) i0;
                        const float a = in[(size_t) (i0 & mask)], b = in[(size_t) ((i0 + 1) & mask)];
                        const float smp = (a + fr * (b - a)) * w * amp;
                        accL[(size_t) (to & mask)] += smp * vc.gl;
                        accR[(size_t) (to & mask)] += smp * vc.gr;
                    }
                }
                vc.nextMark += std::max<int64_t> (8, (int64_t) std::lround (T / p));
            }
        }
        if (ctx.liveNorm != nullptr) ctx.liveNorm[pThr] = clampf (lastGates / (float) std::max (1, activeVoices), 0.0f, 1.0f);

        const float norm = 2.9f / std::sqrt ((float) activeVoices);
        for (int i = 0; i < n; ++i)
        {
            const auto h = (size_t) ((t0 + i) & mask);
            L[i] = sanitize (accL[h] * norm);
            R[i] = sanitize (accR[h] * norm);
            accL[h] = accR[h] = 0.0f;
        }
    }

private:
    struct Mark { int64_t time = 0; float period = 0.0f; };

    void pushMark (int64_t t, float period)
    {
        marks[(size_t) markHead] = { t, period };
        markHead = (markHead + 1) % kMarkCap;
        markCount = std::min (markCount + 1, kMarkCap);
        lastMark = t;
    }
    const Mark& markFromNewest (int i) const { return marks[(size_t) ((markHead - 1 - i + 2 * kMarkCap) % kMarkCap)]; }

    void updateMarks (const AnalysisFrame& f, int64_t tNow)
    {
        // real glottal epochs (ascending), then pseudo-marks for unvoiced stretches
        if (f.epochs != nullptr)
        {
            int64_t ts[64]; float ps[64]; int cnt = 0;
            for (int k = 0; k < f.epochs->size() && cnt < 64; ++k)
            {
                const auto& e = f.epochs->fromNewest (k);
                if (e.time <= lastEpochSeen) break;
                if (e.time <= tNow) { ts[cnt] = e.time; ps[cnt] = e.value; ++cnt; }
            }
            for (int k = cnt - 1; k >= 0; --k)
            {
                lastEpochSeen = ts[k];
                if (ts[k] > lastMark + (int64_t) (0.3f * ps[k])) pushMark (ts[k], ps[k]);
            }
        }
        const int64_t settled = tNow - (int64_t) (0.015 * sr);   // epochs arrive late; only fill settled time
        if (lastMark < settled - 2 * pseudoHop) lastMark = std::max (lastMark, settled - 2 * pseudoHop);
        while (lastMark + pseudoHop <= settled) pushMark (lastMark + pseudoHop, (float) pseudoHop);
    }

    bool nearestMark (int64_t s, int64_t tNow, Mark& out) const
    {
        bool found = false;
        int64_t best = std::numeric_limits<int64_t>::max();
        for (int i = 0; i < markCount; ++i)
        {
            const auto& m = markFromNewest (i);
            if (m.time + maxHalf > tNow) continue;          // grain must be fully recorded
            const int64_t d = std::abs (m.time - s);
            if (d < best) { best = d; out = m; found = true; }
            if (m.time < s - 2 * maxHalf) break;
        }
        return found;
    }

    struct Voice
    {
        int delay = 0;
        float pitch = 1, formant = 1, gl = 0.7f, gr = 0.7f, thr = -40, jitRate = 5;
        int64_t nextMark = 0;
        float gate = 0, jitCents = 0, jitTarget = 0;
        int jitLeft = 0;
        Pcg32 rng;
    };
    std::array<Voice, kMaxVoices> voice;
    std::array<Mark, kMarkCap> marks {};
    int markCount = 0, markHead = 0, activeVoices = 8;
    int64_t lastMark = 0, lastEpochSeen = 0;
    std::vector<float> in, envRing, accL, accR;
    int mask = 0, lookahead = 0, maxHalf = 0, pseudoHop = 0;
    float gateAtk = 0.01f, gateRel = 0.001f, configKey = -1.0f;
    bool started = false;
};

//==============================================================================
// 4.11 Organic Carrier Vocoder — Poisson stream of tiny damped-sinusoid pings shaped by the voice.
class CarrierVocoder final : public ModuleBase
{
public:
    CarrierVocoder() : ModuleBase (ModuleType::CarrierVocoder) {}
    enum { pType, pDensMin, pDensMax, pPingLo, pPingHi, pDecay, pEnvSrc, pPitchLock, pReactAmt, pReactThr };
    enum { Crackle, Gravel, Sizzle, Wet };
    static constexpr int kMaxPings = 128, kBands = 32;

    void onSeedChanged (uint64_t s) override { rng.seedWith (s, 0xca77); }
    void prepareModule() override
    {
        carrierRms.prepare (sr, 10.0f);
        const double hiF = std::min (12000.0, sr * 0.45);
        for (int b = 0; b < kBands; ++b)
        {
            const double fc = 100.0 * std::pow (hiF / 100.0, (b + 0.5) / kBands);
            bandsA[(size_t) b].bandpass (sr, fc, 6.0);
            bandsS[(size_t) b].bandpass (sr, fc, 6.0);
            bandEnv[(size_t) b].prepare (sr); bandEnv[(size_t) b].setTimes (2.0f, 25.0f);
        }
        rmsC = onePoleCoeff (10.0f, sr);
    }
    void resetModule() override
    {
        numPings = 0;
        rateAcc = 0.0f;
        nextThresh = 1.0f;
        synth.reset(); deemph.reset(); post1.reset(); post2.reset();
        for (auto& b : bandsA) b.reset();
        for (auto& b : bandsS) b.reset();
        for (auto& e : bandEnv) e.reset();
        cms = 0.0f;
        rng.seedWith (lastSeed == ~0ULL ? 13 : lastSeed, 0xca77);
        lastType = -1;
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame& f) override
    {
        const int n = buf.getNumSamples(), nch = channels (buf);
        const int type = choice (pType);
        const float dMin = block (pDensMin, n), dMax = std::max (dMin, block (pDensMax, n));
        const float lo = block (pPingLo, n), hi = std::max (lo * 1.01f, block (pPingHi, n));
        const float decayMs = block (pDecay, n);
        const bool filterbank = choice (pEnvSrc) == 1, pitchLock = raw (pPitchLock) >= 0.5f;
        const float rAmt = block (pReactAmt, n), rThr = raw (pReactThr);
        if (type != lastType)
        {
            lastType = type;
            if (type == Gravel)      { post1.lowpass (sr, 1800.0, 0.7); post2.lowpass (sr, 1800.0, 0.7); }
            else if (type == Sizzle) { post1.highpass (sr, 3000.0, 0.7); post2.highpass (sr, 2500.0, 0.7); }
            else                     { post1.highpass (sr, 60.0, 0.7); post2.highpass (sr, 60.0, 0.7); }
        }
        float lastDensity = dMin;

        for (int i = 0; i < n; ++i)
        {
            // ---- carrier: impulse events
            const float drive = reactDrive (f.envDb[i], rThr, 36.0f);
            const float follow = rAmt >= 0.0f ? lerp (1.0f, drive, rAmt) : lerp (1.0f, 1.0f - drive, -rAmt);
            const float density = dMin * std::pow (dMax / dMin, follow);
            lastDensity = density;
            // integrated-rate event clock: Poisson (exponential thresholds) or pitch-locked (unit thresholds)
            const bool locked = pitchLock && f.conf[i] > 0.5f && f.f0[i] > 0.0f;
            rateAcc += (locked ? f.f0[i] : density) / (float) sr;
            if (rateAcc >= nextThresh)
            {
                rateAcc = std::min (rateAcc - nextThresh, 1.0f);
                spawn (type, lo, hi, decayMs);
                nextThresh = locked ? 1.0f + 0.03f * rng.bipolar() : rng.exponential (1.0f);
            }
            float c = 0.0f;
            for (int k = 0; k < numPings; )
            {
                auto& pg = pings[(size_t) k];
                const float re = pg.re * pg.cr - pg.im * pg.ci;
                const float im = pg.re * pg.ci + pg.im * pg.cr;
                pg.re = re; pg.im = im;
                if (pg.chirp != 1.0f)
                {   // rising "bubble" pitch: rotate the step a little each sample
                    const float cr = pg.cr * pg.chirpC - pg.ci * pg.chirpS, ci = pg.cr * pg.chirpS + pg.ci * pg.chirpC;
                    pg.cr = cr; pg.ci = ci;
                }
                c += im;
                if (re * re + im * im < 1.0e-8f) pings[(size_t) k] = pings[(size_t) --numPings];
                else ++k;
            }
            c = post2.process (post1.process (c));
            cms += rmsC * (c * c - cms);
            const float cn = c / (std::sqrt (cms) + 1.0e-4f);   // unit-level carrier

            // ---- apply the voice's spectral envelope
            float y;
            if (! filterbank)
            {
                const float g = f.gainForSample (i);
                y = deemph.process (synth.process (cn * g * 1.25f, f.kForSample (i), f.lpcOrder));
            }
            else
            {
                const float in = nch > 1 ? 0.5f * (buf.getReadPointer (0)[i] + buf.getReadPointer (1)[i]) : buf.getReadPointer (0)[i];
                y = 0.0f;
                for (int b = 0; b < kBands; ++b)
                {
                    const float e = bandEnv[(size_t) b].process (bandsA[(size_t) b].process (in));
                    y += bandsS[(size_t) b].process (cn) * e;
                }
                y *= 3.0f;
            }
            y = sanitize (y);
            for (int ch = 0; ch < nch; ++ch) buf.getWritePointer (ch)[i] = y;
        }
        setLive (pDensMax, lastDensity);
    }

private:
    void spawn (int type, float lo, float hi, float decayMs)
    {
        if (numPings >= kMaxPings) // steal the oldest (index 0) to stay bounded
            pings[0] = pings[(size_t) --numPings];
        auto& pg = pings[(size_t) numPings++];
        float fq = lo * std::pow (hi / lo, rng.uniform());
        float dec = decayMs * rng.range (0.5f, 1.5f);
        float amp = rng.range (0.3f, 1.0f);
        pg.chirp = 1.0f;
        switch (type)
        {
            case Crackle: amp = std::pow (rng.uniform(), 3.0f) * 2.0f + 0.05f; break;  // heavy-tailed pops
            case Gravel:  dec *= 0.6f; break;
            case Sizzle:  dec *= 0.4f; amp *= 0.7f; break;
            case Wet:     pg.chirp = 1.0f + rng.range (0.5f, 2.0f) / (dec * 0.001f * (float) sr); break;
            default: break;
        }
        fq = clampf (fq, 20.0f, 0.45f * (float) sr);
        const float r = std::exp (-1.0f / (std::max (0.05f, dec) * 0.001f * (float) sr));
        const float w = kTwoPi * fq / (float) sr;
        pg.cr = r * std::cos (w); pg.ci = r * std::sin (w);
        const float sign = rng.uniform() < 0.5f ? -1.0f : 1.0f;
        pg.re = type == Crackle ? 0.0f : amp * sign;   // crackle: sine phase start; others cosine (click)
        pg.im = type == Crackle ? amp * sign : 0.0f;
        const float cw = (pg.chirp - 1.0f) * w;
        pg.chirpC = std::cos (cw); pg.chirpS = std::sin (cw);
    }

    struct Ping { float re = 0, im = 0, cr = 1, ci = 0, chirp = 1, chirpC = 1, chirpS = 0; };
    std::array<Ping, kMaxPings> pings {};
    int numPings = 0, lastType = -1;
    float rateAcc = 0.0f, nextThresh = 1.0f, cms = 0.0f, rmsC = 0.01f;
    Pcg32 rng;
    LatticeSynthesis synth;
    DeEmphasis deemph;
    Biquad post1, post2;
    struct RmsDummy { void prepare (double, float) {} } carrierRms;
    std::array<Biquad, kBands> bandsA, bandsS;
    std::array<EnvelopeFollower, kBands> bandEnv;
};

std::unique_ptr<VocalModule> makeMob()            { return std::make_unique<Mob>(); }
std::unique_ptr<VocalModule> makeCarrierVocoder() { return std::make_unique<CarrierVocoder>(); }
} // namespace hv
