// Module catalogue: names + parameter specs. Single source of truth for ranges, mapping
// (normalized 0..1 <-> real units), display text and which knobs show on the slot card.
#pragma once
#include "Common.h"
#include <cstdio>
#include <string>

namespace hv
{
constexpr int kParamsPerSlot = 12;

enum class ModuleType : int
{
    Empty = 0, SourceDrive, PeriodDoubler, FryGate, FormantFreeze, Splitter, SyllableShuffle,
    CuppedMic, ContactMic, ModalRattle, MembraneBuzz, CarrierVocoder, SalivaClicks, Mob,
    NumTypes
};
constexpr int kNumModuleTypes = (int) ModuleType::NumTypes;

enum class Map : uint8_t { Linear, Log, Choice, Toggle, DbInf, Int };

struct ParamSpec
{
    const char* name = "";
    Map   map = Map::Linear;
    float min = 0.0f, max = 1.0f, def = 0.0f;
    const char* unit = "";
    std::vector<const char*> choices {};
    bool key = false;        // shown on the collapsed slot card
    bool reactive = false;   // has a live (input-reactive) value ring in the UI

    int numChoices() const noexcept { return (int) choices.size(); }
    bool continuous() const noexcept { return map == Map::Linear || map == Map::Log || map == Map::DbInf; }

    float toReal (float n) const noexcept
    {
        n = clampf (n, 0.0f, 1.0f);
        switch (map)
        {
            case Map::Linear: return min + n * (max - min);
            case Map::Log:    return min * std::pow (max / min, n);
            case Map::Choice: return (float) std::lround (n * (float) std::max (1, numChoices() - 1));
            case Map::Toggle: return n >= 0.5f ? 1.0f : 0.0f;
            case Map::Int:    return (float) std::lround (min + n * (max - min));
            case Map::DbInf:  return n <= 0.0005f ? -120.0f : min + n * (max - min);
        }
        return n;
    }
    float toNorm (float v) const noexcept
    {
        switch (map)
        {
            case Map::Linear:
            case Map::Int:    return clampf ((v - min) / (max - min), 0.0f, 1.0f);
            case Map::Log:    return clampf (std::log (std::max (v, 1.0e-9f) / min) / std::log (max / min), 0.0f, 1.0f);
            case Map::Choice: return numChoices() > 1 ? clampf (v / (float) (numChoices() - 1), 0.0f, 1.0f) : 0.0f;
            case Map::Toggle: return v >= 0.5f ? 1.0f : 0.0f;
            case Map::DbInf:  return v <= -119.0f ? 0.0f : clampf ((v - min) / (max - min), 0.001f, 1.0f);
        }
        return v;
    }
    std::string format (float real) const
    {
        char buf[64];
        switch (map)
        {
            case Map::Choice:
            {
                const int i = std::clamp ((int) real, 0, std::max (0, numChoices() - 1));
                return numChoices() > 0 ? std::string (choices[(size_t) i]) : std::string();
            }
            case Map::Toggle: return real >= 0.5f ? "On" : "Off";
            case Map::Int:    std::snprintf (buf, sizeof buf, "%d%s%s", (int) real, *unit ? " " : "", unit); return buf;
            case Map::DbInf:
                if (real <= -119.0f) return "-inf dB";
                [[fallthrough]];
            default: break;
        }
        const float a = std::abs (real);
        const int dec = a >= 1000.0f ? 0 : a >= 100.0f ? 0 : a >= 10.0f ? 1 : 2;
        if (std::string (unit) == "Hz" && a >= 1000.0f)
            std::snprintf (buf, sizeof buf, "%.2f kHz", real / 1000.0f);
        else
            std::snprintf (buf, sizeof buf, "%.*f%s%s", dec, real, *unit ? " " : "", unit);
        return buf;
    }
};

struct ModuleInfo
{
    const char* name;
    const char* shortName;
    std::vector<ParamSpec> params;
    bool stereoOut = false;
};

inline const std::vector<ModuleInfo>& moduleCatalogue()
{
    static const std::vector<ModuleInfo> cat = {
        { "Empty", "---", {} },
        { "Source Drive", "DRIVE", {
            { "Drive", Map::Linear, 0, 40, 18, "dB", {}, true, true },
            { "Asymmetry", Map::Linear, 0, 0.5f, 0.15f, "", {}, true },
            { "Rectify", Map::Linear, 0, 100, 0, "%", {}, true },
            { "Formant", Map::Linear, 0, 100, 100, "%", {}, true },
            { "React Amt", Map::Linear, -1, 1, 0.3f, "" },
            { "React Thr", Map::Linear, -60, 0, -30, "dB" },
            { "Analyze", Map::Choice, 0, 1, 0, "", { "Dry", "Local" } },
        } },
        { "Period Doubler", "GROWL", {
            { "Depth", Map::Linear, 0, 100, 70, "%", {}, true, true },
            { "Mode", Map::Choice, 0, 1, 0, "", { "/2", "/3" }, true },
            { "Skew", Map::Linear, 2, 8, 4, "%", {}, true },
            { "Pulse Noise", Map::DbInf, -60, 0, -18, "dB", {}, true },
            { "Noise Freq", Map::Log, 1000, 4000, 2000, "Hz" },
            { "Noise Width", Map::Linear, 0.3f, 2, 1, "oct" },
            { "Noise Decay", Map::Linear, 2, 8, 4, "ms" },
            { "React Amt", Map::Linear, -1, 1, 0.5f, "" },
            { "React Thr", Map::Linear, -60, 0, -24, "dB" },
        } },
        { "Fry Gate", "FRY", {
            { "Rate", Map::Log, 20, 90, 45, "Hz", {}, true, true },
            { "Jitter", Map::Linear, 10, 40, 25, "%", {}, true },
            { "Decay", Map::Linear, 5, 20, 10, "ms", {}, true },
            { "Floor", Map::DbInf, -60, 0, -30, "dB", {}, true },
            { "Rate Src", Map::Choice, 0, 1, 0, "", { "Free", "f0 / N" } },
            { "Divide N", Map::Int, 2, 16, 4, "" },
            { "React Amt", Map::Linear, -1, 1, 0, "" },
            { "React Thr", Map::Linear, -60, 0, -40, "dB" },
        } },
        { "Formant Freeze / Swap", "FORMANT", {
            { "Target", Map::Choice, 0, 7, 1, "", { "Capture", "Ah", "Uh", "Ee", "Oh", "Mirror", "Scale", "Pipe" }, true },
            { "Scale", Map::Linear, 0.5f, 1.8f, 1.3f, "x", {}, true },
            { "Pivot", Map::Log, 500, 4000, 1500, "Hz" },
            { "Pipe Len", Map::Log, 0.2f, 3, 1, "m" },
            { "Bandwidth", Map::Log, 0.25f, 4, 1, "x", {}, true },
            { "Tract Mix", Map::Linear, 0, 100, 100, "%", {}, true, true },
            { "React Amt", Map::Linear, -1, 1, 0, "" },
            { "React Thr", Map::Linear, -60, 0, -30, "dB" },
            { "Analyze", Map::Choice, 0, 1, 0, "", { "Dry", "Local" } },
        } },
        { "Consonant / Vowel Splitter", "SPLIT", {
            { "Voiced", Map::DbInf, -60, 12, 0, "dB", {}, true },
            { "Fricative", Map::DbInf, -60, 12, 0, "dB", {}, true },
            { "Plosive", Map::DbInf, -60, 12, 0, "dB", {}, true },
            { "Comb Time", Map::Log, 1, 10, 3, "ms" },
            { "Comb Fdbk", Map::Linear, 0, 0.95f, 0.0f, "" },
            { "Comb Damp", Map::Linear, 0, 100, 30, "%" },
            { "Burst Len", Map::Linear, 5, 30, 12, "ms" },
            { "Burst Tone", Map::Log, 300, 6000, 1500, "Hz" },
            { "Burst Lvl", Map::DbInf, -60, 0, -60, "dB" },
            { "Sensitivity", Map::Linear, 0, 100, 50, "%", {}, true },
        } },
        { "Syllable Reverse / Shuffle", "SYLLABLE", {
            { "Mode", Map::Choice, 0, 1, 0, "", { "Reverse", "Shuffle" }, true },
            { "Window", Map::Int, 200, 500, 300, "ms", {}, true },
            { "Min Seg", Map::Linear, 40, 150, 80, "ms", {}, true },
            { "Crossfade", Map::Linear, 5, 10, 7, "ms", {}, true },
            { "Seed", Map::Int, 0, 999, 1, "" },
        } },
        { "Cupped Mic", "CUPPED", {
            { "Proximity", Map::Linear, 12, 18, 15, "dB", {}, true },
            { "Hand Size", Map::Linear, 0, 1, 0.5f, "", {}, true },
            { "Tightness", Map::Linear, 0, 1, 0.5f, "" },
            { "Capsule", Map::Linear, 0, 30, 12, "dB", {}, true },
            { "Feedback", Map::Linear, 0, 0.98f, 0.8f, "", {}, true, true },
            { "Fb Delay", Map::Log, 2, 12, 5, "ms" },
            { "React Amt", Map::Linear, -1, 1, 0.5f, "" },
            { "React Thr", Map::Linear, -60, 0, -30, "dB" },
        } },
        { "Contact / Throat Mic", "CONTACT", {
            { "Tissue", Map::Linear, 0, 1, 0.5f, "", {}, true },
            { "Bone", Map::Linear, 0, 18, 9, "dB", {}, true },
            { "Clip", Map::Linear, 0, 30, 10, "dB", {}, true },
            { "De-ess", Map::Linear, 0, 100, 100, "%", {}, true },
            { "Bone Freq", Map::Log, 150, 300, 220, "Hz" },
        } },
        { "Modal Rattle", "RATTLE", {
            { "Material", Map::Choice, 0, 3, 0, "", { "Plate", "Drum", "Pipe", "Spring" }, true },
            { "Size", Map::Log, 0.25f, 4, 1, "x", {}, true },
            { "Decay", Map::Log, 0.05f, 2, 0.4f, "s" },
            { "Brightness", Map::Linear, 0, 1, 0.5f, "" },
            { "Gap", Map::Log, 0.01f, 1, 0.15f, "", {}, true },
            { "Rattle", Map::Linear, 0, 1, 0.5f, "", {}, true },
            { "Spread", Map::Linear, 0, 1, 0.6f, "" },
            { "Modes", Map::Int, 20, 60, 40, "" },
            { "Drive", Map::Linear, -12, 24, 0, "dB" },
        }, true },
        { "Membrane Buzz", "KAZOO", {
            { "Membrane", Map::Log, 200, 800, 400, "Hz", {}, true },
            { "Tension", Map::Linear, 0, 1, 0.5f, "", {}, true },
            { "Buzz Tone", Map::Log, 1000, 5000, 2500, "Hz", {}, true },
            { "Buzz Width", Map::Linear, 0.3f, 3, 1.2f, "oct" },
            { "Buzz Level", Map::Linear, -24, 12, 0, "dB", {}, true },
        } },
        { "Organic Carrier Vocoder", "CARRIER", {
            { "Carrier", Map::Choice, 0, 3, 0, "", { "Crackle", "Gravel", "Sizzle", "Wet" }, true },
            { "Density Min", Map::Log, 50, 8000, 200, "/s" },
            { "Density Max", Map::Log, 50, 8000, 3000, "/s", {}, true, true },
            { "Ping Low", Map::Log, 100, 12000, 1000, "Hz" },
            { "Ping High", Map::Log, 100, 12000, 6000, "Hz" },
            { "Ping Decay", Map::Log, 0.2f, 20, 2, "ms", {}, true },
            { "Env Source", Map::Choice, 0, 1, 0, "", { "LPC", "Filterbank" } },
            { "Pitch Lock", Map::Toggle, 0, 1, 0, "", {}, true },
            { "React Amt", Map::Linear, -1, 1, 1, "" },
            { "React Thr", Map::Linear, -60, 0, -50, "dB" },
        } },
        { "Saliva Clicks", "SALIVA", {
            { "Density", Map::Linear, 0, 400, 60, "/s", {}, true, true },
            { "Click Len", Map::Linear, 0.3f, 2, 0.8f, "ms", {}, true },
            { "Tone Low", Map::Log, 2000, 6000, 2500, "Hz" },
            { "Tone High", Map::Log, 2000, 6000, 5500, "Hz" },
            { "Fric Weight", Map::Linear, 0, 1, 0.5f, "", {}, true },
            { "Level", Map::Linear, -36, 12, 0, "dB", {}, true },
        } },
        { "Mob", "MOB", {
            { "Voices", Map::Int, 4, 16, 8, "", {}, true },
            { "Spread", Map::Linear, 0, 1, 0.6f, "", {}, true },
            { "Delay Range", Map::Linear, 30, 300, 150, "ms" },
            { "Body Range", Map::Linear, 0, 1, 0.5f, "" },
            { "Density Thr", Map::Linear, -60, 0, -40, "dB", {}, true, true },
            { "Gate Range", Map::Linear, 0, 36, 18, "dB" },
            { "Seed", Map::Int, 0, 999, 1, "" },
            { "Pitch Range", Map::Linear, 1, 7, 4, "st", {}, true },
        }, true },
    };
    return cat;
}

inline const ModuleInfo& moduleInfo (ModuleType t) { return moduleCatalogue()[(size_t) t]; }
inline const ModuleInfo& moduleInfo (int t) { return moduleCatalogue()[(size_t) std::clamp (t, 0, kNumModuleTypes - 1)]; }

/** Index of a parameter by name for a module type (-1 if absent). Used by presets/tests. */
inline int paramIndex (ModuleType t, const char* name)
{
    const auto& ps = moduleInfo (t).params;
    for (size_t i = 0; i < ps.size(); ++i)
        if (std::string (ps[i].name) == name) return (int) i;
    return -1;
}
} // namespace hv
