#include "Presets.h"
#include "PluginProcessor.h"

namespace hv
{
using namespace params;
using MT = ModuleType;

PresetBuilder::PresetBuilder (HarshVoxProcessor& proc) : p (proc) {}

PresetBuilder& PresetBuilder::slot (int s, ModuleType t)
{
    cur = s; curType = t;
    p.setSlotType (s, t);
    return *this;
}

PresetBuilder& PresetBuilder::set (const char* name, float real)
{
    const int k = paramIndex (curType, name);
    jassert (k >= 0);
    if (k >= 0) p.setParamNorm (slotIndex (cur, SP0 + k), moduleInfo (curType).params[(size_t) k].toNorm (real));
    return *this;
}

PresetBuilder& PresetBuilder::mix (float pct) { p.setParamReal (slotIndex (cur, SMix), pct); return *this; }
PresetBuilder& PresetBuilder::out (float db) { p.setParamReal (slotIndex (cur, SOut), db); return *this; }
PresetBuilder& PresetBuilder::on (bool enabled) { p.setParamReal (slotIndex (cur, SOn), enabled ? 1.0f : 0.0f); return *this; }

PresetBuilder& PresetBuilder::capture (std::initializer_list<float> freqs, std::initializer_list<float> bws)
{
    Formants f;
    f.count = (int) std::min<size_t> (freqs.size(), (size_t) kMaxFormants);
    int i = 0;
    for (float v : freqs) if (i < kMaxFormants) f.freq[i++] = v;
    i = 0;
    for (float v : bws) if (i < kMaxFormants) f.bw[i++] = v;
    p.engine().slotShared (cur).setCaptured (f);
    return *this;
}

PresetBuilder& PresetBuilder::global (int g, float real) { p.setParamReal (g, real); return *this; }

PresetBuilder& PresetBuilder::mod (int m, int source, int s, const char* paramName, float amount, int curve, float thr, float smoothMs)
{
    const int t = p.slotType (s);
    const int k = juce::String (paramName) == "Mix" ? kParamsPerSlot : paramIndex ((ModuleType) t, paramName);
    jassert (k >= 0);
    p.setParamReal (modIndex (m, MSrc), (float) source);
    p.setParamReal (modIndex (m, MTgt), (float) (1 + s * kTargetsPerSlot + std::max (0, k)));
    p.setParamReal (modIndex (m, MAmt), amount);
    p.setParamReal (modIndex (m, MCurve), (float) curve);
    p.setParamReal (modIndex (m, MThr), thr);
    p.setParamReal (modIndex (m, MSmooth), smoothMs);
    return *this;
}

namespace
{
struct Factory { const char* name; void (*build) (PresetBuilder&); };

const Factory kFactory[] = {
    { "Init", [] (PresetBuilder&) {} },

    { "Unintelligible Scream", [] (PresetBuilder& b) {
        b.slot (0, MT::FormantFreeze).set ("Target", 1).set ("Bandwidth", 1.4f).set ("Tract Mix", 85).set ("React Amt", 0.3f).set ("React Thr", -30);
        b.slot (1, MT::SourceDrive).set ("Drive", 26).set ("Asymmetry", 0.22f).set ("Rectify", 15).set ("Formant", 90).set ("React Amt", 0.4f);
        b.slot (2, MT::CuppedMic).set ("Proximity", 15).set ("Hand Size", 0.6f).set ("Tightness", 0.55f).set ("Capsule", 14).set ("Feedback", 0.72f).set ("React Amt", 0.5f).out (4);
    } },

    { "Inhuman Growl", [] (PresetBuilder& b) {
        b.slot (0, MT::PeriodDoubler).set ("Depth", 80).set ("Mode", 0).set ("Skew", 5).set ("Pulse Noise", -14).set ("Noise Freq", 1800).set ("React Amt", 0.4f);
        b.slot (1, MT::FryGate).set ("Rate", 48).set ("Jitter", 30).set ("Decay", 14).set ("Floor", -14).set ("React Amt", 0.5f).mix (70);
        b.slot (2, MT::ContactMic).set ("Tissue", 0.55f).set ("Bone", 10).set ("Clip", 16).set ("De-ess", 100).mix (85).out (-3);
    } },

    { "Industrial Hiss", [] (PresetBuilder& b) {
        b.slot (0, MT::Splitter).set ("Voiced", -14).set ("Fricative", 6).set ("Plosive", 3).set ("Comb Time", 4.2f)
            .set ("Comb Fdbk", 0.86f).set ("Comb Damp", 25).set ("Burst Lvl", -8).set ("Burst Tone", 2500).set ("Sensitivity", 65).out (6);
        b.slot (1, MT::ModalRattle).set ("Material", 0).set ("Size", 0.8f).set ("Decay", 0.6f).set ("Brightness", 0.75f)
            .set ("Gap", 0.08f).set ("Rattle", 0.75f).set ("Spread", 0.75f).mix (60).out (7);
    } },

    { "Speaking Gravel", [] (PresetBuilder& b) {
        b.slot (0, MT::SyllableShuffle).set ("Mode", 0).set ("Window", 300).set ("Min Seg", 80).set ("Crossfade", 7);
        b.slot (1, MT::CarrierVocoder).set ("Carrier", 1).set ("Density Min", 700).set ("Density Max", 6000)
            .set ("Ping Low", 250).set ("Ping High", 2600).set ("Ping Decay", 1.5f).set ("React Amt", 1).set ("React Thr", -45).out (-14);
    } },

    { "Riot", [] (PresetBuilder& b) {
        b.slot (0, MT::Mob).set ("Voices", 12).set ("Spread", 0.8f).set ("Delay Range", 220).set ("Body Range", 0.7f)
            .set ("Density Thr", -45).set ("Gate Range", 20).set ("Pitch Range", 5).mix (85);
        b.slot (1, MT::SourceDrive).set ("Drive", 22).set ("Asymmetry", 0.2f).set ("Rectify", 10);
        b.slot (2, MT::ModalRattle).set ("Material", 1).set ("Size", 1.3f).set ("Decay", 0.5f).set ("Gap", 0.12f).set ("Rattle", 0.6f).mix (40).out (-1);
    } },

    { "Gagged", [] (PresetBuilder& b) {
        b.slot (0, MT::Splitter).set ("Voiced", 0).set ("Fricative", -120).set ("Plosive", -120).set ("Sensitivity", 55);
        b.slot (8, MT::ContactMic).set ("Tissue", 1.0f).set ("Bone", 6).set ("Clip", 6).set ("De-ess", 100);   // V1 send: muffle the voiced path
        b.slot (1, MT::ContactMic).set ("Tissue", 0.7f).set ("Bone", 12).set ("Clip", 18).set ("De-ess", 100);
        b.slot (2, MT::SalivaClicks).set ("Density", 140).set ("Click Len", 0.7f).set ("Fric Weight", 0.3f).set ("Level", 3).out (-4);
    } },

    { "Drainpipe", [] (PresetBuilder& b) {
        b.slot (0, MT::FormantFreeze).set ("Target", 7).set ("Pipe Len", 2.0f).set ("Bandwidth", 1.2f).set ("Tract Mix", 100);
        b.slot (1, MT::MembraneBuzz).set ("Membrane", 350).set ("Tension", 0.6f).set ("Buzz Tone", 2200).set ("Buzz Width", 1.4f).set ("Buzz Level", 3);
    } },

    { "Feedback Throat", [] (PresetBuilder& b) {
        b.slot (0, MT::CuppedMic).set ("Proximity", 16).set ("Hand Size", 0.45f).set ("Tightness", 0.7f).set ("Capsule", 18)
            .set ("Feedback", 0.93f).set ("Fb Delay", 4).set ("React Amt", 0.6f).set ("React Thr", -32).out (4);
        b.slot (1, MT::PeriodDoubler).set ("Depth", 65).set ("Mode", 0).set ("Skew", 4).set ("Pulse Noise", -20).out (4);
    } },

    // ---- extras
    { "Oil Drum Sermon", [] (PresetBuilder& b) {
        b.slot (0, MT::SourceDrive).set ("Drive", 16).set ("Asymmetry", 0.1f);
        b.slot (1, MT::ModalRattle).set ("Material", 2).set ("Size", 1.6f).set ("Decay", 0.9f).set ("Brightness", 0.4f).set ("Gap", 0.2f).set ("Rattle", 0.8f).mix (55);
        b.slot (2, MT::FryGate).set ("Rate Src", 1).set ("Divide N", 3).set ("Jitter", 20).set ("Floor", -9).mix (60).out (4);
    } },

    { "Swallowed Tongue", [] (PresetBuilder& b) {
        b.slot (0, MT::FormantFreeze).set ("Target", 5).set ("Pivot", 1300).set ("Tract Mix", 100);
        b.slot (1, MT::SalivaClicks).set ("Density", 200).set ("Fric Weight", 0.7f);
        b.slot (2, MT::SourceDrive).set ("Drive", 12).set ("Formant", 100).out (-1);
        b.mod (0, SrcEnvelope, 0, "Tract Mix", -0.5f, 0, 0.3f, 40.0f);
    } },

    { "Shuffled Tongues", [] (PresetBuilder& b) {
        b.slot (0, MT::SyllableShuffle).set ("Mode", 1).set ("Window", 450).set ("Min Seg", 70);
        b.slot (1, MT::FormantFreeze).set ("Target", 6).set ("Scale", 1.35f).set ("Tract Mix", 100);
        b.slot (2, MT::CuppedMic).set ("Feedback", 0.6f).set ("Capsule", 10).out (6);
    } },

    { "Crackling Throat Fire", [] (PresetBuilder& b) {
        b.slot (0, MT::CarrierVocoder).set ("Carrier", 0).set ("Density Min", 80).set ("Density Max", 2500)
            .set ("Ping Low", 1000).set ("Ping High", 6000).set ("Ping Decay", 0.8f).set ("Pitch Lock", 0).mix (75);
        b.slot (1, MT::ContactMic).set ("Tissue", 0.2f).set ("Clip", 8).mix (40).out (5);
    } },

    { "Lo-Fi Fizz Mouth", [] (PresetBuilder& b) {
        b.slot (0, MT::SourceDrive).set ("Drive", 40).set ("Asymmetry", 0.0f).set ("Rectify", 60).set ("Formant", 25);
        b.slot (1, MT::MembraneBuzz).set ("Tension", 0.9f).set ("Buzz Tone", 4200).set ("Buzz Level", 6).out (1);
    } },
};
} // namespace

int numFactoryPresets() { return (int) std::size (kFactory); }
juce::String factoryPresetName (int i) { return i >= 0 && i < numFactoryPresets() ? kFactory[i].name : ""; }

namespace
{
juce::String num (float v, bool integral)
{
    if (integral) return juce::String ((int) std::lround (v));
    juce::String s (v, 4);
    while (s.containsChar ('.') && s.endsWithChar ('0')) s = s.dropLastCharacters (1);
    if (s.endsWithChar ('.')) s = s.dropLastCharacters (1);
    if (! s.containsChar ('.')) return s;   // exact integer: plain literal converts fine
    return s + "f";
}

const char* typeEnumName (int t)
{
    static const char* const n[] = { "Empty", "SourceDrive", "PeriodDoubler", "FryGate", "FormantFreeze", "Splitter",
                                      "SyllableShuffle", "CuppedMic", "ContactMic", "ModalRattle", "MembraneBuzz",
                                      "CarrierVocoder", "SalivaClicks", "Mob" };
    return n[std::clamp (t, 0, kNumModuleTypes - 1)];
}

bool differs (float a, float b) { return std::abs (a - b) > 1.0e-4f * std::max (1.0f, std::abs (b)); }
} // namespace

juce::String exportPresetCode (HarshVoxProcessor& p, const juce::String& name)
{
    const auto defs = buildParamDefs();
    juce::String o;
    o << "    { \"" << name.replace ("\"", "'") << "\", [] (PresetBuilder& b) {\n";
    for (int s = 0; s < kNumSlots; ++s)
    {
        const int t = p.slotType (s);
        if (t <= 0) continue;
        const auto& specs = moduleInfo (t).params;
        juce::String line;
        line << "        b.slot (" << s << ", MT::" << typeEnumName (t) << ")";
        for (int k = 0; k < (int) specs.size(); ++k)
        {
            const auto& sp = specs[(size_t) k];
            const float v = sp.toReal (p.param (slotIndex (s, SP0 + k))->getValue());
            if (differs (v, sp.toReal (sp.toNorm (sp.def))))
                line << ".set (\"" << sp.name << "\", " << num (v, ! sp.continuous()) << ")";
        }
        const float mixV = p.paramReal (slotIndex (s, SMix)), outV = p.paramReal (slotIndex (s, SOut));
        if (differs (mixV, 100.0f)) line << ".mix (" << num (mixV, false) << ")";
        if (differs (outV, 0.0f)) line << ".out (" << num (outV, false) << ")";
        if (p.paramReal (slotIndex (s, SOn)) < 0.5f) line << ".on (false)";
        const auto cap = p.engine().slotShared (s).getCaptured();
        if (cap.count > 0)
        {
            juce::String fs, bs;
            for (int i = 0; i < cap.count; ++i)
            {
                fs << (i ? ", " : "") << num (cap.freq[i], false);
                bs << (i ? ", " : "") << num (cap.bw[i], false);
            }
            line << ".capture ({ " << fs << " }, { " << bs << " })";
        }
        if (s >= kMainSlots) line << ";   // send " << slotLabel (s) << "\n";
        else line << ";\n";
        o << line;
    }
    static const char* const gNames[] = { "InGain", "OutGain", "Mix", "Seed", "EnvAtk", "EnvRel" };
    for (int g = 0; g < kNumGlobal; ++g)
    {
        const float v = p.paramReal (g);
        if (differs (v, defs[(size_t) g].def)) o << "        b.global (" << gNames[g] << ", " << num (v, g == Seed) << ");\n";
    }
    static const char* const srcNames[] = { "SrcNone", "SrcEnvelope", "SrcPitch", "SrcPitchConf", "SrcVoiced", "SrcFricative", "SrcPlosive" };
    for (int m = 0; m < kNumMods; ++m)
    {
        const int src = (int) std::lround (p.paramReal (modIndex (m, MSrc)));
        const int tgt = (int) std::lround (p.paramReal (modIndex (m, MTgt)));
        if (src <= 0 || tgt <= 0) continue;
        const int s = (tgt - 1) / kTargetsPerSlot, k = (tgt - 1) % kTargetsPerSlot;
        const int t = p.slotType (s);
        juce::String pname = k == kParamsPerSlot ? "Mix" : (k < (int) moduleInfo (t).params.size() ? moduleInfo (t).params[(size_t) k].name : "");
        if (pname.isEmpty()) { o << "        // mod " << (m + 1) << " targets an unused parameter (skipped)\n"; continue; }
        o << "        b.mod (" << m << ", " << srcNames[std::clamp (src, 0, kNumSources - 1)] << ", " << s << ", \"" << pname << "\", "
          << num (p.paramReal (modIndex (m, MAmt)), false) << ", " << (int) std::lround (p.paramReal (modIndex (m, MCurve))) << ", "
          << num (p.paramReal (modIndex (m, MThr)), false) << ", " << num (p.paramReal (modIndex (m, MSmooth)), false) << ");\n";
    }
    o << "    } },\n";
    return o;
}

void applyFactoryPreset (HarshVoxProcessor& p, int index)
{
    if (index < 0 || index >= numFactoryPresets()) return;
    p.resetToDefaults();
    PresetBuilder b (p);
    kFactory[index].build (b);
    p.currentPresetName = kFactory[index].name;
}
} // namespace hv
