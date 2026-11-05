#include "PluginEditor.h"
#include "plugin/Presets.h"

namespace hv
{
using namespace params;
using namespace ui;

HarshVoxEditor::HarshVoxEditor (HarshVoxProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      inMeter (&p.engine().inPeak), outMeter (&p.engine().outPeakL, &p.engine().outPeakR),
      chain (p), detail (p), modPanel (p), analysis (p)
{
    setLookAndFeel (&laf);
    juce::LookAndFeel::setDefaultLookAndFeel (&laf);

    presetButton.onClick = [this] { showPresetMenu(); };
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this]
    {
        auto dir = HarshVoxProcessor::userPresetFolder();
        dir.createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Save Harsh Vox preset", dir.getChildFile (proc.currentPresetName + ".hvx"), "*.hvx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f != juce::File()) proc.savePresetFile (f.withFileExtension ("hvx"));
                              });
    };
    modButton.setClickingTogglesState (true);
    modButton.setTooltip ("Show the input-reactive modulation matrix");
    modButton.onClick = [this] { detail.setVisible (! modButton.getToggleState()); modPanel.setVisible (modButton.getToggleState()); };
    for (auto* b : { &presetButton, &prevButton, &nextButton, &saveButton, &modButton }) addAndMakeVisible (b);

    const std::pair<int, const char*> g[] = { { InGain, "Input" }, { EnvAtk, "Env Atk" }, { EnvRel, "Env Rel" },
                                              { Seed, "Seed" }, { Mix, "Mix" }, { OutGain, "Output" } };
    for (auto& [idx, name] : g)
    {
        globals.push_back (std::make_unique<ParamKnob> (proc, idx, -1, -1, true));
        globals.back()->setLabel (name);
        addAndMakeVisible (*globals.back());
    }
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);
    chain.onSelect = [this] (int s) { selectSlot (s); };
    detail.onSelectSlot = [this] (int s) { selectSlot (s); };
    addAndMakeVisible (chain);
    addAndMakeVisible (detail);
    addChildComponent (modPanel);
    addAndMakeVisible (analysis);

    for (int s = 0; s < kMainSlots; ++s)
        if (proc.slotType (s) > 0) { selected = s; break; }
    selectSlot (selected);

    setResizable (true, true);
    setResizeLimits (900, 560, 2400, 1600);
    setSize (1220, 760);
    startTimerHz (30);
}

HarshVoxEditor::~HarshVoxEditor()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void HarshVoxEditor::selectSlot (int s)
{
    selected = s;
    chain.setSelected (s);
    detail.setSlot (s);
    if (modButton.getToggleState()) { modButton.setToggleState (false, juce::dontSendNotification); modPanel.setVisible (false); detail.setVisible (true); }
}

void HarshVoxEditor::timerCallback()
{
    for (auto& k : globals) k->sync();
    inMeter.tick();
    outMeter.tick();
    chain.tick();
    if (detail.isVisible()) detail.tick();
    if (modPanel.isVisible()) modPanel.tick();
    analysis.tick (selected);
    presetButton.setButtonText (proc.currentPresetName);
    const auto lt = juce::String (proc.getLatencySamples() * 1000.0 / std::max (1.0, proc.getSampleRate()), 1) + " ms";
    if (lt != latencyText) { latencyText = lt; repaint (headerArea()); }
}

void HarshVoxEditor::stepPreset (int delta)
{
    const int n = numFactoryPresets();
    int cur = 0;
    for (int i = 0; i < n; ++i) if (factoryPresetName (i) == proc.currentPresetName) cur = i;
    proc.setCurrentProgram ((cur + delta + n) % n);
}

void HarshVoxEditor::showPresetMenu()
{
    juce::PopupMenu m, user;
    m.addSectionHeader ("FACTORY");
    for (int i = 0; i < numFactoryPresets(); ++i) m.addItem (1 + i, factoryPresetName (i), true, factoryPresetName (i) == proc.currentPresetName);
    auto files = HarshVoxProcessor::userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.hvx");
    files.sort();
    for (int i = 0; i < files.size(); ++i) user.addItem (1000 + i, files[i].getFileNameWithoutExtension());
    m.addSeparator();
    m.addSubMenu ("User", user, ! files.isEmpty());
    m.addItem (900, "Load preset file...");
    m.addItem (901, "Reveal user preset folder");
    m.addSeparator();
    m.addItem (902, "Export preset as code (copy to clipboard)");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (260), [this, files] (int r)
    {
        if (r <= 0) return;
        if (r < 900) proc.setCurrentProgram (r - 1);
        else if (r >= 1000) proc.loadPresetFile (files[r - 1000]);
        else if (r == 901) { HarshVoxProcessor::userPresetFolder().createDirectory(); HarshVoxProcessor::userPresetFolder().revealToUser(); }
        else if (r == 902) exportPreset();
        else if (r == 900)
        {
            chooser = std::make_unique<juce::FileChooser> ("Load Harsh Vox preset", HarshVoxProcessor::userPresetFolder(), "*.hvx");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.loadPresetFile (fc.getResult()); });
        }
    });
}

void HarshVoxEditor::exportPreset()
{
    auto* w = new juce::AlertWindow ("Export preset as code", "Name for this preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.currentPresetName == "Init" ? juce::String ("My Preset") : proc.currentPresetName);
    w->addButton ("Export", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r)
    {
        if (r != 1) return;
        const auto name = w->getTextEditorContents ("name").trim();
        const auto code = exportPresetCode (proc, name.isEmpty() ? juce::String ("Untitled") : name);
        juce::SystemClipboard::copyTextToClipboard (code);
        auto dir = HarshVoxProcessor::userPresetFolder().getChildFile ("Exports");
        dir.createDirectory();
        auto f = dir.getNonexistentChildFile (juce::File::createLegalFileName (name.isEmpty() ? "Untitled" : name), ".txt");
        f.replaceWithText (code);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Preset exported",
            "Copied to the clipboard and saved to:\n" + f.getFullPathName() + "\n\nPaste it into the preset list (src/plugin/Presets.cpp).", "OK", this);
    }), true);
}

void HarshVoxEditor::paint (juce::Graphics& g)
{
    using namespace palette;
    g.fillAll (bg0);
    auto h = headerArea().toFloat();
    g.setColour (bg1);
    g.fillRect (h);
    g.setColour (line);
    g.drawHorizontalLine ((int) h.getBottom() - 1, 0.0f, (float) getWidth());
    drawHarshVoxWordmark (g, 16.0f, h.getCentreY() - 10.0f, 20.0f);
    const float lw = staticRuinLogoWidth (11.0f);
    drawStaticRuinLogo (g, h.getRight() - lw - 16.0f, h.getCentreY() - 5.5f, 11.0f, true);

    // global strip background + labels
    auto gs = getLocalBounds().withTrimmedTop (48).removeFromTop (78).reduced (8, 6).toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (gs, 5.0f);
    BlockFont::draw (g, "IN", gs.getX() + 10.0f, gs.getY() + 8.0f, 7.0f, textDim);
    BlockFont::draw (g, "OUT", gs.getRight() - 30.0f, gs.getY() + 8.0f, 7.0f, textDim);
    g.setFont (monoFont (11.0f));
    g.setColour (textDim);
    g.drawText ("LATENCY " + latencyText, juce::Rectangle<float> (gs.getRight() - 300.0f, gs.getY(), 220.0f, gs.getHeight()).toNearestInt(),
                juce::Justification::centredRight);
}

void HarshVoxEditor::resized()
{
    auto r = getLocalBounds();
    auto h = r.removeFromTop (48).reduced (0, 10);
    const int cx = getWidth() / 2;
    presetButton.setBounds (cx - 140, h.getY(), 280, h.getHeight());
    prevButton.setBounds (cx - 140 - 32, h.getY(), 28, h.getHeight());
    nextButton.setBounds (cx + 144, h.getY(), 28, h.getHeight());
    saveButton.setBounds (cx + 180, h.getY(), 56, h.getHeight());

    auto gs = r.removeFromTop (78).reduced (8, 6);
    inMeter.setBounds (gs.getX() + 32, gs.getY() + 8, 14, gs.getHeight() - 16);
    outMeter.setBounds (gs.getRight() - 46, gs.getY() + 8, 14, gs.getHeight() - 16);
    auto knobs = gs.withTrimmedLeft (60).withTrimmedRight (60);
    for (auto& k : globals) k->setBounds (knobs.removeFromLeft (74));
    modButton.setBounds (knobs.removeFromLeft (70).withSizeKeepingCentre (56, 26).translated (10, 0));

    r.reduce (8, 0);
    r.removeFromBottom (8);
    const int avail = r.getHeight();
    const int chainH = std::max (180, (int) (avail * 0.38f));
    const int analysisH = std::max (110, (int) (avail * 0.25f));
    chain.setBounds (r.removeFromTop (chainH));
    r.removeFromTop (8);
    analysis.setBounds (r.removeFromBottom (analysisH));
    r.removeFromBottom (8);
    detail.setBounds (r);
    modPanel.setBounds (r);
}
} // namespace hv
