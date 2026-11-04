#pragma once
#include "dsp/Engine.h"
#include "dsp/modules/Modules.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>

namespace hvtest
{
using Signal = std::vector<float>;

Signal silence (int n);
Signal whiteNoise (int n, float amp, uint64_t seed = 1);
Signal square (int n, double sr, double freq, float amp = 1.0f);
Signal dc (int n, float v);
Signal impulses (int n, int spacing, float amp = 1.0f);
Signal sine (int n, double sr, double freq, float amp);
/** Glottal-pulse train (Rosenberg flow derivative) at f0 through vowel formants. */
Signal glottalVowel (int n, double sr, double f0, const float* formants = nullptr, float jitter = 0.0f, uint64_t seed = 1);

enum class VoiceStyle { Spoken, Sung, Screamed, Whispered };
/** Deterministic synthetic vocal material for listening tests and analysis checks. */
Signal synthVoice (VoiceStyle style, double sr, double seconds, uint64_t seed = 1);

bool writeWav (const juce::File& f, const std::vector<Signal>& channels, double sr);
bool readWav (const juce::File& f, std::vector<Signal>& channels, double& sr);

/** Owns atomics for every engine parameter (real values), initialised to defaults. */
struct ParamStore
{
    ParamStore();
    std::vector<std::unique_ptr<std::atomic<float>>> v;
    hv::params::ParamRefs refs() const;
    void set (int idx, float real) { v[(size_t) idx]->store (real); }
    float get (int idx) const { return v[(size_t) idx]->load(); }
    /** Put module `t` in slot s with default params. */
    void setSlot (int s, hv::ModuleType t);
    void setSlotParam (int s, const char* name, float real);
};

/** Run one module standalone (mono in, 2-ch buffer out) with a fresh analysis engine fed by the input. */
struct ModuleRig
{
    ModuleRig (hv::ModuleType t, double sr, uint64_t seed = 1);
    void setParam (const char* name, float real);
    /** Process a mono signal; returns {L, R}. */
    std::vector<Signal> run (const Signal& in, int block = 64);

    hv::ModuleType type;
    double sr;
    std::unique_ptr<hv::VocalModule> mod;
    hv::AnalysisEngine ana;
    float values[hv::kParamsPerSlot] {};
    float live[hv::kParamsPerSlot + 1] {};
    hv::SlotShared shared;
    uint64_t seed;
};

float peakAbs (const Signal& s, int from = 0);
float rms (const Signal& s, int from = 0, int to = -1);
bool allFinite (const Signal& s);
/** Frequency (Hz) of the largest magnitude bin within [lo, hi] of the signal's spectrum. */
double spectralPeak (const Signal& s, double sr, double lo, double hi);
} // namespace hvtest
