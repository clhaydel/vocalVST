#include "TestHelpers.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace hv;
using namespace hvtest;

TEST_CASE ("Lattice analysis/synthesis reconstruct exactly with time-varying k", "[analysis][lpc]")
{
    const double sr = 48000;
    auto x = synthVoice (VoiceStyle::Spoken, sr, 1.0, 3);
    LpcAnalyzer a; a.prepare (sr, false);
    LatticeSynthesis syn; DeEmphasis de;
    float maxErr = 0.0f;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const float e = a.processSample (x[i]);
        const float y = de.process (syn.process (e, a.currentK(), a.order()));
        maxErr = std::max (maxErr, std::abs (y - x[i]));
    }
    REQUIRE (maxErr < 2.0e-3f);
}

TEST_CASE ("LPC reflection coefficients stay inside the unit circle", "[analysis][lpc]")
{
    const double sr = GENERATE (44100.0, 48000.0, 96000.0, 192000.0);
    LpcAnalyzer a; a.prepare (sr, true);
    for (int which = 0; which < 3; ++which)
    {
        a.reset();
        Signal x = which == 0 ? whiteNoise ((int) sr, 0.8f, 9)
                 : which == 1 ? synthVoice (VoiceStyle::Screamed, sr, 1.0, 4)
                              : square ((int) sr, sr, 97.0, 1.0f);
        float maxK = 0.0f;
        for (float s : x)
        {
            a.processSample (s);
            for (int m = 0; m < a.order(); ++m) maxK = std::max (maxK, std::abs (a.currentK()[m]));
        }
        INFO ("sr " << sr << " signal " << which);
        REQUIRE (maxK < 1.0f);   // all poles strictly inside the unit circle
    }
    REQUIRE (lpcOrderForRate (44100) == 24);
    REQUIRE (lpcOrderForRate (192000) == 40);
}

TEST_CASE ("YIN tracks synthetic glottal pulses", "[analysis][yin]")
{
    const double sr = GENERATE (44100.0, 48000.0, 96000.0);
    for (double f0 : { 82.0, 110.0, 165.0, 247.0, 392.0, 660.0 })
    {
        auto x = glottalVowel ((int) (0.6 * sr), sr, f0, nullptr, 0.005f);
        YinTracker y; y.prepare (sr);
        double sum = 0; int cnt = 0; float confSum = 0;
        for (size_t i = 0; i < x.size(); ++i)
            if (y.push (x[i]) && i > (size_t) (0.15 * sr)) { sum += y.frequency(); confSum += y.confidence(); ++cnt; }
        INFO ("sr " << sr << " f0 " << f0);
        REQUIRE (cnt > 10);
        REQUIRE (sum / cnt == Catch::Approx (f0).epsilon (0.02));
        REQUIRE (confSum / cnt > 0.8f);
    }
}

TEST_CASE ("Epoch detector marks one epoch per glottal period", "[analysis][epochs]")
{
    const double sr = 48000, f0 = 150.0;
    auto x = glottalVowel ((int) sr, sr, f0, nullptr, 0.0f);
    AnalysisEngine a; a.prepare (sr, 64);
    for (size_t i = 0; i < x.size(); i += 64) a.process (x.data() + i, (int) std::min<size_t> (64, x.size() - i));
    const auto& ep = a.epochs();
    REQUIRE (ep.size() > 100);
    // spacing of the last 50 epochs ~ one period
    double sum = 0; int cnt = 0;
    for (int i = 0; i < 50; ++i) { sum += (double) (ep.fromNewest (i).time - ep.fromNewest (i + 1).time); ++cnt; }
    REQUIRE (sum / cnt == Catch::Approx (sr / f0).epsilon (0.03));
}

TEST_CASE ("Classifier separates voiced, fricative and silence", "[analysis][classifier]")
{
    const double sr = 48000;
    Signal x;
    auto v = glottalVowel ((int) (0.5 * sr), sr, 140.0);
    Biquad hp; hp.highpass (sr, 4500.0, 0.7);
    auto nz = whiteNoise ((int) (0.5 * sr), 0.3f, 4);
    for (auto& s : nz) s = hp.process (s);
    x.insert (x.end(), v.begin(), v.end());
    x.insert (x.end(), nz.begin(), nz.end());
    x.resize (x.size() + (size_t) (0.3 * sr), 0.0f);
    AnalysisEngine a; a.prepare (sr, 64);
    float vV = 0, vF = 0, fV = 0, fF = 0, sV = 0, sF = 0;
    for (size_t i = 0; i < x.size(); i += 64)
    {
        a.process (x.data() + i, 64);
        const auto& l = a.latest();
        const double t = (double) i / sr;
        if (t > 0.2 && t < 0.45) { vV = std::max (vV, l.voiced); vF = std::max (vF, l.fricative); }
        if (t > 0.7 && t < 0.95) { fV = std::max (fV, l.voiced); fF = std::max (fF, l.fricative); }
        if (t > 1.15) { sV = std::max (sV, l.voiced); sF = std::max (sF, l.fricative); }
    }
    REQUIRE (vV > 0.8f); REQUIRE (vF < 0.2f);
    REQUIRE (fF > 0.6f); REQUIRE (fV < 0.2f);
    REQUIRE (sV < 0.05f); REQUIRE (sF < 0.05f);
}

TEST_CASE ("Onset detector finds syllable boundaries", "[analysis][onsets]")
{
    const double sr = 48000;
    Signal x ((size_t) (2.0 * sr), 0.0f);
    std::vector<int> starts;
    for (int k = 0; k < 6; ++k)
    {
        const int st = (int) ((0.1 + 0.3 * k) * sr);
        starts.push_back (st);
        auto v = glottalVowel ((int) (0.18 * sr), sr, 120.0 + 20 * k);
        for (size_t i = 0; i < v.size(); ++i) x[(size_t) st + i] = v[i] * (float) std::min (1.0, i / (0.005 * sr));
    }
    AnalysisEngine a; a.prepare (sr, 64);
    for (size_t i = 0; i < x.size(); i += 64) a.process (x.data() + i, 64);
    const auto& on = a.onsets();
    int matched = 0;
    for (int st : starts)
        for (int i = 0; i < on.size(); ++i)
            if (std::abs ((double) (on.fromNewest (i).time - st)) < 0.03 * sr) { ++matched; break; }
    REQUIRE (matched == (int) starts.size());
    REQUIRE (on.size() <= 12);
}
