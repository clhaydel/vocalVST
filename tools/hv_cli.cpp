// hv_cli — headless utility: render presets over test vocals, benchmark modules, generate test vocals.
//
//   hv_cli render  --in <wav|folder> --out <folder> [--preset <name|all>] [--sr <rate>]
//   hv_cli bench   [--sr 48000] [--block 128] [--seconds 10]
//   hv_cli gen-vocals --out <folder> [--sr 48000]
//   hv_cli list-presets
#include "TestHelpers.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Presets.h"
#include <chrono>
#include <iostream>

using namespace hv;
using namespace hvtest;

namespace
{
juce::String arg (const juce::StringArray& a, const juce::String& key, const juce::String& def = {})
{
    const int i = a.indexOf (key);
    return i >= 0 && i + 1 < a.size() ? a[i + 1] : def;
}

std::vector<Signal> renderProc (HarshVoxProcessor& p, const std::vector<Signal>& in, double sr, int block, int tailSamples)
{
    const size_t n = in[0].size() + (size_t) tailSamples;
    p.setPlayConfigDetails ((int) std::min<size_t> (2, in.size()), 2, sr, block);
    p.prepareToPlay (sr, block);
    const int latency = p.getLatencySamples();
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    std::vector<Signal> out (2, Signal (n + (size_t) latency, 0.0f));
    for (size_t pos = 0; pos < n + (size_t) latency; pos += (size_t) block)
    {
        const int len = (int) std::min<size_t> ((size_t) block, n + (size_t) latency - pos);
        buf.setSize (2, len, false, false, true);
        for (int c = 0; c < 2; ++c)
        {
            const auto& src = in[std::min<size_t> ((size_t) c, in.size() - 1)];
            float* d = buf.getWritePointer (c);
            for (int i = 0; i < len; ++i) d[i] = pos + (size_t) i < src.size() ? src[pos + (size_t) i] : 0.0f;
        }
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) std::copy (buf.getReadPointer (c), buf.getReadPointer (c) + len, out[(size_t) c].begin() + (long) pos);
    }
    // remove plugin latency so renders line up with the source
    for (auto& c : out) c.erase (c.begin(), c.begin() + latency);
    return out;
}

int cmdRender (const juce::StringArray& a)
{
    const juce::File inArg (juce::File::getCurrentWorkingDirectory().getChildFile (arg (a, "--in", "test_vocals")));
    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (arg (a, "--out", "renders")));
    const juce::String presetArg = arg (a, "--preset", "all");
    juce::Array<juce::File> files;
    if (inArg.isDirectory()) files = inArg.findChildFiles (juce::File::findFiles, false, "*.wav");
    else files.add (inArg);
    files.sort();
    if (files.isEmpty()) { std::cerr << "no input wavs in " << inArg.getFullPathName() << "\n"; return 1; }

    std::vector<int> presets;
    for (int i = 0; i < numFactoryPresets(); ++i)
        if (presetArg == "all" ? i > 0 : factoryPresetName (i).equalsIgnoreCase (presetArg)) presets.push_back (i);
    if (presets.empty()) { std::cerr << "unknown preset " << presetArg << "\n"; return 1; }

    for (int pi : presets)
    {
        const auto pname = factoryPresetName (pi);
        for (const auto& f : files)
        {
            std::vector<Signal> in; double sr = 0;
            if (! readWav (f, in, sr)) { std::cerr << "cannot read " << f.getFullPathName() << "\n"; continue; }
            if (a.contains ("--sr")) jassert (std::abs (sr - arg (a, "--sr").getDoubleValue()) < 1);
            HarshVoxProcessor p;
            applyFactoryPreset (p, pi);
            for (int k = 0; k + 1 < a.size(); ++k)   // --disable <slot 1-14> (repeatable) for bisecting chains
                if (a[k] == "--disable") p.setParamReal (params::slotIndex (a[k + 1].getIntValue() - 1, params::SOn), 0.0f);
            const auto t0 = std::chrono::steady_clock::now();
            auto out = renderProc (p, in, sr, 128, (int) (0.5 * sr));
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            const auto dst = outDir.getChildFile (pname.replaceCharacter (' ', '_'))
                                 .getChildFile (f.getFileNameWithoutExtension() + "__" + pname.replaceCharacter (' ', '_') + ".wav");
            writeWav (dst, out, sr);
            const double dur = (double) in[0].size() / sr;
            std::printf ("%-24s %-28s peak %6.1f dBFS  rms %6.1f dBFS  latency %5d  %.1fx realtime\n", pname.toRawUTF8(),
                         f.getFileName().toRawUTF8(), gainToDb (std::max (peakAbs (out[0]), peakAbs (out[1]))),
                         gainToDb (rms (out[0])), p.getLatencySamples(), dur / secs);
        }
    }
    return 0;
}

int cmdBench (const juce::StringArray& a)
{
    const double sr = arg (a, "--sr", "48000").getDoubleValue();
    const int block = arg (a, "--block", "128").getIntValue();
    const double seconds = arg (a, "--seconds", "10").getDoubleValue();
    auto x = synthVoice (VoiceStyle::Screamed, sr, seconds, 3);
    const double blockBudgetUs = block / sr * 1.0e6;
    std::printf ("Benchmark @ %.0f Hz, %d-sample blocks (budget %.1f us/block)\n", sr, block, blockBudgetUs);
    std::printf ("%-28s %10s %10s\n", "module", "us/block", "% core");

    auto runChain = [&] (const char* label, auto setup)
    {
        HarshVoxProcessor p;
        p.resetToDefaults();
        setup (p);
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        const int nb = (int) (x.size() / (size_t) block);
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < nb; ++b)
        {
            for (int c = 0; c < 2; ++c) std::copy (x.begin() + (long) b * block, x.begin() + (long) (b + 1) * block, buf.getWritePointer (c));
            p.processBlock (buf, midi);
        }
        const double us = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t0).count() / nb;
        std::printf ("%-28s %10.2f %9.2f%%\n", label, us, 100.0 * us / blockBudgetUs);
    };

    runChain ("(analysis + global only)", [] (HarshVoxProcessor&) {});
    for (int t = 1; t < kNumModuleTypes; ++t)
        runChain (moduleInfo (t).name, [t] (HarshVoxProcessor& p) { p.setSlotType (0, (ModuleType) t); });
    runChain ("Vocoder (filterbank)", [] (HarshVoxProcessor& p)
    {
        p.setSlotType (0, ModuleType::CarrierVocoder);
        PresetBuilder b (p); b.slot (0, ModuleType::CarrierVocoder).set ("Env Source", 1);
    });
    runChain ("Mob (16 voices)", [] (HarshVoxProcessor& p) { PresetBuilder b (p); b.slot (0, ModuleType::Mob).set ("Voices", 16); });
    runChain ("Modal Rattle (60 modes)", [] (HarshVoxProcessor& p) { PresetBuilder b (p); b.slot (0, ModuleType::ModalRattle).set ("Modes", 60); });
    runChain ("8 heavy slots", [] (HarshVoxProcessor& p)
    {
        const ModuleType heavy[8] = { ModuleType::Mob, ModuleType::FormantFreeze, ModuleType::SourceDrive, ModuleType::CarrierVocoder,
                                      ModuleType::ModalRattle, ModuleType::PeriodDoubler, ModuleType::CuppedMic, ModuleType::ContactMic };
        for (int s = 0; s < 8; ++s) p.setSlotType (s, heavy[s]);
    });
    runChain ("all 13 modules (8 + sends)", [] (HarshVoxProcessor& p)
    {
        int t = 1;
        p.setSlotType (0, ModuleType::Splitter);
        for (int s = 1; s < 14 && t < kNumModuleTypes; ++s, ++t)
        {
            if ((ModuleType) t == ModuleType::Splitter) ++t;
            if (t < kNumModuleTypes) p.setSlotType (s, (ModuleType) t);
        }
    });
    return 0;
}

int cmdLevels (const juce::StringArray& a)
{
    // Each module alone at default settings: output RMS relative to input RMS (dB), per test file.
    const juce::File inDir (juce::File::getCurrentWorkingDirectory().getChildFile (arg (a, "--in", "test_vocals")));
    auto files = inDir.findChildFiles (juce::File::findFiles, false, "*.wav");
    files.sort();
    std::printf ("%-28s", "module");
    for (auto& f : files) std::printf (" %10s", f.getFileNameWithoutExtension().substring (0, 10).toRawUTF8());
    std::printf ("\n");
    for (int t = 1; t < kNumModuleTypes; ++t)
    {
        std::printf ("%-28s", moduleInfo (t).name);
        for (auto& f : files)
        {
            std::vector<Signal> in; double sr = 0;
            readWav (f, in, sr);
            HarshVoxProcessor p;
            p.resetToDefaults();
            p.setSlotType (0, (ModuleType) t);
            auto out = renderProc (p, in, sr, 128, 0);
            const float ri = rms (in[0]), ro = 0.5f * (rms (out[0]) + rms (out[1]));
            std::printf (" %+10.1f", gainToDb (ro) - gainToDb (ri));
        }
        std::printf ("\n");
    }
    return 0;
}

int cmdGenVocals (const juce::StringArray& a)
{
    const double sr = arg (a, "--sr", "48000").getDoubleValue();
    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (arg (a, "--out", "test_vocals")));
    const std::pair<const char*, VoiceStyle> styles[] = {
        { "synth_spoken", VoiceStyle::Spoken }, { "synth_sung", VoiceStyle::Sung },
        { "synth_screamed", VoiceStyle::Screamed }, { "synth_whispered", VoiceStyle::Whispered } };
    for (const auto& [name, st] : styles)
    {
        auto s = synthVoice (st, sr, 6.0, 1234);
        writeWav (outDir.getChildFile (juce::String (name) + ".wav"), { s }, sr);
        std::printf ("wrote %s\n", outDir.getChildFile (juce::String (name) + ".wav").getFullPathName().toRawUTF8());
    }
    return 0;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray a;
    for (int i = 1; i < argc; ++i) a.add (argv[i]);
    if (a.isEmpty()) { std::cerr << "usage: hv_cli render|bench|gen-vocals|list-presets [options]\n"; return 1; }
    if (a[0] == "render") return cmdRender (a);
    if (a[0] == "bench") return cmdBench (a);
    if (a[0] == "gen-vocals") return cmdGenVocals (a);
    if (a[0] == "levels") return cmdLevels (a);
    if (a[0] == "list-presets") { for (int i = 0; i < numFactoryPresets(); ++i) std::printf ("%s\n", factoryPresetName (i).toRawUTF8()); return 0; }
    std::cerr << "unknown command\n";
    return 1;
}
