// Renders the real editor to PNG (run under xvfb-run) for visual review / docs.
//   hv_ui_snapshot <out.png> [preset index] [width height] [wav to feed] [selected slot] [mod]
#include "plugin/PluginProcessor.h"
#include "plugin/Presets.h"
#include "gui/PluginEditor.h"
#include "TestHelpers.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "ui.png"));
    const int preset = argc > 2 ? juce::String (argv[2]).getIntValue() : 1;
    const int w = argc > 4 ? juce::String (argv[3]).getIntValue() : 1220, h = argc > 4 ? juce::String (argv[4]).getIntValue() : 760;
    hv::HarshVoxProcessor proc;
    hv::applyFactoryPreset (proc, preset);
    const double sr = 48000;
    proc.setPlayConfigDetails (2, 2, sr, 128);
    proc.prepareToPlay (sr, 128);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (w, h);
    if (argc > 7 && juce::String (argv[7]) == "mod")
        if (auto* b = dynamic_cast<juce::Button*> (ed->getChildComponent (4))) b->triggerClick();
    std::vector<hvtest::Signal> in;
    double fsr = sr;
    if (argc > 5) hvtest::readWav (juce::File::getCurrentWorkingDirectory().getChildFile (argv[5]), in, fsr);
    if (in.empty()) in.push_back (hvtest::synthVoice (hvtest::VoiceStyle::Screamed, sr, 3.0, 4));
    juce::AudioBuffer<float> buf (2, 128);
    juce::MidiBuffer midi;
    const size_t n = std::min<size_t> (in[0].size(), (size_t) (2.6 * sr));
    for (size_t pos = 0; pos + 128 <= n; pos += 128)
    {
        for (int c = 0; c < 2; ++c) std::copy (in[0].begin() + (long) pos, in[0].begin() + (long) pos + 128, buf.getWritePointer (c));
        proc.processBlock (buf, midi);
        if ((pos / 128) % 12 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::PNGImageFormat png;
    out.deleteFile();
    juce::FileOutputStream os (out);
    png.writeImageToStream (img, os);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    return 0;
}
