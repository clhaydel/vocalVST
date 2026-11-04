// Common module interface. DSP only — no JUCE GUI dependencies (juce::AudioBuffer is from juce_audio_basics).
#pragma once
#include "Analysis.h"
#include "ModuleInfo.h"
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>

namespace hv
{
/** Per-slot data shared between audio thread and UI / state (lock-free). */
struct SlotShared
{
    std::atomic<bool> captureRequest { false };
    std::array<std::atomic<float>, kMaxFormants * 2> captured {};   // freq[5], bw[5]
    std::atomic<int> capturedCount { 0 };
    std::array<std::atomic<float>, kMaxFormants * 2> targetFormants {};  // for analysis-display overlay
    std::atomic<int> targetCount { 0 };
    std::array<std::atomic<float>, kParamsPerSlot + 1> liveNorm {};  // live modulated values (12 params + mix)
    std::atomic<float> level { 0.0f };

    void setCaptured (const Formants& f) noexcept
    {
        for (int i = 0; i < kMaxFormants; ++i)
        {
            captured[(size_t) i].store (f.freq[i]);
            captured[(size_t) (i + kMaxFormants)].store (f.bw[i]);
        }
        capturedCount.store (f.count);
    }
    Formants getCaptured() const noexcept
    {
        Formants f;
        f.count = capturedCount.load();
        for (int i = 0; i < kMaxFormants; ++i)
        {
            f.freq[i] = captured[(size_t) i].load();
            f.bw[i]   = captured[(size_t) (i + kMaxFormants)].load();
        }
        return f;
    }
};

/** Everything a module needs for one block besides audio + analysis. */
struct ProcessContext
{
    const float* values   = nullptr;   // real-unit parameter targets (after modulation), kParamsPerSlot
    float*       liveNorm = nullptr;   // module may overwrite with live reactive values (normalized)
    int          numChannels = 1;      // 1 while the chain is still mono, 2 after a stereo module
    uint64_t     seed = 1;             // derived from the global Seed + slot (+ module Seed param)
    SlotShared*  shared = nullptr;
};

struct VocalModule
{
    virtual ~VocalModule() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void process (juce::AudioBuffer<float>&, const AnalysisFrame&) = 0;
    virtual int  getLatencySamples() const = 0;
    virtual void reset() = 0;

    // ---- Harsh Vox extensions
    /** Called before every process() with this block's parameters. */
    virtual void setContext (const ProcessContext& c) { ctx = c; }
    /** True if this module writes a genuinely stereo signal to channels 0 and 1. */
    virtual bool producesStereo() const { return false; }

protected:
    ProcessContext ctx;
};

/** Helper base: sample rate, per-parameter smoothers (20 ms), seeded PRNG handling. */
class ModuleBase : public VocalModule
{
public:
    explicit ModuleBase (ModuleType t) : type (t) {}

    void prepare (double sampleRate, int maxBlock) override
    {
        sr = sampleRate;
        maxBlockSize = maxBlock;
        for (auto& s : sm) s.prepare (sr, 20.0f);
        prepareModule();
        reset();
    }

    void reset() override
    {
        needsSnap = true;
        lastSeed = ~0ULL;
        resetModule();
    }

    int getLatencySamples() const override { return 0; }

    void setContext (const ProcessContext& c) override
    {
        ctx = c;
        const auto& specs = moduleInfo (type).params;
        for (size_t i = 0; i < specs.size() && i < (size_t) kParamsPerSlot; ++i)
        {
            if (needsSnap || ! specs[i].continuous()) sm[i].snap (c.values[i]);
            else sm[i].setTarget (c.values[i]);
        }
        needsSnap = false;
        if (c.seed != lastSeed) { lastSeed = c.seed; onSeedChanged (c.seed); }
    }

protected:
    virtual void prepareModule() {}
    virtual void resetModule() {}
    virtual void onSeedChanged (uint64_t) {}

    /** Raw (unsmoothed) real value — for choices/toggles/ints. */
    float raw (int i) const noexcept
    {
        if (ctx.values != nullptr) return ctx.values[i];
        const auto& ps = moduleInfo (type).params;   // before the first block: spec default
        return i < (int) ps.size() ? ps[(size_t) i].def : 0.0f;
    }
    int   choice (int i) const noexcept { return (int) std::lround (raw (i)); }
    /** Smoothed value advanced by n samples (block-rate coefficient updates). */
    float block (int i, int n) noexcept { return sm[(size_t) i].skip (n); }
    /** Smoothed value, per-sample. */
    float next (int i) noexcept { return sm[(size_t) i].next(); }
    void  setLive (int i, float real) noexcept
    {
        if (ctx.liveNorm != nullptr) ctx.liveNorm[i] = moduleInfo (type).params[(size_t) i].toNorm (real);
    }
    int channels (const juce::AudioBuffer<float>& b) const noexcept { return std::min (ctx.numChannels, b.getNumChannels()); }

    ModuleType type;
    double sr = 48000.0;
    int maxBlockSize = kMaxSubBlock;
    std::array<Smoother, kParamsPerSlot> sm {};
    bool needsSnap = true;
    uint64_t lastSeed = ~0ULL;
};

/** Latency a module of `type` reports for the given real parameter values (callable from any thread). */
int moduleLatencySamples (ModuleType type, const float* realValues, double sampleRate);

std::unique_ptr<VocalModule> createModule (ModuleType type);
} // namespace hv
