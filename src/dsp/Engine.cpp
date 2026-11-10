#include "Engine.h"
#include "modules/Modules.h"

namespace hv
{
using namespace params;

//==============================================================================
void Limiter::prepare (double sr, float ceilingDb, float lookaheadMs, float releaseMs)
{
    ceiling = dbToGain (ceilingDb);
    look    = std::max (1, (int) std::lround (lookaheadMs * 0.001 * sr));
    relC    = onePoleCoeff (releaseMs, sr);
    dL.assign ((size_t) look, 0.0f);
    dR.assign ((size_t) look, 0.0f);
    box.assign ((size_t) look, 1.0f);
    dqCap = look + 4;
    dqVal.assign ((size_t) dqCap, 1.0f);
    dqIdx.assign ((size_t) dqCap, 0);
    reset();
}

void Limiter::reset()
{
    std::fill (dL.begin(), dL.end(), 0.0f);
    std::fill (dR.begin(), dR.end(), 0.0f);
    std::fill (box.begin(), box.end(), 1.0f);
    boxSum = (double) look;
    state = 1.0f;
    pos = 0;
    dqHead = dqTail = 0;
    t = 0;
}

void Limiter::process (float* L, float* R, int n) noexcept
{
    for (int i = 0; i < n; ++i, ++t)
    {
        const float pk  = std::max (std::abs (L[i]), std::abs (R[i]));
        const float req = pk > ceiling ? ceiling / pk : 1.0f;
        // sliding minimum over the last look+1 samples (monotonic deque, ring storage)
        while (dqTail != dqHead && dqVal[(size_t) ((dqTail - 1 + dqCap) % dqCap)] >= req)
            dqTail = (dqTail - 1 + dqCap) % dqCap;
        dqVal[(size_t) dqTail] = req;
        dqIdx[(size_t) dqTail] = t;
        dqTail = (dqTail + 1) % dqCap;
        while (dqIdx[(size_t) dqHead] < t - look) dqHead = (dqHead + 1) % dqCap;
        const float m = dqVal[(size_t) dqHead];

        state = m < state ? m : state + relC * (m - state);
        boxSum += (double) state - (double) box[(size_t) pos];
        box[(size_t) pos] = state;
        const float g = (float) (boxSum / (double) look);

        const float xl = dL[(size_t) pos], xr = dR[(size_t) pos];
        dL[(size_t) pos] = L[i];
        dR[(size_t) pos] = R[i];
        if (++pos >= look) pos = 0;
        L[i] = clampf (xl * g, -ceiling, ceiling);
        R[i] = clampf (xr * g, -ceiling, ceiling);
    }
}

//==============================================================================
Engine::Engine()
{
    for (auto& s : shared) s = std::make_unique<SlotShared>();
    for (auto& s : slots)
        for (int t = 1; t < kNumModuleTypes; ++t)
            s.mods[(size_t) t] = createModule ((ModuleType) t);
}

Engine::~Engine() = default;

void Engine::prepare (double sampleRate, int maxBlk)
{
    sr = sampleRate;
    maxBlock = maxBlk;
    ana.prepare (sr, kMaxSubBlock);
    for (auto& s : slots)
    {
        for (int t = 1; t < kNumModuleTypes; ++t) s.mods[(size_t) t]->prepare (sr, kMaxSubBlock);
        s.mix.prepare (sr, 20.0f);
        s.gain.prepare (sr, 20.0f);
        for (auto& d : s.dry) d.prepare ((int) (1.3 * sr));
        for (auto& d : s.align) d.prepare ((int) (1.3 * sr));
        s.active = -1;
    }
    chainStore.setSize (2, kMaxSubBlock);
    dryStore.setSize (2, kMaxSubBlock);
    for (auto& b : pathStore) b.setSize (2, kMaxSubBlock);
    mono.assign ((size_t) kMaxSubBlock, 0.0f);
    inGL.assign ((size_t) kMaxSubBlock, 0.0f);
    inGR.assign ((size_t) kMaxSubBlock, 0.0f);
    globalDry.prepare ((int) (4.5 * sr));
    globalDryR.prepare ((int) (4.5 * sr));
    inGainS.prepare (sr, 20.0f);
    outGainS.prepare (sr, 20.0f);
    mixS.prepare (sr, 20.0f);
    dcL.prepare (sr, 8.0);
    dcR.prepare (sr, 8.0);
    limiter.prepare (sr, -0.3f, 1.0f, 60.0f);
    reset();
}

void Engine::reset()
{
    ana.reset();
    for (auto& s : slots)
    {
        for (int t = 1; t < kNumModuleTypes; ++t) s.mods[(size_t) t]->reset();
        for (auto& d : s.dry) d.reset();
        for (auto& d : s.align) d.reset();
        s.mix.snap (1.0f);
        s.gain.snap (1.0f);
        s.active = -1;
    }
    globalDry.reset();
    globalDryR.reset();
    inGainS.snap (dbToGain (P.get (InGain)));
    outGainS.snap (dbToGain (P.get (OutGain)));
    mixS.snap (P.get (Mix) * 0.01f);
    dcL.reset(); dcR.reset();
    limiter.reset();
    std::fill (std::begin (modState), std::end (modState), 0.0f);
}

void Engine::slotValues (int s, int type, float* out) const noexcept
{
    const auto& specs = moduleInfo (type).params;
    for (int k = 0; k < kParamsPerSlot; ++k)
        out[k] = k < (int) specs.size() ? specs[(size_t) k].toReal (P.get (slotIndex (s, SP0 + k))) : 0.0f;
}

bool Engine::splitterHasSends() const noexcept
{
    for (int s = kMainSlots; s < kNumSlots; ++s)
    {
        const int t = (int) std::lround (P.get (slotIndex (s, SType)));
        if (t != 0 && t != (int) ModuleType::Splitter && P.get (slotIndex (s, SOn)) >= 0.5f) return true;
    }
    return false;
}

int Engine::slotLatencyFromParams (int s, bool allowSends) const noexcept
{
    const int type = (int) std::lround (P.get (slotIndex (s, SType)));
    if (type <= 0 || type >= kNumModuleTypes || P.get (slotIndex (s, SOn)) < 0.5f) return 0;
    if (type == (int) ModuleType::Splitter)
    {
        if (! allowSends || ! splitterHasSends()) return 0;
        int mx = 0;
        for (int p = 0; p < 3; ++p)
            mx = std::max (mx, slotLatencyFromParams (kMainSlots + 2 * p, false) + slotLatencyFromParams (kMainSlots + 2 * p + 1, false));
        return mx;
    }
    float v[kParamsPerSlot];
    slotValues (s, type, v);
    return moduleLatencySamples ((ModuleType) type, v, sr);
}

int Engine::latencyFromParams() const noexcept
{
    int total = 0;
    bool sendsUsed = false;
    for (int s = 0; s < kMainSlots; ++s)
    {
        const int type = (int) std::lround (P.get (slotIndex (s, SType)));
        const bool isSplit = type == (int) ModuleType::Splitter && P.get (slotIndex (s, SOn)) >= 0.5f;
        total += slotLatencyFromParams (s, isSplit && ! sendsUsed);
        if (isSplit) sendsUsed = true;
    }
    return std::min (total, globalDry.maxDelay()) + limiter.latency();
}

void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    int done = 0;
    while (done < n)
    {
        const int len = std::min (kMaxSubBlock, n - done);
        processSub (inL + done, inR != nullptr ? inR + done : nullptr, outL + done, outR + done, len);
        done += len;
    }
}

void Engine::computeModulation() noexcept
{
    for (auto& row : modOff)
        std::fill (std::begin (row), std::end (row), 0.0f);
    const auto& L = ana.latest();
    for (int m = 0; m < kNumMods; ++m)
    {
        const int src = (int) std::lround (P.get (modIndex (m, MSrc)));
        const int tgt = (int) std::lround (P.get (modIndex (m, MTgt)));
        if (src == SrcNone || tgt <= 0 || tgt >= kNumTargets) continue;
        float x = 0.0f;
        switch (src)
        {
            case SrcEnvelope:  x = clampf ((L.envDb + 60.0f) / 60.0f, 0.0f, 1.0f); break;
            case SrcPitch:     x = L.f0 > 0.0f ? clampf (std::log2 (L.f0 / 40.0f) / std::log2 (25.0f), 0.0f, 1.0f) : 0.0f; break;
            case SrcPitchConf: x = L.conf; break;
            case SrcVoiced:    x = L.voiced; break;
            case SrcFricative: x = L.fricative; break;
            case SrcPlosive:   x = L.plosive; break;
            default: break;
        }
        const float thr = P.get (modIndex (m, MThr));
        x = thr < 0.999f ? clampf ((x - thr) / (1.0f - thr), 0.0f, 1.0f) : 0.0f;
        switch ((int) std::lround (P.get (modIndex (m, MCurve))))
        {
            case 1: x = x * x; break;           // exp
            case 2: x = std::sqrt (x); break;   // log
            default: break;
        }
        const float ms = P.get (modIndex (m, MSmooth));
        const float a  = 1.0f - std::exp (-(float) kMaxSubBlock / (std::max (1.0f, ms) * 0.001f * (float) sr));
        modState[m] += a * (x - modState[m]);
        const int si = (tgt - 1) / kTargetsPerSlot, pi = (tgt - 1) % kTargetsPerSlot;
        modOff[si][pi] += P.get (modIndex (m, MAmt)) * modState[m];
    }
}

int Engine::runSlot (int s, juce::AudioBuffer<float>& buf, int n, bool& stereo, int upstream, bool allowSends) noexcept
{
    auto& sl = slots[(size_t) s];
    const int type = std::clamp ((int) std::lround (P.get (slotIndex (s, SType))), 0, kNumModuleTypes - 1);
    if (type != sl.active)
    {
        if (type > 0) sl.mods[(size_t) type]->reset();
        for (auto& d : sl.dry) d.reset();
        for (auto& d : sl.align) d.reset();
        sl.active = type;
        sl.mix.snap (clampf (P.get (slotIndex (s, SMix)) * 0.01f, 0.0f, 1.0f));
    }
    auto& sh = *shared[(size_t) s];
    if (type == 0 || P.get (slotIndex (s, SOn)) < 0.5f) { sh.level.store (0.0f, std::memory_order_relaxed); return 0; }

    // parameter values (normalized + modulation -> real units)
    const auto& specs = moduleInfo (type).params;
    for (int k = 0; k < kParamsPerSlot; ++k)
    {
        float nv = P.get (slotIndex (s, SP0 + k));
        if (k < (int) specs.size() && specs[(size_t) k].continuous()) nv = clampf (nv + modOff[s][k], 0.0f, 1.0f);
        sl.live[k]   = nv;
        sl.values[k] = k < (int) specs.size() ? specs[(size_t) k].toReal (nv) : 0.0f;
    }
    const float mixN = clampf (P.get (slotIndex (s, SMix)) * 0.01f + modOff[s][kParamsPerSlot], 0.0f, 1.0f);
    sl.live[kParamsPerSlot] = mixN;
    sl.mix.setTarget (mixN);
    sl.gain.setTarget (dbToGain (P.get (slotIndex (s, SOut))));

    AnalysisFrame frame;
    ana.makeFrame (upstream, n, frame);

    ProcessContext ctx;
    ctx.values = sl.values;
    ctx.liveNorm = sl.live;
    ctx.numChannels = stereo ? 2 : 1;
    ctx.seed = mixSeed (globalSeed, (uint64_t) (s * 131 + type));
    ctx.shared = &sh;

    for (int c = 0; c < 2; ++c)
        std::copy (buf.getReadPointer (stereo ? c : 0), buf.getReadPointer (stereo ? c : 0) + n, sl.dryBuf[c]);

    bool outStereo = stereo;
    int lat = 0;
    auto* mod = sl.mods[(size_t) type].get();
    if (type == (int) ModuleType::Splitter && allowSends && splitterHasSends())
    {
        juce::AudioBuffer<float>* paths[3];
        for (int p = 0; p < 3; ++p)
        {
            pathView[p].setDataToReferTo (pathStore[p].getArrayOfWritePointers(), 2, n);
            paths[p] = &pathView[p];
        }
        mod->setContext (ctx);
        dynamic_cast<SplitterPaths*> (mod)->processPaths (buf, paths, frame);
        int pathLat[3] {}, maxLat = 0;
        bool pathStereo[3] {};
        for (int p = 0; p < 3; ++p)
        {
            pathStereo[p] = stereo;
            for (int k = 0; k < 2; ++k)
                pathLat[p] += runSlot (kMainSlots + 2 * p + k, *paths[p], n, pathStereo[p], upstream + pathLat[p], false);
            maxLat = std::max (maxLat, pathLat[p]);
            outStereo |= pathStereo[p];
        }
        for (int c = 0; c < 2; ++c)
        {
            float* o = buf.getWritePointer (c);
            for (int i = 0; i < n; ++i) o[i] = 0.0f;
            for (int p = 0; p < 3; ++p)
            {
                const float* src = paths[p]->getReadPointer (pathStereo[p] ? c : 0);
                auto& al = sl.align[(size_t) (p * 2 + c)];
                const int d = maxLat - pathLat[p];
                for (int i = 0; i < n; ++i) o[i] += al.process (src[i], d);
            }
        }
        lat = maxLat;
    }
    else
    {
        mod->setContext (ctx);
        mod->process (buf, frame);
        lat = mod->getLatencySamples();
        if (mod->producesStereo()) outStereo = true;
    }

    // slot mix (dry delayed by the module's latency) + output gain
    const int nOut = outStereo ? 2 : 1;
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float m = sl.mix.next(), g = sl.gain.next();
        for (int c = 0; c < nOut; ++c)
        {
            const float d = sl.dry[(size_t) c].process (sl.dryBuf[stereo ? c : 0][i], lat);
            float& w = buf.getWritePointer (c)[i];
            w = sanitize (d * (1.0f - m) + w * m * g);
            peak = std::max (peak, std::abs (w));
        }
    }
    stereo = outStereo;
    sh.level.store (peak, std::memory_order_relaxed);
    for (int k = 0; k <= kParamsPerSlot; ++k) sh.liveNorm[(size_t) k].store (sl.live[k], std::memory_order_relaxed);
    return lat;
}

void Engine::processSub (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    globalSeed = (uint64_t) P.get (Seed) + 1;
    inGainS.setTarget (dbToGain (P.get (InGain)));
    outGainS.setTarget (dbToGain (P.get (OutGain)));
    mixS.setTarget (P.get (Mix) * 0.01f);
    ana.setEnvelopeTimes (P.get (EnvAtk), P.get (EnvRel));

    // Stereo mode keeps L/R separate through the chain and the global dry path; mono mode (or a mono
    // input bus) sums to mono up front. Analysis always runs on the mono sum.
    const bool stereoIn = inR != nullptr && P.get (InputMode) >= 0.5f;
    float ip = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainS.next();
        if (stereoIn)
        {
            inGL[(size_t) i] = sanitize (inL[i] * g, 16.0f);
            inGR[(size_t) i] = sanitize (inR[i] * g, 16.0f);
            mono[(size_t) i] = 0.5f * (inGL[(size_t) i] + inGR[(size_t) i]);
            ip = std::max ({ ip, std::abs (inGL[(size_t) i]), std::abs (inGR[(size_t) i]) });
        }
        else
        {
            const float x = inR != nullptr ? 0.5f * (inL[i] + inR[i]) : inL[i];
            mono[(size_t) i] = sanitize (x * g, 16.0f);
            ip = std::max (ip, std::abs (mono[(size_t) i]));
        }
    }
    inPeak.store (std::max (ip, inPeak.load (std::memory_order_relaxed)), std::memory_order_relaxed);

    ana.process (mono.data(), n);
    computeModulation();

    chainView.setDataToReferTo (chainStore.getArrayOfWritePointers(), 2, n);
    const float* srcL = stereoIn ? inGL.data() : mono.data();
    const float* srcR = stereoIn ? inGR.data() : mono.data();
    std::copy (srcL, srcL + n, chainView.getWritePointer (0));
    std::copy (srcR, srcR + n, chainView.getWritePointer (1));

    bool stereo = stereoIn, sendsUsed = false;
    int latency = 0;
    for (int s = 0; s < kMainSlots; ++s)
    {
        const int type = (int) std::lround (P.get (slotIndex (s, SType)));
        const bool isSplit = type == (int) ModuleType::Splitter && P.get (slotIndex (s, SOn)) >= 0.5f;
        latency += runSlot (s, chainView, n, stereo, latency, isSplit && ! sendsUsed);
        if (isSplit) sendsUsed = true;
    }
    // keep send slots' state fresh when unused (type switches reset there)
    if (! stereo) std::copy (chainView.getReadPointer (0), chainView.getReadPointer (0) + n, chainView.getWritePointer (1));

    latency = std::min (latency, globalDry.maxDelay());
    float* L = chainView.getWritePointer (0);
    float* R = chainView.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        const float dL = globalDry.process (srcL[i], latency);
        const float dR = globalDryR.process (srcR[i], latency);
        const float m = mixS.next(), g = outGainS.next();
        L[i] = dcL.process ((dL * (1.0f - m) + L[i] * m) * g);
        R[i] = dcR.process ((dR * (1.0f - m) + R[i] * m) * g);
    }
    limiter.process (L, R, n);
    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        outL[i] = L[i]; outR[i] = R[i];
        pl = std::max (pl, std::abs (L[i])); pr = std::max (pr, std::abs (R[i]));
    }
    outPeakL.store (std::max (pl, outPeakL.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    outPeakR.store (std::max (pr, outPeakR.load (std::memory_order_relaxed)), std::memory_order_relaxed);
}
} // namespace hv
