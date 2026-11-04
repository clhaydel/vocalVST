// Harsh Vox processing engine: analysis, slot chain (with splitter sends), modulation, global section.
// JUCE-GUI-free so it can be driven headless by tests, the render tool and the benchmark.
#pragma once
#include "Analysis.h"
#include "Params.h"
#include "VocalModule.h"

namespace hv
{
/** Transparent look-ahead peak limiter (sliding-minimum gain + box smoothing => never overshoots). */
class Limiter
{
public:
    void prepare (double sr, float ceilingDb = -0.3f, float lookaheadMs = 1.0f, float releaseMs = 60.0f);
    void reset();
    void process (float* L, float* R, int n) noexcept;
    int latency() const noexcept { return look; }

private:
    float ceiling = 0.966f, relC = 0.001f, state = 1.0f;
    double boxSum = 0.0;
    int look = 48, pos = 0;
    std::vector<float> dL, dR, req, box;
    // monotonic deque for the sliding minimum
    std::vector<float> dqVal; std::vector<int64_t> dqIdx; int dqHead = 0, dqTail = 0, dqCap = 0;
    int64_t t = 0;
};

class Engine
{
public:
    Engine();
    ~Engine();

    void bind (const params::ParamRefs& refs) { P = refs; }
    void prepare (double sampleRate, int maxBlock);
    void reset();

    /** inR may be nullptr (mono input). Output is always stereo. */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;

    /** Total latency (chain + limiter) implied by the current parameter values. Any thread. */
    int latencyFromParams() const noexcept;
    int limiterLatency() const noexcept { return limiter.latency(); }

    AnalysisEngine& analysis() noexcept { return ana; }
    SlotShared& slotShared (int s) noexcept { return *shared[(size_t) s]; }
    double sampleRate() const noexcept { return sr; }

    std::atomic<float> inPeak { 0.0f }, outPeakL { 0.0f }, outPeakR { 0.0f };

    /** Real parameter values of slot s's 12 params (no modulation). */
    void slotValues (int s, int type, float* out) const noexcept;

private:
    struct Slot
    {
        std::array<std::unique_ptr<VocalModule>, kNumModuleTypes> mods;
        int active = -1;
        Smoother mix, gain;
        std::array<CompDelay, 2> dry;
        std::array<CompDelay, 6> align;   // splitter path alignment (3 paths x 2 ch)
        float values[kParamsPerSlot] {};
        float live[kParamsPerSlot + 1] {};
        float dryBuf[2][kMaxSubBlock] {};
    };

    void processSub (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept;
    int  runSlot (int s, juce::AudioBuffer<float>& buf, int n, bool& stereo, int upstream, bool allowSends) noexcept;
    void computeModulation() noexcept;
    int  slotLatencyFromParams (int s, bool allowSends) const noexcept;
    bool splitterHasSends() const noexcept;

    params::ParamRefs P;
    double sr = 48000.0;
    int maxBlock = 512;
    AnalysisEngine ana;
    std::array<Slot, params::kNumSlots> slots;
    std::array<std::unique_ptr<SlotShared>, params::kNumSlots> shared;
    float modOff[params::kNumSlots][params::kTargetsPerSlot] {};
    float modState[params::kNumMods] {};

    // scratch buffers (preallocated)
    juce::AudioBuffer<float> chainStore, dryStore, pathStore[3];
    juce::AudioBuffer<float> chainView, pathView[3];
    std::vector<float> mono;
    CompDelay globalDry;
    Smoother inGainS, outGainS, mixS;
    DcBlocker dcL, dcR;
    Limiter limiter;
    uint64_t globalSeed = 1;
};
} // namespace hv
