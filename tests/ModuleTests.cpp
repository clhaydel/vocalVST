#include "TestHelpers.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace hv;
using namespace hvtest;

namespace
{
std::vector<ModuleType> allTypes()
{
    std::vector<ModuleType> t;
    for (int i = 1; i < kNumModuleTypes; ++i) t.push_back ((ModuleType) i);
    return t;
}
}

TEST_CASE ("Every module: silence in -> silence out", "[modules][silence]")
{
    const double sr = 48000;
    for (auto t : allTypes())
    {
        ModuleRig rig (t, sr);
        // prime with a burst of voice so internal states are non-trivial, then 2.5 s of silence
        auto x = synthVoice (VoiceStyle::Screamed, sr, 0.5, 2);
        x.resize ((size_t) (3.0 * sr), 0.0f);
        auto y = rig.run (x);
        INFO (moduleInfo (t).name);
        REQUIRE (allFinite (y[0]));
        REQUIRE (allFinite (y[1]));
        const int tail = (int) (2.7 * sr);
        if (t == ModuleType::CuppedMic)
            REQUIRE (peakAbs (y[0], tail) < 1.0f);   // feedback loop allowed to ring, but bounded
        else
        {
            REQUIRE (peakAbs (y[0], tail) < 1.0e-4f);
            REQUIRE (peakAbs (y[1], tail) < 1.0e-4f);
        }
        // pure digital silence from the start must not generate output (no self-oscillation)
        ModuleRig rig2 (t, sr);
        auto z = rig2.run (silence ((int) sr));
        REQUIRE (peakAbs (z[0]) < 1.0e-6f);
    }
}

TEST_CASE ("Every module: no NaN/Inf and bounded on hostile input", "[modules][robust]")
{
    const double sr = 48000;
    const int n = (int) (1.0 * sr);
    const std::vector<std::pair<const char*, Signal>> inputs = {
        { "white", whiteNoise (n, 1.0f, 5) },
        { "square", square (n, sr, 55.0, 1.0f) },
        { "dc", dc (n, 1.0f) },
        { "impulses", impulses (n, 4801, 1.0f) },
        { "scream", synthVoice (VoiceStyle::Screamed, sr, 1.0, 8) },
    };
    for (auto t : allTypes())
        for (const auto& [name, x] : inputs)
        {
            ModuleRig rig (t, sr);
            // extreme settings: every continuous param at max
            for (const auto& p : moduleInfo (t).params) if (p.continuous()) rig.setParam (p.name, p.max);
            auto y = rig.run (x);
            INFO (moduleInfo (t).name << " / " << name);
            REQUIRE (allFinite (y[0]));
            REQUIRE (allFinite (y[1]));
            REQUIRE (peakAbs (y[0]) < 70.0f);
        }
}

TEST_CASE ("Every module: deterministic for the same seed", "[modules][determinism]")
{
    const double sr = 48000;
    auto x = synthVoice (VoiceStyle::Spoken, sr, 1.5, 6);
    for (auto t : allTypes())
    {
        ModuleRig a (t, sr, 42), b (t, sr, 42);
        auto ya = a.run (x), yb = b.run (x);
        INFO (moduleInfo (t).name);
        REQUIRE (std::memcmp (ya[0].data(), yb[0].data(), ya[0].size() * sizeof (float)) == 0);
        REQUIRE (std::memcmp (ya[1].data(), yb[1].data(), ya[1].size() * sizeof (float)) == 0);
        // and re-running the same instance after reset() is identical too
        a.mod->reset(); a.ana.reset();
        auto yc = a.run (x);
        REQUIRE (std::memcmp (ya[0].data(), yc[0].data(), ya[0].size() * sizeof (float)) == 0);
    }
}

TEST_CASE ("Period Doubler at zero depth is a pure delay of its reported latency", "[modules][latency]")
{
    const double sr = GENERATE (44100.0, 48000.0, 96000.0);
    ModuleRig rig (ModuleType::PeriodDoubler, sr);
    rig.setParam ("Depth", 0);
    rig.setParam ("Pulse Noise", -120);
    rig.setParam ("React Amt", 0);
    auto x = synthVoice (VoiceStyle::Sung, sr, 1.0, 3);
    auto y = rig.run (x);
    const int L = rig.mod->getLatencySamples();
    REQUIRE (L == (int) std::lround (0.03 * sr));
    double err = 0, ref = 0;
    for (size_t i = (size_t) L; i < x.size(); ++i) { err += std::pow (y[0][i] - x[i - (size_t) L], 2.0); ref += std::pow (x[i - (size_t) L], 2.0); }
    INFO ("relative error " << std::sqrt (err / ref));
    REQUIRE (std::sqrt (err / ref) < 0.05);
}

TEST_CASE ("Syllable Reverse keeps energy and reports window-sized latency", "[modules][latency]")
{
    const double sr = 48000;
    ModuleRig rig (ModuleType::SyllableShuffle, sr);
    rig.setParam ("Window", 300);
    ProcessContext ctx; float v[12] {}; std::copy (rig.values, rig.values + 12, v);
    ctx.values = v; rig.mod->setContext (ctx);
    REQUIRE (rig.mod->getLatencySamples() == (int) std::lround (0.312 * sr) + kMaxSubBlock);
    auto x = synthVoice (VoiceStyle::Spoken, sr, 3.0, 5);
    auto y = rig.run (x);
    const int L = rig.mod->getLatencySamples();
    const float rIn = rms (x, 0, (int) x.size() - L), rOut = rms (y[0], L, (int) x.size());
    REQUIRE (rOut == Catch::Approx (rIn).epsilon (0.25));
}

TEST_CASE ("Sample-rate independence of key frequencies", "[modules][samplerate]")
{
    std::vector<double> rates { 44100.0, 48000.0, 96000.0, 192000.0 };

    SECTION ("Modal Rattle fundamental (Plate, Drum, Pipe)")
    {
        for (int material : { 0, 1, 2 })
        {
            std::vector<double> peaks;
            for (double sr : rates)
            {
                ModuleRig rig (ModuleType::ModalRattle, sr);
                rig.setParam ("Material", (float) material);
                rig.setParam ("Gap", 1.0f); rig.setParam ("Rattle", 0.0f); rig.setParam ("Decay", 1.0f);
                rig.setParam ("Spread", 0.0f);
                auto y = rig.run (impulses ((int) sr, (int) sr * 2, 0.05f));
                const double expect = (material == 2 ? 110.0 : 150.0);
                peaks.push_back (spectralPeak (y[0], sr, expect * 0.8, expect * 1.2));
            }
            for (double p : peaks) REQUIRE (p == Catch::Approx (material == 2 ? 110.0 : 150.0).epsilon (0.02));
        }
    }
    SECTION ("Cupped Mic first hand resonance")
    {
        std::vector<double> peaks;
        for (double sr : rates)
        {
            ModuleRig rig (ModuleType::CuppedMic, sr);
            rig.setParam ("Feedback", 0.0f); rig.setParam ("Capsule", 0.0f); rig.setParam ("Hand Size", 0.5f);
            rig.setParam ("Proximity", 12.0f); rig.setParam ("Tightness", 1.0f); rig.setParam ("React Amt", 0.0f);
            auto y = rig.run (impulses ((int) (sr * 0.5), (int) sr, 0.01f));
            peaks.push_back (spectralPeak (y[0], sr, 600, 900));
        }
        for (double p : peaks) REQUIRE (p == Catch::Approx (peaks[1]).epsilon (0.02));
        REQUIRE (peaks[1] == Catch::Approx (750.0).epsilon (0.05));
    }
    SECTION ("Formant Freeze 'ee' F2 resonance")
    {
        std::vector<double> peaks;
        for (double sr : rates)
        {
            ModuleRig rig (ModuleType::FormantFreeze, sr);
            rig.setParam ("Target", 3);   // ee: F2 = 2290 Hz
            auto y = rig.run (whiteNoise ((int) (3 * sr), 0.1f, 3));
            peaks.push_back (spectralPeak (y[0], sr, 1900, 2700));
        }
        for (double p : peaks) REQUIRE (p == Catch::Approx (2290.0).epsilon (0.02));
    }
}

TEST_CASE ("Formant Mirror targets stay in the speech band", "[modules][formant]")
{
    const double sr = 48000;
    std::vector<Signal> in; double fsr = 0;
    REQUIRE (readWav (juce::File (juce::String (HV_SOURCE_DIR) + "/test_vocals/cmu_spoken_slt.wav"), in, fsr));
    ModuleRig rig (ModuleType::FormantFreeze, fsr);
    rig.setParam ("Target", 5);  // Mirror
    Signal x (in[0].begin(), in[0].begin() + (long) (1.5 * fsr));
    rig.run (x);
    for (int i = 0; i < 5; ++i)
    {
        const float f = rig.shared.targetFormants[(size_t) i].load();
        INFO ("F" << (i + 1) << " = " << f << "  bw " << rig.shared.targetFormants[(size_t) i + 5].load());
        CHECK (f > 150.0f);
        CHECK (f < 9001.0f);
    }
    juce::ignoreUnused (sr);
}
