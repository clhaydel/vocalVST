#include "Analysis.h"

namespace hv
{
void AnalysisEngine::prepare (double sampleRate, int /*maxBlock*/)
{
    sr = sampleRate;
    histLen = 1;
    while (histLen < (int) (sr * 1.2) + 4 * kMaxSubBlock) histLen <<= 1;
    histMask = histLen - 1;
    for (auto& r : rings) r.assign ((size_t) histLen, 0.0f);
    chunkLen  = histLen / kLpcChunk;
    chunkMask = chunkLen - 1;
    kRing.assign ((size_t) chunkLen, {});
    gainRing.assign ((size_t) chunkLen, 0.0f);
    fmRing.assign ((size_t) chunkLen, {});

    envF.prepare (sr);
    lpcA.prepare (sr, true);
    yin.prepare (sr);

    fftSize = 1;
    while (fftSize < (int) (sr * 0.02)) fftSize <<= 1;
    hop = (int) std::lround (sr * 0.005);
    fft.prepare (fftSize);
    specRing.assign ((size_t) fftSize, 0.0f);
    specFrame.assign ((size_t) fftSize, 0.0f);
    specWin.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        specWin[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * ((float) i + 0.5f) / (float) fftSize);
    mag.assign ((size_t) fftSize / 2 + 1, 0.0f);
    prevLogMag.assign ((size_t) fftSize / 2 + 1, 0.0f);

    smoothC   = onePoleCoeff (3.0f, sr);
    plosDecay = (float) std::exp (-1.0 / (0.025 * sr));
    f0Glide   = onePoleCoeff (4.0f, sr);
    epochSmooth.setCutoff (sr, 2500.0);
    tele.sampleRate.store ((float) sr);
    reset();
}

void AnalysisEngine::reset()
{
    timeNow = 0;
    for (auto& r : rings) std::fill (r.begin(), r.end(), 0.0f);
    std::fill (rings[rEnvDb].begin(), rings[rEnvDb].end(), -120.0f);
    for (auto& k : kRing) k.fill (0.0f);
    std::fill (gainRing.begin(), gainRing.end(), 0.0f);
    std::fill (fmRing.begin(), fmRing.end(), Formants {});
    envF.reset();
    lpcA.reset();
    yin.reset();
    std::fill (specRing.begin(), specRing.end(), 0.0f);
    std::fill (prevLogMag.begin(), prevLogMag.end(), 0.0f);
    std::fill (std::begin (fluxHist), std::end (fluxHist), 0.0f);
    std::fill (std::begin (energyHist), std::end (energyHist), -120.0f);
    prevFlux[0] = prevFlux[1] = 0.0f;
    fluxPos = energyPos = zcrCount = hopCount = specPos = 0;
    lastSample = 0.0f;
    lastOnset = lastPlosive = -1000000;
    hopVoiced = hopFric = hopPeriod = hopFricScore = 0.0f;
    sVoiced = sFric = sPlos = 0.0f;
    heldF0 = curF0 = curConf = 0.0f;
    lastEpoch = candPos = -1;
    candVal = 0.0f;
    epochSmooth.reset();
    epochRing.clear(); onsetRing.clear(); plosiveRing.clear();
    last = {};
}

void AnalysisEngine::process (const float* dry, int n) noexcept
{
    bool plosTrigThisBlock = false;
    for (int i = 0; i < n; ++i)
    {
        const int64_t t = timeNow;
        const float x = sanitize (dry[i], 8.0f);

        const float env = envF.process (x);
        if (yin.push (x))
        {
            curConf = yin.confidence();
            if (curConf > 0.5f && yin.frequency() > 0.0f)
            {
                curF0 = yin.frequency();
                if (heldF0 <= 0.0f) heldF0 = curF0;
            }
        }
        if (curConf > 0.5f && curF0 > 0.0f) heldF0 += f0Glide * (curF0 - heldF0);

        const float e = lpcA.processSample (x);
        if (lpcA.chunkStarted())
        {
            const auto ci = (size_t) ((t / kLpcChunk) & chunkMask);
            std::copy (lpcA.currentK(), lpcA.currentK() + lpcA.order(), kRing[ci].begin());
            gainRing[ci] = lpcA.currentGain();
            fmRing[ci]   = lpcA.formants();
        }
        if (lpcA.frameJustAnalysed())
        {
            const float* envDb = lpcA.envelope();
            for (int j = 0; j < kEnvPoints; ++j) tele.lpcCurve[(size_t) j].store (envDb[j], std::memory_order_relaxed);
            const auto& fm = lpcA.formants();
            for (int j = 0; j < kMaxFormants; ++j)
            {
                tele.formants[(size_t) j].store (fm.freq[j], std::memory_order_relaxed);
                tele.formants[(size_t) (j + kMaxFormants)].store (fm.bw[j], std::memory_order_relaxed);
            }
            tele.numFormants.store (fm.count, std::memory_order_relaxed);
        }

        // spectral classifier / onset detector input
        specRing[(size_t) specPos] = x;
        if (++specPos >= fftSize) specPos = 0;
        if ((x >= 0.0f) != (lastSample >= 0.0f)) ++zcrCount;
        lastSample = x;
        if (++hopCount >= hop)
        {
            hopCount = 0;
            const float before = sPlos;
            classifyHop();
            if (sPlos > before) plosTrigThisBlock = true;
        }
        sVoiced += smoothC * (hopVoiced - sVoiced);
        sFric   += smoothC * (hopFric - sFric);
        sPlos   *= plosDecay;

        detectEpoch (epochSmooth.lp (std::abs (e)), t);

        const auto h = (size_t) (t & histMask);
        rings[rEnv][h]       = env;
        rings[rEnvDb][h]     = gainToDb (env);
        rings[rF0][h]        = heldF0;
        rings[rConf][h]      = curConf;
        rings[rVoiced][h]    = sVoiced;
        rings[rFric][h]      = sFric;
        rings[rPlos][h]      = sPlos;
        rings[rPeriod][h]    = hopPeriod;
        rings[rFricScore][h] = hopFricScore;
        rings[rResidual][h]  = e;
        ++timeNow;
    }

    if (n > 0)
    {
        const auto h = (size_t) ((timeNow - 1) & histMask);
        last.env       = rings[rEnv][h];
        last.envDb     = rings[rEnvDb][h];
        last.f0        = heldF0;
        last.conf      = curConf;
        last.voiced    = sVoiced;
        last.fricative = sFric;
        last.plosive   = sPlos;
        last.plosiveTrig = plosTrigThisBlock;

        const int w = tele.writeIndex.load (std::memory_order_relaxed);
        tele.hist[(size_t) (w % AnalysisTelemetry::kHistory)] = { last.envDb, curConf > 0.5f ? heldF0 : 0.0f, curConf,
                                                                   sVoiced, sFric, sPlos };
        tele.writeIndex.store (w + 1, std::memory_order_release);
    }
}

void AnalysisEngine::classifyHop() noexcept
{
    for (int i = 0; i < fftSize; ++i)
        specFrame[(size_t) i] = specRing[(size_t) ((specPos + i) % fftSize)] * specWin[(size_t) i];
    fft.magnitude (specFrame.data(), mag.data());

    const int bins = fftSize / 2 + 1;
    const float binHz = (float) sr / (float) fftSize;
    double pw = 0.0, pwF = 0.0;
    float flux = 0.0f;
    int fluxBins = 0;
    const float norm = 2.0f / (float) fftSize;
    for (int b = 1; b < bins; ++b)
    {
        const float m = mag[(size_t) b] * norm;
        const double p2 = (double) m * m;
        pw += p2;
        pwF += p2 * (b * binHz);
        if (b * binHz < 12000.0f)
        {
            const float lm = std::log1p (1000.0f * m);
            flux += std::max (0.0f, lm - prevLogMag[(size_t) b]);
            prevLogMag[(size_t) b] = lm;
            ++fluxBins;
        }
    }
    flux /= (float) std::max (1, fluxBins);
    const float centroid = pw > 1.0e-14 ? (float) (pwF / pw) : 0.0f;
    const float energyDb = 10.0f * std::log10 ((float) pw + 1.0e-12f);
    const float zcrHz    = (float) zcrCount * (float) sr / (float) hop * 0.5f;
    zcrCount = 0;

    const float gate     = smoothstep (-62.0f, -50.0f, energyDb);
    hopPeriod            = yin.confidence();
    hopVoiced            = smoothstep (0.6f, 0.8f, hopPeriod) * gate;
    hopFricScore         = smoothstep (2800.0f, 4500.0f, centroid) * smoothstep (1200.0f, 3000.0f, zcrHz);
    hopFric              = hopFricScore * (1.0f - hopVoiced) * gate;

    // adaptive-threshold onset detection on spectral flux (peak picked one hop late)
    float sorted[16];
    std::copy (std::begin (fluxHist), std::end (fluxHist), sorted);
    std::nth_element (sorted, sorted + 8, sorted + 16);
    const float median = sorted[8];
    const float thr    = median * 1.5f + 0.02f;
    const int64_t peakTime = timeNow - hop - fftSize / 4;
    float minEnergy = 0.0f;
    for (float eh : energyHist) minEnergy = std::min (minEnergy, eh);
    const float prevEnergy = energyHist[(size_t) ((energyPos + 11) % 12)];
    if (prevFlux[0] > prevFlux[1] && prevFlux[0] >= flux && prevFlux[0] > thr && gate > 0.2f
        && peakTime - lastOnset > (int64_t) (0.05 * sr))
    {
        lastOnset = peakTime;
        onsetRing.push (peakTime, prevFlux[0]);
        // plosive: sudden broadband burst out of (near) closure, not strongly periodic
        if (prevEnergy - minEnergy > 15.0f && hopPeriod < 0.65f && peakTime - lastPlosive > (int64_t) (0.08 * sr))
        {
            lastPlosive = peakTime;
            plosiveRing.push (peakTime, prevFlux[0]);
            sPlos = 1.0f;
        }
    }
    prevFlux[1] = prevFlux[0];
    prevFlux[0] = flux;
    fluxHist[fluxPos] = flux;
    fluxPos = (fluxPos + 1) % 16;
    energyHist[energyPos] = energyDb;
    energyPos = (energyPos + 1) % 12;
}

void AnalysisEngine::detectEpoch (float v, int64_t t) noexcept
{
    if (curConf < 0.5f || heldF0 <= 0.0f)
    {
        lastEpoch = -1; candPos = -1; candVal = 0.0f;
        return;
    }
    const float T = (float) sr / heldF0;
    if (lastEpoch < 0) lastEpoch = t - (int64_t) (0.6f * T);
    const float since = (float) (t - lastEpoch);
    if (since < 0.6f * T) return;
    if (candPos < 0 || v > candVal) { candVal = v; candPos = t; }
    if ((float) (t - candPos) >= 0.4f * T || since >= 1.5f * T)
    {
        epochRing.push (candPos, T);
        lastEpoch = candPos;
        candPos = -1;
        candVal = 0.0f;
    }
}

void AnalysisEngine::makeFrame (int delaySamples, int n, AnalysisFrame& f) const noexcept
{
    delaySamples = std::clamp (delaySamples, 0, maxDelay());
    n = std::min (n, kMaxSubBlock);
    f.numSamples = n;
    f.sampleRate = sr;
    f.startTime  = timeNow - n - delaySamples;
    f.knownUntil = timeNow;
    f.lpcOrder   = lpcA.order();
    f.epochs     = &epochRing;
    f.onsets     = &onsetRing;
    f.plosives   = &plosiveRing;

    for (int i = 0; i < n; ++i)
    {
        const int64_t t = f.startTime + i;
        if (t < 0)
        {
            f.env[i] = f.f0[i] = f.conf[i] = f.voiced[i] = f.fricative[i] = f.plosive[i] = 0.0f;
            f.periodicity[i] = f.fricScore[i] = f.residual[i] = 0.0f;
            f.envDb[i] = -120.0f;
            continue;
        }
        const auto h = (size_t) (t & histMask);
        f.env[i]         = rings[rEnv][h];
        f.envDb[i]       = rings[rEnvDb][h];
        f.f0[i]          = rings[rF0][h];
        f.conf[i]        = rings[rConf][h];
        f.voiced[i]      = rings[rVoiced][h];
        f.fricative[i]   = rings[rFric][h];
        f.plosive[i]     = rings[rPlos][h];
        f.periodicity[i] = rings[rPeriod][h];
        f.fricScore[i]   = rings[rFricScore][h];
        f.residual[i]    = rings[rResidual][h];
    }

    f.firstChunk = f.startTime >= 0 ? f.startTime / kLpcChunk : -((-f.startTime + kLpcChunk - 1) / kLpcChunk);
    for (int c = 0; c < AnalysisFrame::kMaxChunks; ++c)
    {
        const int64_t chunk = f.firstChunk + c;
        if (chunk < 0 || chunk * kLpcChunk >= timeNow)
        {
            std::fill (std::begin (f.k[c]), std::end (f.k[c]), 0.0f);
            f.lpcGain[c] = 0.0f;
            continue;
        }
        const auto ci = (size_t) (chunk & chunkMask);
        std::copy (kRing[ci].begin(), kRing[ci].end(), f.k[c]);
        f.lpcGain[c] = gainRing[ci];
    }
    const int64_t lastT = f.startTime + n - 1;
    f.formants = lastT >= 0 ? fmRing[(size_t) ((lastT / kLpcChunk) & chunkMask)] : Formants {};
}
} // namespace hv
