#include "TestHelpers.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Presets.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace hv;
using namespace hv::params;
using namespace hvtest;

namespace
{
/** Render a mono signal through a processor (block size 128). */
std::vector<Signal> renderProc (HarshVoxProcessor& p, const Signal& x, double sr, int block = 128)
{
    p.setPlayConfigDetails (2, 2, sr, block);
    p.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    std::vector<Signal> out (2, Signal (x.size(), 0.0f));
    for (size_t pos = 0; pos < x.size(); pos += (size_t) block)
    {
        const int len = (int) std::min<size_t> ((size_t) block, x.size() - pos);
        buf.setSize (2, len, false, false, true);
        for (int c = 0; c < 2; ++c) std::copy (x.begin() + (long) pos, x.begin() + (long) pos + len, buf.getWritePointer (c));
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) std::copy (buf.getReadPointer (c), buf.getReadPointer (c) + len, out[(size_t) c].begin() + (long) pos);
    }
    return out;
}

int firstAbove (const Signal& s, float thr)
{
    for (size_t i = 0; i < s.size(); ++i) if (std::abs (s[i]) > thr) return (int) i;
    return -1;
}
}

TEST_CASE ("Factory presets render deterministically (bit-identical)", "[engine][determinism]")
{
    const double sr = 48000;
    auto x = synthVoice (VoiceStyle::Screamed, sr, 1.5, 11);
    for (int i = 0; i < numFactoryPresets(); ++i)
    {
        HarshVoxProcessor a, b;
        applyFactoryPreset (a, i); applyFactoryPreset (b, i);
        auto ya = renderProc (a, x, sr), yb = renderProc (b, x, sr);
        INFO (factoryPresetName (i));
        REQUIRE (allFinite (ya[0]));
        REQUIRE (std::memcmp (ya[0].data(), yb[0].data(), ya[0].size() * sizeof (float)) == 0);
        REQUIRE (std::memcmp (ya[1].data(), yb[1].data(), ya[1].size() * sizeof (float)) == 0);
    }
}

TEST_CASE ("Reported latency matches measured impulse delay", "[engine][latency]")
{
    const double sr = 48000;
    // every module with slot mix 0 (dry compensation path), plus combinations
    std::vector<std::vector<ModuleType>> chains;
    for (int t = 1; t < kNumModuleTypes; ++t) chains.push_back ({ (ModuleType) t });
    chains.push_back ({ ModuleType::PeriodDoubler, ModuleType::SyllableShuffle, ModuleType::PeriodDoubler });
    chains.push_back ({ ModuleType::SyllableShuffle, ModuleType::Mob, ModuleType::SyllableShuffle, ModuleType::CuppedMic });
    for (const auto& chain : chains)
    {
        HarshVoxProcessor p;
        p.resetToDefaults();
        for (size_t s = 0; s < chain.size(); ++s)
        {
            p.setSlotType ((int) s, chain[s]);
            p.setParamReal (slotIndex ((int) s, SMix), 0.0f);
        }
        Signal x ((size_t) (2.0 * sr), 0.0f);
        x[(size_t) (0.1 * sr)] = 0.5f;
        auto y = renderProc (p, x, sr);
        const int measured = firstAbove (y[0], 0.05f) - (int) (0.1 * sr);
        INFO ("chain starting " << moduleInfo (chain[0]).name << " len " << chain.size());
        REQUIRE (measured == p.getLatencySamples());
    }
    SECTION ("Splitter sends: path latencies aligned and reported")
    {
        HarshVoxProcessor p;
        p.resetToDefaults();
        p.setSlotType (0, ModuleType::Splitter);
        p.setSlotType (8, ModuleType::PeriodDoubler);       // V1
        p.setParamReal (slotIndex (8, SMix), 0.0f);
        p.setSlotType (10, ModuleType::SyllableShuffle);    // F1
        p.setParamReal (slotIndex (10, SMix), 0.0f);
        Signal x ((size_t) (2.0 * sr), 0.0f);
        x[(size_t) (0.1 * sr)] = 0.5f;
        auto y = renderProc (p, x, sr);
        REQUIRE (p.getLatencySamples() == hv::syllableLatency (300.0f, sr) + p.engine().limiterLatency());
        REQUIRE (firstAbove (y[0], 0.05f) - (int) (0.1 * sr) == p.getLatencySamples());
    }
}

TEST_CASE ("Global mix 0 passes the input through, aligned", "[engine]")
{
    const double sr = 48000;
    HarshVoxProcessor p;
    applyFactoryPreset (p, 4);  // Speaking Gravel (has lookahead)
    p.setParamReal (Mix, 0.0f);
    auto x = synthVoice (VoiceStyle::Spoken, sr, 1.0, 2);
    for (auto& s : x) s *= 0.5f;
    auto y = renderProc (p, x, sr);
    const int L = p.getLatencySamples();
    float err = 0.0f;
    for (size_t i = (size_t) L + 2000; i < x.size(); ++i) err = std::max (err, std::abs (y[0][i] - x[i - (size_t) L]));
    REQUIRE (err < 0.01f);
}

TEST_CASE ("Output limiter never exceeds -0.3 dBFS", "[engine][limiter]")
{
    const double sr = 48000;
    HarshVoxProcessor p;
    applyFactoryPreset (p, 5);  // Riot
    p.setParamReal (InGain, 24.0f);
    p.setParamReal (OutGain, 24.0f);
    auto x = synthVoice (VoiceStyle::Screamed, sr, 1.5, 7);
    auto y = renderProc (p, x, sr);
    REQUIRE (peakAbs (y[0]) <= dbToGain (-0.3f) + 1.0e-6f);
    REQUIRE (peakAbs (y[1]) <= dbToGain (-0.3f) + 1.0e-6f);
}

TEST_CASE ("State round-trip keeps chain, params and captured formants", "[engine][state]")
{
    HarshVoxProcessor a;
    applyFactoryPreset (a, 1);
    Formants f; f.count = 3; f.freq[0] = 600; f.freq[1] = 1400; f.freq[2] = 2600;
    a.engine().slotShared (0).setCaptured (f);
    a.setParamReal (slotIndex (1, SP0), 0.77f);
    juce::MemoryBlock mb;
    a.getStateInformation (mb);
    HarshVoxProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    for (int i = 0; i < kNumParams; ++i) REQUIRE (b.param (i)->getValue() == Catch::Approx (a.param (i)->getValue()));
    const auto g = b.engine().slotShared (0).getCaptured();
    REQUIRE (g.count == 3);
    REQUIRE (g.freq[1] == Catch::Approx (1400.0f));
    REQUIRE (b.currentPresetName == a.currentPresetName);
}

TEST_CASE ("Drag-to-reorder moves modules with their parameters", "[engine][routing]")
{
    HarshVoxProcessor p;
    applyFactoryPreset (p, 1);   // Formant -> Drive -> Cupped
    const float driveBefore = p.param (slotIndex (1, SP0))->getValue();
    p.moveSlot (0, 2);           // Formant moves to the end
    REQUIRE (p.slotType (0) == (int) ModuleType::SourceDrive);
    REQUIRE (p.slotType (1) == (int) ModuleType::CuppedMic);
    REQUIRE (p.slotType (2) == (int) ModuleType::FormantFreeze);
    REQUIRE (p.param (slotIndex (0, SP0))->getValue() == Catch::Approx (driveBefore));
    REQUIRE (p.param (slotIndex (0, SP0))->getName (64) == "S1 Drive");
}

TEST_CASE ("Modulation slot drives its target", "[engine][modulation]")
{
    const double sr = 48000;
    HarshVoxProcessor p;
    p.resetToDefaults();
    PresetBuilder b (p);
    b.slot (0, ModuleType::SourceDrive).set ("Drive", 0).set ("React Amt", 0);
    b.mod (0, SrcEnvelope, 0, "Drive", 1.0f);
    auto x = synthVoice (VoiceStyle::Sung, sr, 0.5, 1);
    renderProc (p, x, sr);
    REQUIRE (p.engine().slotShared (0).liveNorm[0].load() > 0.3f);
}

TEST_CASE ("Preset export emits builder code for the current settings", "[engine][presets]")
{
    HarshVoxProcessor p;
    applyFactoryPreset (p, 10);   // Swallowed Tongue: 3 slots + a mod
    p.setParamReal (slotIndex (2, SOn), 0.0f);
    p.setParamReal (Seed, 42.0f);
    const auto code = exportPresetCode (p, "Test Export");
    INFO (code);
    REQUIRE (code.contains ("{ \"Test Export\", [] (PresetBuilder& b) {"));
    REQUIRE (code.contains ("b.slot (0, MT::FormantFreeze).set (\"Target\", 5).set (\"Pivot\", 1300)"));
    REQUIRE (code.contains ("b.slot (1, MT::SalivaClicks).set (\"Density\", 200)"));
    REQUIRE (code.contains (".on (false)"));
    REQUIRE (code.contains ("b.global (Seed, 42);"));
    REQUIRE (code.contains ("b.mod (0, SrcEnvelope, 0, \"Tract Mix\", -0.5f, 0, 0.3f, 40);"));
}

TEST_CASE ("Preset export round-trips through an .hvx file", "[engine][presets]")
{
    HarshVoxProcessor p;
    applyFactoryPreset (p, 6);   // Gagged: splitter + V1 send
    Formants f; f.count = 2; f.freq[0] = 640; f.freq[1] = 1190; f.bw[0] = 80; f.bw[1] = 95;
    p.engine().slotShared (1).setCaptured (f);
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hv_export_test.hvx");
    REQUIRE (p.savePresetFile (file));
    HarshVoxProcessor q;
    REQUIRE (q.loadPresetFile (file));
    const auto code = exportPresetCode (q, "Gagged Copy");
    INFO (code);
    REQUIRE (code.contains ("b.slot (8, MT::ContactMic)"));
    REQUIRE (code.contains ("// send V1"));
    REQUIRE (code.contains (".capture ({ 640, 1190 }, { 80, 95 })"));
    file.deleteFile();
}

namespace
{
/** Render separate L/R signals through a processor (block size 128). */
std::vector<Signal> renderStereo (HarshVoxProcessor& p, const Signal& l, const Signal& r, double sr, int block = 128)
{
    p.setPlayConfigDetails (2, 2, sr, block);
    p.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    std::vector<Signal> out (2, Signal (l.size(), 0.0f));
    for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
    {
        const int len = (int) std::min<size_t> ((size_t) block, l.size() - pos);
        buf.setSize (2, len, false, false, true);
        std::copy (l.begin() + (long) pos, l.begin() + (long) pos + len, buf.getWritePointer (0));
        std::copy (r.begin() + (long) pos, r.begin() + (long) pos + len, buf.getWritePointer (1));
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) std::copy (buf.getReadPointer (c), buf.getReadPointer (c) + len, out[(size_t) c].begin() + (long) pos);
    }
    return out;
}
}

TEST_CASE ("Input Mode: stereo keeps L/R, mono sums; default is stereo and presets keep it", "[engine][stereo]")
{
    const double sr = 48000;
    const Signal l = synthVoice (VoiceStyle::Sung, sr, 0.6, 3);
    const Signal r ((size_t) l.size(), 0.0f);   // hard-left source

    SECTION ("default is stereo, and survives factory and Init preset loads")
    {
        HarshVoxProcessor p;
        REQUIRE (p.paramReal (InputMode) == (float) InputStereo);
        p.setParamReal (InputMode, (float) InputMono);
        applyFactoryPreset (p, 1);
        REQUIRE (p.paramReal (InputMode) == (float) InputMono);
        applyFactoryPreset (p, 0);
        REQUIRE (p.paramReal (InputMode) == (float) InputMono);
        REQUIRE (! exportPresetCode (p, "x").contains ("InputMode"));
    }
    SECTION ("empty chain: stereo passes L/R through, mono puts the sum on both sides")
    {
        HarshVoxProcessor st;
        auto ys = renderStereo (st, l, r, sr);
        const int L = st.getLatencySamples();
        double eL = 0, eR = 0;
        for (size_t i = 0; i + (size_t) L < l.size(); ++i) { eL += ys[0][i + (size_t) L] * ys[0][i + (size_t) L]; eR += ys[1][i] * ys[1][i]; }
        REQUIRE (eL > 1.0);
        REQUIRE (eR < 1.0e-9 * eL);

        HarshVoxProcessor mo;
        mo.setParamReal (InputMode, (float) InputMono);
        auto ym = renderStereo (mo, l, r, sr);
        REQUIRE (std::memcmp (ym[0].data(), ym[1].data(), ym[0].size() * sizeof (float)) == 0);
    }
    SECTION ("dual-mono input in stereo mode equals mono mode (mono-chain modules)")
    {
        for (auto t : { ModuleType::SourceDrive, ModuleType::PeriodDoubler, ModuleType::CuppedMic, ModuleType::SalivaClicks })
        {
            HarshVoxProcessor a, b;
            a.setSlotType (0, t); b.setSlotType (0, t);
            b.setParamReal (InputMode, (float) InputMono);
            auto ya = renderStereo (a, l, l, sr), yb = renderStereo (b, l, l, sr);
            INFO (moduleInfo (t).name);
            REQUIRE (std::memcmp (ya[0].data(), yb[0].data(), ya[0].size() * sizeof (float)) == 0);
            REQUIRE (std::memcmp (ya[1].data(), yb[1].data(), ya[1].size() * sizeof (float)) == 0);
        }
    }
}

TEST_CASE ("Saliva Clicks at 100% mix outputs only clicks (no dry voice)", "[engine][modules]")
{
    const double sr = 48000;
    const auto x = synthVoice (VoiceStyle::Spoken, sr, 1.0, 5);
    HarshVoxProcessor p;
    PresetBuilder b (p);
    b.slot (0, ModuleType::SalivaClicks).set ("Density", 0);   // no clicks at all -> silence
    auto y = renderProc (p, x, sr);
    double e = 0;
    for (float v : y[0]) e += (double) v * v;
    REQUIRE (e < 1.0e-12);
}
