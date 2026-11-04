// Parameter layout shared by the plugin (APVTS) and headless tests. JUCE-free.
#pragma once
#include "ModuleInfo.h"
#include <string>
#include <array>
#include <atomic>

namespace hv::params
{
constexpr int kMainSlots = 8;
constexpr int kSendSlots = 6;               // splitter sub-chains: voiced A/B, fricative A/B, plosive A/B
constexpr int kNumSlots  = kMainSlots + kSendSlots;
constexpr int kNumMods   = 4;

enum Global { InGain, OutGain, Mix, Seed, EnvAtk, EnvRel, kNumGlobal };
enum ModField { MSrc, MTgt, MAmt, MCurve, MThr, MSmooth, kModFields };
enum SlotField { SType, SOn, SMix, SOut, SP0, kSlotFields = SP0 + kParamsPerSlot };

enum ModSource { SrcNone, SrcEnvelope, SrcPitch, SrcPitchConf, SrcVoiced, SrcFricative, SrcPlosive, kNumSources };
inline const char* const kSourceNames[] = { "None", "Envelope", "Pitch", "Pitch Conf", "Voiced", "Fricative", "Plosive" };
inline const char* const kCurveNames[]  = { "Linear", "Exp", "Log" };

constexpr int kTargetsPerSlot = kParamsPerSlot + 1;   // 12 module params + slot mix
constexpr int kNumTargets     = 1 + kNumSlots * kTargetsPerSlot;  // 0 = none

constexpr int globalIndex (int g) { return g; }
constexpr int modIndex (int m, int f) { return kNumGlobal + m * kModFields + f; }
constexpr int slotIndex (int s, int f) { return kNumGlobal + kNumMods * kModFields + s * kSlotFields + f; }
constexpr int kNumParams = kNumGlobal + kNumMods * kModFields + kNumSlots * kSlotFields;

inline std::string slotLabel (int s)
{
    static const char* const sends[] = { "V1", "V2", "F1", "F2", "P1", "P2" };
    return s < kMainSlots ? std::to_string (s + 1) : std::string (sends[s - kMainSlots]);
}

struct ParamDef
{
    std::string id, name;
    float min = 0, max = 1, def = 0;
    bool isInt = false, isBool = false;
    std::vector<std::string> choices;
    bool automatable = true;
};

inline std::vector<ParamDef> buildParamDefs()
{
    std::vector<ParamDef> d ((size_t) kNumParams);
    auto set = [&] (int idx, ParamDef p) { d[(size_t) idx] = std::move (p); };
    set (InGain,  { "in_gain", "Input Gain", -24, 24, 0 });
    set (OutGain, { "out_gain", "Output Gain", -24, 24, 0 });
    set (Mix,     { "mix", "Global Mix", 0, 100, 100 });
    set (Seed,    { "seed", "Seed", 0, 9999, 1, true });
    set (EnvAtk,  { "env_atk", "Env Attack", 1, 5, 2 });
    set (EnvRel,  { "env_rel", "Env Release", 30, 100, 60 });
    for (int m = 0; m < kNumMods; ++m)
    {
        const std::string p = "m" + std::to_string (m + 1) + "_", n = "Mod " + std::to_string (m + 1) + " ";
        ParamDef src { p + "src", n + "Source", 0, (float) kNumSources - 1, 0 };
        for (auto* s : kSourceNames) src.choices.push_back (s);
        set (modIndex (m, MSrc), src);
        set (modIndex (m, MTgt), { p + "tgt", n + "Target", 0, (float) kNumTargets - 1, 0, true });
        set (modIndex (m, MAmt), { p + "amt", n + "Amount", -1, 1, 0 });
        ParamDef cv { p + "curve", n + "Curve", 0, 2, 0 };
        for (auto* s : kCurveNames) cv.choices.push_back (s);
        set (modIndex (m, MCurve), cv);
        set (modIndex (m, MThr), { p + "thr", n + "Threshold", 0, 1, 0 });
        set (modIndex (m, MSmooth), { p + "smooth", n + "Smoothing", 1, 200, 20 });
    }
    for (int s = 0; s < kNumSlots; ++s)
    {
        const std::string p = "s" + std::to_string (s + 1) + "_", n = "S" + slotLabel (s) + " ";
        ParamDef ty { p + "type", n + "Module", 0, (float) kNumModuleTypes - 1, 0 };
        for (const auto& mi : moduleCatalogue()) ty.choices.push_back (mi.name);
        set (slotIndex (s, SType), ty);
        ParamDef on { p + "on", n + "On", 0, 1, 1 };
        on.isBool = true;
        set (slotIndex (s, SOn), on);
        set (slotIndex (s, SMix), { p + "mix", n + "Mix", 0, 100, 100 });
        set (slotIndex (s, SOut), { p + "out", n + "Output", -24, 24, 0 });
        for (int k = 0; k < kParamsPerSlot; ++k)
            set (slotIndex (s, SP0 + k), { p + "p" + std::to_string (k + 1), n + "P" + std::to_string (k + 1), 0, 1, 0 });
    }
    return d;
}

/** Real-valued parameter storage the engine reads from (APVTS raw values in the plugin). */
struct ParamRefs
{
    std::array<const std::atomic<float>*, (size_t) kNumParams> p {};
    float get (int idx) const noexcept { return p[(size_t) idx] != nullptr ? p[(size_t) idx]->load (std::memory_order_relaxed) : 0.0f; }
};
} // namespace hv::params
