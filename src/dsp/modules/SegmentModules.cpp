// Segmenting modules: Consonant/Vowel Splitter, Syllable Reverse / Shuffle.
#include "Modules.h"

namespace hv
{
//==============================================================================
// 4.5 Consonant / Vowel Splitter
class Splitter final : public ModuleBase, public SplitterPaths
{
public:
    Splitter() : ModuleBase (ModuleType::Splitter) {}
    enum { pGV, pGF, pGP, pCombT, pCombFb, pCombDamp, pBurstLen, pBurstTone, pBurstLvl, pSens };

    void prepareModule() override
    {
        for (auto& c : ch) c.comb.prepare ((int) (0.011 * sr) + 4);
        xfade = onePoleCoeff (4.0f, sr);
        combT.prepare (sr, 30.0f);
        for (auto& b : tmp) b.setSize (2, kMaxSubBlock);
    }
    void resetModule() override
    {
        for (auto& c : ch) { c.comb.reset(); c.damp.reset(); }
        wV = 1.0f; wF = wP = 0.0f;
        burstLeft = 0; burstAmp = 0.0f; burstLp.reset();
        lastPlosive = std::numeric_limits<int64_t>::min();
        rng.seedWith (lastSeed == ~0ULL ? 9 : lastSeed, 0xb0057);
        combT.snap (raw (pCombT) > 0 ? raw (pCombT) : 3.0f);
    }
    void onSeedChanged (uint64_t s) override { rng.seedWith (s, 0xb0057); }

    void processPaths (const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>* paths[3], const AnalysisFrame& f) override
    {
        const int n = in.getNumSamples(), nch = std::min (ctx.numChannels, in.getNumChannels());
        const float gV = dbToGain (block (pGV, n)), gF = dbToGain (block (pGF, n)), gP = dbToGain (block (pGP, n));
        const float fb = block (pCombFb, n), dampAmt = block (pCombDamp, n) * 0.01f;
        const float bLen = block (pBurstLen, n), bTone = block (pBurstTone, n);
        const float bLvlDb = block (pBurstLvl, n);
        const float bLvl = bLvlDb <= -119.0f ? 0.0f : dbToGain (bLvlDb);
        const float s = block (pSens, n) * 0.01f;
        combT.setTarget (raw (pCombT));
        burstLp.setCutoff (sr, bTone);
        for (auto& c : ch) c.damp.setCutoff (sr, 18000.0 * std::pow (0.04, (double) dampAmt));

        const float centre = 0.8f - 0.2f * s;
        const float fricGain = 0.5f + s, plosGain = 0.5f + s;
        const float burstDecay = std::exp (-5.0f / (bLen * 0.001f * (float) sr));

        // plosive events inside this block trigger the burst exciter
        int64_t trigAt = -1;
        if (f.plosives != nullptr)
        {
            EventRing::Event e;
            if (f.plosives->lastAtOrBefore (f.startTime + n - 1, e) && e.time >= f.startTime && e.time > lastPlosive)
            { trigAt = e.time; lastPlosive = e.time; }
        }

        for (int i = 0; i < n; ++i)
        {
            const float gate = smoothstep (-62.0f, -52.0f, f.envDb[i]);
            const float pv = smoothstep (centre - 0.08f, centre + 0.08f, f.periodicity[i]) * gate;
            const float pf = clampf (f.fricScore[i] * fricGain, 0.0f, 1.0f) * (1.0f - pv) * gate;
            const float pp = clampf (f.plosive[i] * plosGain, 0.0f, 1.0f) * gate;
            const float tP = pp, tF = (1.0f - tP) * pf, tV = 1.0f - tP - tF;
            wV += xfade * (tV - wV); wF += xfade * (tF - wF); wP += xfade * (tP - wP);

            if (f.startTime + i == trigAt)
            {
                burstLeft = (int) (bLen * 0.001f * (float) sr);
                burstAmp  = bLvl * std::max (f.env[i], 0.05f) * 3.0f;
            }
            float burst = 0.0f;
            if (burstLeft > 0) { burst = burstLp.lp (rng.bipolar()) * burstAmp * 2.5f; burstAmp *= burstDecay; --burstLeft; }

            const float dly = combT.next() * 0.001f * (float) sr;
            for (int c = 0; c < nch; ++c)
            {
                auto& st = ch[(size_t) c];
                const float x = in.getReadPointer (c)[i];
                paths[0]->getWritePointer (c)[i] = x * wV * gV;
                const float u = x * wF;
                const float y = u + fb * st.damp.lp (st.comb.tapLinear (dly));
                st.comb.push (sanitize (y, 8.0f));
                paths[1]->getWritePointer (c)[i] = y * (1.0f - 0.6f * fb) * gF;
                paths[2]->getWritePointer (c)[i] = (x * wP + burst) * gP;
            }
        }
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame& f) override
    {
        const int n = buf.getNumSamples(), nch = channels (buf);
        juce::AudioBuffer<float>* paths[3];
        for (int p = 0; p < 3; ++p)
        {
            tmpView[p].setDataToReferTo (tmp[p].getArrayOfWritePointers(), 2, n);
            paths[p] = &tmpView[p];
        }
        processPaths (buf, paths, f);
        for (int c = 0; c < nch; ++c)
        {
            float* x = buf.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                x[i] = sanitize (paths[0]->getSample (c, i) + paths[1]->getSample (c, i) + paths[2]->getSample (c, i));
        }
    }

private:
    struct Ch { DelayLine comb; OnePole damp; };
    std::array<Ch, 2> ch;
    std::array<juce::AudioBuffer<float>, 3> tmp, tmpView;
    Smoother combT;
    float wV = 1, wF = 0, wP = 0, xfade = 0.01f;
    int burstLeft = 0;
    float burstAmp = 0.0f;
    OnePole burstLp;
    Pcg32 rng;
    int64_t lastPlosive = 0;
};

//==============================================================================
// 4.6 Syllable Reverse / Shuffle — onset-segmented lookahead buffer.
class SyllableShuffle final : public ModuleBase
{
public:
    SyllableShuffle() : ModuleBase (ModuleType::SyllableShuffle) {}
    enum { pMode, pWindow, pMinSeg, pXfade, pSeed };

    static int latencyFor (float windowMs, double sr)
    {
        return (int) std::lround ((windowMs + kSyllableExtraLatencyMs) * 0.001 * sr) + kMaxSubBlock;
    }
    int getLatencySamples() const override { return curLatency; }

    void prepareModule() override
    {
        int sz = 1;
        while (sz < (int) (1.3 * sr) + 4 * kMaxSubBlock) sz <<= 1;
        mask = sz - 1;
        for (auto& c : ch) c.ring.assign ((size_t) sz, 0.0f);
        curLatency = latencyFor (300.0f, sr);
    }
    void resetModule() override
    {
        for (auto& c : ch) std::fill (c.ring.begin(), c.ring.end(), 0.0f);
        numEntries = 0;
        groupCount = 0;
        started = false;
        lastOnset = std::numeric_limits<int64_t>::min();
        seeded = false;
    }

    void setContext (const ProcessContext& c) override
    {
        ModuleBase::setContext (c);
        curLatency = latencyFor (c.values[pWindow], sr);
        const uint64_t s = mixSeed (c.seed, (uint64_t) c.values[pSeed]);
        if (! seeded || s != moduleSeed) { moduleSeed = s; rng.seedWith (s, 0x5ff1e); seeded = true; }
    }

    void process (juce::AudioBuffer<float>& buf, const AnalysisFrame& f) override
    {
        const int n = buf.getNumSamples(), nch = channels (buf);
        const int64_t t0 = f.startTime, tNow = t0 + n - 1;
        const bool shuffle = choice (pMode) == 1;
        const int W = (int) (raw (pWindow) * 0.001f * (float) sr);
        const int minSeg = (int) (block (pMinSeg, n) * 0.001f * (float) sr);
        const int xh = (int) (block (pXfade, n) * 0.0005f * (float) sr);   // half crossfade
        const int L = curLatency;
        const int extra = (int) (kSyllableExtraLatencyMs * 0.001f * (float) sr);

        for (int c = 0; c < nch; ++c)
        {
            const float* x = buf.getReadPointer (c);
            for (int i = 0; i < n; ++i) ch[(size_t) c].ring[(size_t) ((t0 + i) & mask)] = x[i];
        }
        if (! started) { segStart = t0; groupStart = t0; started = true; }

        // ---- segment decisions up to the time for which onsets are final
        const int64_t decide = std::min (tNow, f.knownUntil - extra);
        if (f.onsets != nullptr)
        {
            int64_t ons[64]; int cnt = 0;
            for (int k = 0; k < f.onsets->size() && cnt < 64; ++k)
            {
                const auto& e = f.onsets->fromNewest (k);
                if (e.time <= lastOnset) break;
                if (e.time <= decide) ons[cnt++] = e.time;
            }
            for (int k = cnt - 1; k >= 0; --k)
            {
                lastOnset = ons[k];
                if (ons[k] <= segStart) continue;
                while (ons[k] - segStart > W) closeSegment (segStart + W, shuffle, W, L, xh);
                if (ons[k] - segStart >= minSeg) closeSegment (ons[k], shuffle, W, L, xh);
            }
        }
        while (decide - segStart >= W) closeSegment (segStart + W, shuffle, W, L, xh);
        if (shuffle && groupCount > 0 && decide - groupStart >= W) flushGroup (L, xh);

        // ---- render scheduled playback
        for (int i = 0; i < n; ++i)
        {
            const int64_t t = t0 + i;
            float acc[2] = { 0.0f, 0.0f };
            for (int e = 0; e < numEntries; ++e)
            {
                const auto& en = entries[(size_t) e];
                const int64_t rel = t - en.outStart;
                if (rel < -en.xh || rel >= en.len + en.xh) continue;
                float g = 1.0f;
                if (en.xh > 0)
                {
                    if (rel < en.xh) g *= std::sin (0.5f * kPi * (float) (rel + en.xh) / (float) (2 * en.xh));
                    if (rel >= en.len - en.xh) g *= std::cos (0.5f * kPi * (float) (rel - (en.len - en.xh)) / (float) (2 * en.xh));
                }
                const int64_t src = en.reversed ? en.srcStart + en.len - 1 - rel : en.srcStart + rel;
                for (int c = 0; c < nch; ++c) acc[c] += g * ch[(size_t) c].ring[(size_t) (src & mask)];
            }
            for (int c = 0; c < nch; ++c) buf.getWritePointer (c)[i] = sanitize (acc[c]);
        }
        // drop finished entries
        int w = 0;
        for (int e = 0; e < numEntries; ++e)
            if (entries[(size_t) e].outStart + entries[(size_t) e].len + entries[(size_t) e].xh > tNow)
                entries[(size_t) w++] = entries[(size_t) e];
        numEntries = w;
    }

private:
    struct Entry { int64_t outStart = 0, srcStart = 0; int len = 0, xh = 0; bool reversed = false; };
    struct Seg { int64_t start; int len; };

    void schedule (int64_t outStart, int64_t srcStart, int len, bool rev, int xh)
    {
        if (len <= 0 || numEntries >= (int) entries.size()) return;
        entries[(size_t) numEntries++] = { outStart, srcStart, len, std::min (xh, len / 2), rev };
    }

    void closeSegment (int64_t end, bool shuffle, int W, int L, int xh)
    {
        const int len = (int) (end - segStart);
        if (len <= 0) return;
        if (! shuffle)
            schedule (segStart + L, segStart, len, true, xh);
        else
        {
            int64_t total = 0;
            for (int g = 0; g < groupCount; ++g) total += group[(size_t) g].len;
            if (groupCount > 0 && (total + len > W || groupCount >= (int) group.size())) flushGroup (L, xh);
            if (groupCount == 0) groupStart = segStart;
            group[(size_t) groupCount++] = { segStart, len };
        }
        segStart = end;
    }

    void flushGroup (int L, int xh)
    {
        int order[32];
        for (int g = 0; g < groupCount; ++g) order[g] = g;
        for (int g = groupCount - 1; g > 0; --g) std::swap (order[g], order[rng.below (g + 1)]);
        if (groupCount > 1)
        {   // never leave the order unchanged
            bool identity = true;
            for (int g = 0; g < groupCount; ++g) identity &= order[g] == g;
            if (identity) std::rotate (order, order + 1, order + groupCount);
        }
        int64_t out = groupStart + L;
        for (int g = 0; g < groupCount; ++g)
        {
            const auto& sg = group[(size_t) order[g]];
            schedule (out, sg.start, sg.len, false, xh);
            out += sg.len;
        }
        groupCount = 0;
    }

    struct Ch { std::vector<float> ring; };
    std::array<Ch, 2> ch;
    std::array<Entry, 128> entries {};
    std::array<Seg, 32> group {};
    int numEntries = 0, groupCount = 0, mask = 0, curLatency = 0;
    int64_t segStart = 0, groupStart = 0, lastOnset = 0;
    bool started = false, seeded = false;
    uint64_t moduleSeed = 0;
    Pcg32 rng;
};

std::unique_ptr<VocalModule> makeSplitter()        { return std::make_unique<Splitter>(); }
std::unique_ptr<VocalModule> makeSyllableShuffle() { return std::make_unique<SyllableShuffle>(); }

int syllableLatency (float windowMs, double sr) { return SyllableShuffle::latencyFor (windowMs, sr); }
} // namespace hv
