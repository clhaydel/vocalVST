// Pitch-synchronous modules: Period Doubler (false-cord growl), Fry Gate (creak / rattle).
#include "Modules.h"

namespace hv
{
//==============================================================================
// 4.2 Period Doubler — attenuate + skew every Nth glottal period (PSOLA overlap-add on epochs),
// add pulse-synchronous band-passed noise. Fallback: zero-crossing alternate-cycle gating.
class PeriodDoubler final : public ModuleBase
{
public:
    PeriodDoubler() : ModuleBase (ModuleType::PeriodDoubler) {}
    enum { pDepth, pMode, pSkew, pNoise, pNoiseFreq, pNoiseWidth, pNoiseDecay, pReactAmt, pReactThr };

    int getLatencySamples() const override { return latency; }

    void prepareModule() override
    {
        latency = (int) std::lround (kPeriodDoublerLatencyMs * 0.001 * sr);
        maxHalf = (int) (0.4 * latency);
        int sz = 1;
        while (sz < (int) (0.25 * sr) + 4 * kMaxSubBlock) sz <<= 1;
        mask = sz - 1;
        for (auto& c : ch)
        {
            c.in.assign ((size_t) sz, 0.0f);
            c.acc.assign ((size_t) sz, 0.0f);
            c.noise.assign ((size_t) sz, 0.0f);
        }
        wsum.assign ((size_t) sz, 0.0f);
        gateC = onePoleCoeff (1.0f, sr);
    }
    void resetModule() override
    {
        for (auto& c : ch)
        {
            std::fill (c.in.begin(), c.in.end(), 0.0f);
            std::fill (c.acc.begin(), c.acc.end(), 0.0f);
            std::fill (c.noise.begin(), c.noise.end(), 0.0f);
            c.bp1.reset(); c.bp2.reset(); c.lp1.reset(); c.lp2.reset();
            c.gate = 1.0f; c.cycle = 0; c.wasPos = false;
        }
        std::fill (wsum.begin(), wsum.end(), 0.0f);
        lastEpoch = std::numeric_limits<int64_t>::min();
        epochCount = 0;
        rng.seedWith (lastSeed == ~0ULL ? 7 : lastSeed, 0xd0b1e);
    }
    void onSeedChanged (uint64_t s) override { rng.seedWith (s, 0xd0b1e); }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame& f) override
    {
        const int n = buf.getNumSamples(), nch = channels (buf);
        const int64_t t0 = f.startTime, tNow = t0 + n - 1;
        const float depthBase = block (pDepth, n) * 0.01f;
        const float skew = block (pSkew, n) * 0.01f;
        const float noiseDb = block (pNoise, n);
        const float nf = block (pNoiseFreq, n), nw = block (pNoiseWidth, n), nd = block (pNoiseDecay, n);
        const float rAmt = block (pReactAmt, n), rThr = raw (pReactThr);
        const int N = choice (pMode) == 1 ? 3 : 2;
        const float depth = clampf (depthBase + rAmt * reactDrive (f.envDb[n - 1], rThr), 0.0f, 1.0f);
        setLive (pDepth, depth * 100.0f);
        const float noiseLvl = noiseDb <= -119.0f ? 0.0f : dbToGain (noiseDb);

        // noise band: two cascaded band-passes, Q from octave width
        const double bwPow = std::pow (2.0, (double) nw);
        const double q = std::sqrt (bwPow) / (bwPow - 1.0);
        for (int c = 0; c < nch; ++c) { ch[(size_t) c].bp1.bandpass (sr, nf, q); ch[(size_t) c].bp2.bandpass (sr, nf, q); }

        // 1) store input
        for (int c = 0; c < nch; ++c)
        {
            const float* x = buf.getReadPointer (c);
            for (int i = 0; i < n; ++i) ch[(size_t) c].in[(size_t) ((t0 + i) & mask)] = x[i];
        }

        // 2) place grains for newly usable epochs (ascending order)
        if (f.epochs != nullptr)
        {
            int64_t times[96]; float periods[96]; int cnt = 0;
            for (int k = 0; k < f.epochs->size() && cnt < 96; ++k)
            {
                const auto& e = f.epochs->fromNewest (k);
                if (e.time <= lastEpoch) break;
                times[cnt] = e.time; periods[cnt] = e.value; ++cnt;
            }
            for (int k = cnt - 1; k >= 0; --k)
            {
                const int64_t e = times[k];
                const int W = std::clamp ((int) periods[k], 8, maxHalf);
                if (e + W > tNow) break;               // not enough input yet; try next block
                lastEpoch = e;
                if (e - W < t0 - (int64_t) latency) continue;   // too old (e.g. after reset)
                const bool hit = (epochCount++ % N) == 0;
                const float g = hit ? 1.0f - depth : 1.0f;
                // skew scales with depth so Depth = 0 is fully transparent
                const float jitterU = rng.uniform();
                const int delta = hit ? (int) std::lround (depth * skew * periods[k] * (0.75f + 0.5f * jitterU)) : 0;
                const int64_t outCenter = e + latency + delta;
                for (int s = -W; s <= W; ++s)
                {
                    const int64_t to = outCenter + s;
                    if (to < t0) continue;
                    const float w = 0.5f + 0.5f * std::cos (kPi * (float) s / (float) (W + 1));
                    wsum[(size_t) (to & mask)] += w;
                    for (int c = 0; c < nch; ++c)
                        ch[(size_t) c].acc[(size_t) (to & mask)] += g * w * ch[(size_t) c].in[(size_t) ((e + s) & mask)];
                }
                if (noiseLvl > 0.0f)
                {   // pulse-synchronous noise: 0.5 ms attack, exponential decay
                    const int att = std::max (1, (int) (0.0005 * sr));
                    const float tau = nd * 0.001f * (float) sr;
                    const int len = att + (int) (5.0f * tau);
                    const float amp = noiseLvl * f.env[n - 1] * 4.0f;
                    for (int s = 0; s < len; ++s)
                    {
                        const float env = s < att ? (float) s / (float) att : std::exp (-(float) (s - att) / tau);
                        const float nz = rng.bipolar() * env * amp;
                        for (int c = 0; c < nch; ++c) ch[(size_t) c].noise[(size_t) ((e + latency + s) & mask)] += nz;
                    }
                }
            }
        }

        // level compensation for the attenuated periods (keeps loudness roughly constant with Depth)
        const float meanSq = 1.0f - (1.0f - (1.0f - depth) * (1.0f - depth)) / (float) N;
        const float comp = std::pow (1.0f / std::max (0.05f, meanSq), 0.35f);

        // 3) output: normalised overlap-add, falling back to zero-crossing gating where no grains exist
        for (int i = 0; i < n; ++i)
        {
            const int64_t t = t0 + i;
            const auto h = (size_t) (t & mask);
            const float w = wsum[h];
            wsum[h] = 0.0f;
            const float cov = std::min (w, 1.0f);
            for (int c = 0; c < nch; ++c)
            {
                auto& s = ch[(size_t) c];
                const float xd = s.in[(size_t) ((t - latency) & mask)];
                // zero-crossing cycle counter on the delayed signal
                const float lp = s.lp2.lp (s.lp1.lp (xd));
                const bool pos = lp > 0.0f;
                if (pos && ! s.wasPos) ++s.cycle;
                s.wasPos = pos;
                const float gt = (s.cycle % N) == 0 ? 1.0f - depth : 1.0f;
                s.gate += gateC * (gt - s.gate);

                const float a = s.acc[h];
                s.acc[h] = 0.0f;
                const float psola = w >= 1.0f ? a / w : a;
                const float nz = s.bp2.process (s.bp1.process (s.noise[h]));
                s.noise[h] = 0.0f;
                buf.getWritePointer (c)[i] = sanitize ((psola + (1.0f - cov) * xd * s.gate) * comp + nz);
            }
        }
    }

private:
    struct Ch
    {
        std::vector<float> in, acc, noise;
        Biquad bp1, bp2;
        OnePole lp1, lp2;
        float gate = 1.0f;
        int cycle = 0;
        bool wasPos = false;
    };
    std::array<Ch, 2> ch;
    std::vector<float> wsum;
    int latency = 0, maxHalf = 0, mask = 0;
    int64_t lastEpoch = 0;
    uint64_t epochCount = 0;
    float gateC = 0.1f;
    Pcg32 rng;

public:
    void prepareFilters()
    {
        for (auto& c : ch) { c.lp1.setCutoff (sr, 900.0); c.lp2.setCutoff (sr, 900.0); }
    }
    void prepare (double sampleRate, int maxBlock) override
    {
        ModuleBase::prepare (sampleRate, maxBlock);
        prepareFilters();
    }
};

//==============================================================================
// 4.3 Fry Gate — irregular pulse train (20-90 Hz, jittered) amplitude-modulates the voice.
class FryGate final : public ModuleBase
{
public:
    FryGate() : ModuleBase (ModuleType::FryGate) {}
    enum { pRate, pJitter, pDecay, pFloor, pSource, pDivide, pReactAmt, pReactThr };

    void onSeedChanged (uint64_t s) override { rng.seedWith (s, 0xf2a7); }
    void resetModule() override
    {
        untilNext = 0.0f; env = 0.0f; attackLeft = 0;
        rng.seedWith (lastSeed == ~0ULL ? 3 : lastSeed, 0xf2a7);
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame& f) override
    {
        const int n = buf.getNumSamples(), nch = channels (buf);
        const float rateBase = block (pRate, n), jit = block (pJitter, n) * 0.01f;
        const float decayMs = block (pDecay, n), floorDb = block (pFloor, n);
        const float rAmt = block (pReactAmt, n), rThr = raw (pReactThr);
        const bool fromF0 = choice (pSource) == 1;
        const int N = std::max (2, choice (pDivide));
        const float floorG = floorDb <= -119.0f ? 0.0f : dbToGain (floorDb);
        const float decayCoeff = std::exp (-1.0f / (decayMs * 0.001f * (float) sr));
        const int attack = std::max (1, (int) (0.0005 * sr));
        float lastRate = rateBase;

        for (int i = 0; i < n; ++i)
        {
            float rate = rateBase;
            if (fromF0 && f.conf[i] > 0.5f && f.f0[i] > 0.0f) rate = f.f0[i] / (float) N;
            // quieter = slower / creakier (bipolar amount)
            const float d = reactDrive (f.envDb[i], rThr);
            rate = clampf (rate * std::pow (2.0f, rAmt * (2.0f * d - 1.0f)), 5.0f, 400.0f);
            lastRate = rate;

            untilNext -= 1.0f;
            if (untilNext <= 0.0f)
            {
                const float period = (float) sr / rate;
                untilNext += std::max (8.0f, period * (1.0f + jit * rng.bipolar()));
                attackLeft = attack;
            }
            if (attackLeft > 0)
            {
                env += (1.0f - env) / (float) attackLeft;
                --attackLeft;
            }
            else env *= decayCoeff;

            const float g = floorG + (1.0f - floorG) * env;
            for (int c = 0; c < nch; ++c) buf.getWritePointer (c)[i] *= g;
        }
        setLive (pRate, clampf (lastRate, 20.0f, 90.0f));
    }

private:
    Pcg32 rng;
    float untilNext = 0.0f, env = 0.0f;
    int attackLeft = 0;
};

std::unique_ptr<VocalModule> makePeriodDoubler() { return std::make_unique<PeriodDoubler>(); }
std::unique_ptr<VocalModule> makeFryGate()       { return std::make_unique<FryGate>(); }
} // namespace hv
