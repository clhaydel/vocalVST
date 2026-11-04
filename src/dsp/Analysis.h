// Shared analysis engine. Runs once per block on the dry (mono) input; modules get read-only
// AnalysisFrames, optionally delayed by the latency upstream of their slot so analysis stays aligned
// with the audio they actually receive.
#pragma once
#include "Common.h"
#include "Fft.h"
#include "Lpc.h"
#include "Yin.h"
#include <atomic>

namespace hv
{
constexpr int kMaxSubBlock = 64;   // the chain processes in sub-blocks of at most this many samples

/** Fixed-capacity ring of timestamped events (epochs, onsets, plosives). Audio thread only. */
class EventRing
{
public:
    struct Event { int64_t time = 0; float value = 0.0f; };
    static constexpr int kCap = 2048;

    void clear() noexcept { count = 0; head = 0; }
    void push (int64_t t, float v) noexcept
    {
        ev[(size_t) head] = { t, v };
        head = (head + 1) % kCap;
        count = std::min (count + 1, kCap);
    }
    int size() const noexcept { return count; }
    /** i = 0 is the newest event. */
    const Event& fromNewest (int i) const noexcept { return ev[(size_t) ((head - 1 - i + kCap * 2) % kCap)]; }

    /** First event with time >= t (searching newest→oldest); returns false if none. */
    bool firstAtOrAfter (int64_t t, Event& out) const noexcept
    {
        bool found = false;
        for (int i = 0; i < count; ++i)
        {
            const auto& e = fromNewest (i);
            if (e.time < t) break;
            out = e; found = true;
        }
        return found;
    }
    /** Latest event with time <= t. */
    bool lastAtOrBefore (int64_t t, Event& out) const noexcept
    {
        for (int i = 0; i < count; ++i)
        {
            const auto& e = fromNewest (i);
            if (e.time <= t) { out = e; return true; }
        }
        return false;
    }

private:
    std::array<Event, kCap> ev {};
    int head = 0, count = 0;
};

struct AnalysisFrame
{
    static constexpr int kMaxChunks = kMaxSubBlock / kLpcChunk + 2;

    int     numSamples = 0;
    double  sampleRate = 48000.0;
    int64_t startTime  = 0;   // absolute (dry-timeline) time of sample 0 of this frame
    int64_t knownUntil = 0;   // events up to this time are present in the rings (lookahead for delayed slots)
    int     lpcOrder   = 24;

    float env[kMaxSubBlock] {};        // linear envelope
    float envDb[kMaxSubBlock] {};
    float f0[kMaxSubBlock] {};         // Hz (held from last confident estimate when unvoiced)
    float conf[kMaxSubBlock] {};       // pitch confidence 0..1
    float voiced[kMaxSubBlock] {};     // soft class probabilities
    float fricative[kMaxSubBlock] {};
    float plosive[kMaxSubBlock] {};
    float periodicity[kMaxSubBlock] {};// raw classifier features (for per-module sensitivity)
    float fricScore[kMaxSubBlock] {};
    float residual[kMaxSubBlock] {};   // dry LPC excitation (pre-emphasised domain)

    float k[kMaxChunks][kMaxLpcOrder] {};
    float lpcGain[kMaxChunks] {};
    int64_t firstChunk = 0;
    Formants formants;

    const EventRing* epochs   = nullptr;
    const EventRing* onsets   = nullptr;
    const EventRing* plosives = nullptr;

    const float* kForSample (int i) const noexcept
    {
        const int64_t c = (startTime + i) / kLpcChunk - firstChunk;
        return k[std::clamp ((int) c, 0, kMaxChunks - 1)];
    }
    float gainForSample (int i) const noexcept
    {
        const int64_t c = (startTime + i) / kLpcChunk - firstChunk;
        return lpcGain[std::clamp ((int) c, 0, kMaxChunks - 1)];
    }
};

/** Lock-free telemetry for the analysis display (written by audio thread, read by UI). */
struct AnalysisTelemetry
{
    struct Point { float envDb, f0, conf, voiced, fricative, plosive; };
    static constexpr int kHistory = 4096;
    std::array<Point, kHistory> hist {};
    std::atomic<int> writeIndex { 0 };
    std::array<std::atomic<float>, kEnvPoints> lpcCurve {};
    std::array<std::atomic<float>, kMaxFormants * 2> formants {};
    std::atomic<int> numFormants { 0 };
    std::atomic<float> sampleRate { 48000.0f };
};

class AnalysisEngine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setEnvelopeTimes (float attackMs, float releaseMs) noexcept { envF.setTimes (attackMs, releaseMs); }

    /** Analyse n (<= kMaxSubBlock) new dry mono samples. */
    void process (const float* dry, int n) noexcept;

    /** Fill `f` for the most recent n samples, delayed by `delaySamples` (0 = aligned with dry input). */
    void makeFrame (int delaySamples, int n, AnalysisFrame& f) const noexcept;

    int64_t now() const noexcept { return timeNow; }
    double  sampleRate() const noexcept { return sr; }
    int     lpcOrder() const noexcept { return lpcA.order(); }
    int     maxDelay() const noexcept { return histLen - 2 * kMaxSubBlock; }

    /** Latest block-level values (for modulation sources). */
    struct Latest { float env = 0, envDb = -120, f0 = 0, conf = 0, voiced = 0, fricative = 0, plosive = 0; bool plosiveTrig = false; };
    const Latest& latest() const noexcept { return last; }

    AnalysisTelemetry& telemetry() noexcept { return tele; }
    const EventRing& epochs() const noexcept { return epochRing; }
    const EventRing& onsets() const noexcept { return onsetRing; }

private:
    void classifyHop() noexcept;
    void detectEpoch (float residualMag, int64_t t) noexcept;

    double sr = 48000.0;
    int64_t timeNow = 0;

    // per-sample history rings
    int histLen = 0, histMask = 0;
    enum Ring { rEnv, rEnvDb, rF0, rConf, rVoiced, rFric, rPlos, rPeriod, rFricScore, rResidual, kNumRings };
    std::array<std::vector<float>, kNumRings> rings;
    int chunkLen = 0, chunkMask = 0;
    std::vector<std::array<float, kMaxLpcOrder>> kRing;
    std::vector<float> gainRing;
    std::vector<Formants> fmRing;

    EnvelopeFollower envF;
    LpcAnalyzer lpcA;
    YinTracker yin;

    // classifier / onset state
    Fft fft;
    int fftSize = 1024, hop = 240, hopCount = 0, specPos = 0;
    std::vector<float> specRing, specFrame, specWin, mag, prevLogMag;
    float fluxHist[16] {};
    int fluxPos = 0;
    float prevFlux[2] {};
    float energyHist[12] {};
    int energyPos = 0;
    int zcrCount = 0;
    float lastSample = 0.0f;
    int64_t lastOnset = -1000000, lastPlosive = -1000000;
    float hopVoiced = 0, hopFric = 0, hopPeriod = 0, hopFricScore = 0;
    float sVoiced = 0, sFric = 0, sPlos = 0, smoothC = 0.01f, plosDecay = 0.999f;
    float heldF0 = 0.0f, curF0 = 0.0f, curConf = 0.0f, f0Glide = 0.01f;

    // epoch detector
    int64_t lastEpoch = -1, candPos = -1;
    float candVal = 0.0f;
    OnePole epochSmooth;

    EventRing epochRing, onsetRing, plosiveRing;
    Latest last;
    AnalysisTelemetry tele;
};
} // namespace hv
