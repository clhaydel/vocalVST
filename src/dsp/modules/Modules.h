#pragma once
#include "../VocalModule.h"

namespace hv
{
std::unique_ptr<VocalModule> makeSourceDrive();
std::unique_ptr<VocalModule> makeFormantFreeze();
std::unique_ptr<VocalModule> makeSalivaClicks();
std::unique_ptr<VocalModule> makePeriodDoubler();
std::unique_ptr<VocalModule> makeFryGate();
std::unique_ptr<VocalModule> makeCuppedMic();
std::unique_ptr<VocalModule> makeContactMic();
std::unique_ptr<VocalModule> makeMembraneBuzz();
std::unique_ptr<VocalModule> makeModalRattle();
std::unique_ptr<VocalModule> makeSplitter();
std::unique_ptr<VocalModule> makeSyllableShuffle();
std::unique_ptr<VocalModule> makeMob();
std::unique_ptr<VocalModule> makeCarrierVocoder();

/** The splitter can expose its three paths separately so the engine can run sub-chains on them. */
struct SplitterPaths
{
    virtual ~SplitterPaths() = default;
    /** Write voiced / fricative / plosive paths (each already gained) for the block in `in`. */
    virtual void processPaths (const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>* paths[3],
                               const AnalysisFrame& frame) = 0;
};

/** Latency of Syllable Reverse/Shuffle for a given window (ms). */
int syllableLatency (float windowMs, double sr);

// Shared constants
constexpr float kPeriodDoublerLatencyMs = 30.0f;
constexpr float kSyllableExtraLatencyMs = 12.0f;   // onset-detection delay on top of the window
} // namespace hv
