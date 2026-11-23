#include "PluginEditor.h"
#include "plugin/Presets.h"

namespace hv
{
using namespace params;
using namespace ui;
using Rc = juce::Rectangle<float>;
using Pt = juce::Point<float>;

namespace
{
/** Preset drum behind the PROGRAM window: current preset with its neighbours rolling off the cylinder. */
class ProgramWindow final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ProgramWindow (HarshVoxProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void update()
    {
        if (proc.currentPresetName != shown) { shown = proc.currentPresetName; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        juce::ColourGradient drum (juce::Colour::fromFloatRGBA (0.30f, 0.28f, 0.22f, 1), 0, r.getY(), juce::Colour::fromFloatRGBA (0.30f, 0.28f, 0.22f, 1), 0, r.getBottom(), false);
        drum.addColour (0.25, juce::Colour::fromFloatRGBA (0.86f, 0.81f, 0.66f, 1));
        drum.addColour (0.5, juce::Colour::fromFloatRGBA (0.95f, 0.91f, 0.77f, 1));
        drum.addColour (0.75, juce::Colour::fromFloatRGBA (0.86f, 0.81f, 0.66f, 1));
        g.setGradientFill (drum);
        g.fillRoundedRectangle (r, 3.0f);
        const int n = numFactoryPresets();
        int cur = -1;
        for (int i = 0; i < n; ++i) if (factoryPresetName (i) == shown) cur = i;
        auto line = [&] (const juce::String& s, float y, float size, float alpha)
        {
            g.setFont (tagFont (size));
            g.setColour (palette::black.withAlpha (alpha));
            g.drawFittedText (s, Rc (r.getX() + 18.0f, y - size, r.getWidth() - 36.0f, size * 2.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        };
        auto label = [] (int i) { return juce::String (i + 1).paddedLeft ('0', 2) + "  " + factoryPresetName (i).toUpperCase(); };
        if (cur >= 0)
        {
            line (label ((cur - 1 + n) % n), r.getY() + 7.0f, 9.0f, 0.35f);
            line (label ((cur + 1) % n), r.getBottom() - 7.0f, 9.0f, 0.35f);
            line (label (cur), r.getCentreY(), 15.0f, 1.0f);
        }
        else line ("USER  " + shown.toUpperCase(), r.getCentreY(), 15.0f, 1.0f);
        g.setColour (palette::red.withAlpha (0.85f));
        g.drawLine (r.getX() + 4, r.getCentreY(), r.getX() + 14, r.getCentreY(), 1.2f);
        g.drawLine (r.getRight() - 14, r.getCentreY(), r.getRight() - 4, r.getCentreY(), 1.2f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f),
                                                 r.getX() + r.getWidth() * 0.4f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
    }

private:
    HarshVoxProcessor& proc;
    juce::String shown;
};
} // namespace

//==============================================================================
class HarshVoxEditor::Panel final : public juce::Component, private juce::Timer
{
public:
    Panel (HarshVoxEditor& e, HarshVoxProcessor& p)
        : ed (e), proc (p), program (p), vuIn (&p.engine().inPeak, nullptr, "INPUT"), vuOut (&p.engine().outPeakL, &p.engine().outPeakR, "OUTPUT"),
          inputMode (p, InputMode, 1.0f), chain (p), detail (p), modPanel (p), analysis (p)
    {
        using namespace layout;
        setOpaque (true);
        program.setBounds (programWin.toNearestInt());
        program.onClick = [this] { ed.showPresetMenu (&program); };
        program.setTooltip ("Click for presets: factory, user, load / save, export as code");
        prev.placeAt (prevBtn);
        next.placeAt (nextBtn);
        store.placeAt (storeBtn);
        prev.onClick = [this] { ed.stepPreset (-1); };
        next.onClick = [this] { ed.stepPreset (1); };
        store.onClick = [this] { ed.savePreset(); };
        store.setTooltip ("Save the current settings as a user preset (.hvx)");
        powerLamp.placeAt (layout::powerLamp);
        powerLamp.setLit (true);
        limitLamp.placeAt (layout::limitLamp);
        vuIn.setArea (layout::vuIn);
        vuOut.setArea (layout::vuOut);
        for (auto* c : { (juce::Component*) &program, (juce::Component*) &prev, (juce::Component*) &next, (juce::Component*) &store,
                         (juce::Component*) &powerLamp, (juce::Component*) &limitLamp, (juce::Component*) &vuIn, (juce::Component*) &vuOut })
            addAndMakeVisible (c);

        KnobLook gl;
        gl.r = masterKnobR; gl.numbers = true; gl.labelSize = 8.6f; gl.tagSize = 7.4f;
        const std::pair<int, const char*> g[] = { { InGain, "Input" }, { EnvAtk, "Env Atk" }, { EnvRel, "Env Rel" }, { Mix, "Mix" }, { OutGain, "Output" } };
        for (int i = 0; i < 5; ++i)
        {
            globals.push_back (std::make_unique<ParamKnob> (proc, g[i].first, -1, -1, gl));
            globals.back()->setLabel (g[i].second);
            globals.back()->placeAt ({ masterKnobX[i], knobRowY });
            addAndMakeVisible (*globals.back());
        }
        KnobLook sl;
        sl.r = seedR; sl.seedDial = true; sl.labelSize = 8.6f; sl.tagSize = 7.4f;
        seed = std::make_unique<ParamKnob> (proc, Seed, -1, -1, sl);
        seed->setLabel ("Seed");
        seed->placeAt (seedDial);
        addAndMakeVisible (*seed);
        inputMode.setTexts ("STEREO", "MONO", "INPUT MODE");
        inputMode.placeAt (inputModeSw);
        inputMode.setTooltip ("STEREO keeps left/right separate through the chain; MONO sums the input first. Not changed by presets.");
        addAndMakeVisible (inputMode);
        modView.setTexts ("MATRIX", "CHAIN", "MOD VIEW");
        modView.placeAt (modViewSw);
        modView.onChange = [this] (bool on) { ed.setModView (on); };
        modView.setTooltip ("Show the input-reactive modulation matrix in the lower section");
        addAndMakeVisible (modView);

        chain.setBounds (Rc (bayX0, bayY, 8.0f * bayW + 7.0f * bayGap, bayH).toNearestInt());
        detail.setBounds (detailPlate.toNearestInt());
        modPanel.setBounds (detailPlate.toNearestInt());
        analysis.setBounds (Rc (scopeL.getX(), scopeL.getY(), scopeR.getRight() - scopeL.getX(), scopeL.getHeight()).toNearestInt());
        classifier.setBounds (Rc (layout::classifier.x - 50.0f, layout::classifier.y - 8.0f, 100.0f, 104.0f).toNearestInt());
        addAndMakeVisible (chain);
        addAndMakeVisible (detail);
        addChildComponent (modPanel);
        addAndMakeVisible (analysis);
        addAndMakeVisible (classifier);
    }

    void paint (juce::Graphics& g) override
    {
        const float k = std::max (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto occ = chain.occupancy();
        if (! art.isValid() || occ != artOcc) renderArt (k, occ);
        else if (std::abs (k - artScale) > 0.01f * artScale) { wantScale = k; startTimer (160); }
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageTransformed (art, juce::AffineTransform::scale (1.0f / artScale));
        // dynamic lettering on the baked panel
        mil::sectionTag (g, layout::detailPlate.getX(), layout::detailPlate.getY(),
                         modPanel.isVisible() ? juce::String ("MODULATION MATRIX  -  INPUT-REACTIVE ONLY") : detail.title());
        const auto lb = layout::latencyBox;
        const juce::String digits = latencyText.paddedLeft ('0', 5);
        float x = lb.getX() + 5.0f;
        for (auto ch : digits)
        {
            if (ch == '.')
            {
                g.setColour (palette::ink);
                g.fillEllipse (x - 0.5f, lb.getBottom() - 6.0f, 2.6f, 2.6f);
                x += 4.0f;
                continue;
            }
            const Rc cell (x, lb.getY() + 3.0f, 13.0f, lb.getHeight() - 6.0f);
            g.setColour (juce::Colour::fromFloatRGBA (0.93f, 0.92f, 0.86f, 1));
            g.fillRoundedRectangle (cell, 1.0f);
            g.setFont (tagFont (11.0f));
            g.setColour (palette::black);
            g.drawText (juce::String::charToString (ch), cell, juce::Justification::centred, false);
            x += 16.0f;
        }
    }

    void tick()
    {
        program.update();
        vuIn.tick();
        vuOut.tick();
        limitLamp.setLit (vuOut.hot());
        for (auto& k : globals) k->sync();
        seed->sync();
        inputMode.sync();
        chain.tick();
        if (detail.isVisible()) detail.tick();
        if (modPanel.isVisible()) modPanel.tick();
        analysis.tick (ed.selected);
        classifier.update (analysis.last());
        const double ms = proc.getLatencySamples() * 1000.0 / std::max (1.0, proc.getSampleRate());
        const auto lt = juce::String (std::min (999.9, ms), 1);
        if (lt != latencyText) { latencyText = lt; repaint (layout::latencyBox.expanded (2.0f).toNearestInt()); }
        const auto t = modPanel.isVisible() ? juce::String() : detail.title();
        if (t != shownTitle) { shownTitle = t; repaint (Rc (layout::detailPlate.getX(), layout::detailPlate.getY() - 10.0f, 520.0f, 20.0f).toNearestInt()); }
        if (chain.occupancy() != artOcc) repaint();
    }

    HarshVoxEditor& ed;
    HarshVoxProcessor& proc;
    ProgramWindow program;
    PushButton prev { juce::Colour (0xff1e1e1c), 9.0f, "< PREV" }, next { juce::Colour (0xff1e1e1c), 9.0f, "NEXT >" }, store { palette::red, 10.0f, "STORE" };
    Lamp powerLamp { juce::Colour (0xffff2e14), 5.5f }, limitLamp { palette::amber, 5.5f };
    VuMeter vuIn, vuOut;
    std::vector<std::unique_ptr<ParamKnob>> globals;
    std::unique_ptr<ParamKnob> seed;
    ToggleSwitch inputMode, modView { 1.0f };
    ChainView chain;
    SlotDetail detail;
    ModMatrixPanel modPanel;
    AnalysisDisplay analysis;
    ClassifierPanel classifier;

private:
    void renderArt (float k, uint8_t occ)
    {
        art = PanelArt::render (k, occ);
        artScale = k;
        artOcc = occ;
    }
    void timerCallback() override
    {
        stopTimer();
        renderArt (wantScale, chain.occupancy());
        repaint();
    }
    juce::Image art;
    float artScale = 1.0f, wantScale = 1.0f;
    uint8_t artOcc = 0;
    juce::String latencyText = "0.0", shownTitle;
};

//==============================================================================
HarshVoxEditor::HarshVoxEditor (HarshVoxProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);
    juce::LookAndFeel::setDefaultLookAndFeel (&laf);
    panel = std::make_unique<Panel> (*this, p);
    panel->chain.onSelect = [this] (int s) { selectSlot (s); };
    panel->detail.onSelectSlot = [this] (int s) { selectSlot (s); };
    addAndMakeVisible (*panel);

    for (int s = 0; s < kMainSlots; ++s)
        if (proc.slotType (s) > 0) { selected = s; break; }
    selectSlot (selected);

    setResizable (true, true);
    setResizeLimits ((int) std::lround (layout::kW * layout::kMinScale), (int) std::lround (layout::kH * layout::kMinScale),
                     (int) std::lround (layout::kW * layout::kMaxScale), (int) std::lround (layout::kH * layout::kMaxScale));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (layout::kW / layout::kH);
    setSize ((int) layout::kW, (int) layout::kH);
    startTimerHz (30);
}

HarshVoxEditor::~HarshVoxEditor()
{
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void HarshVoxEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111110));
}

void HarshVoxEditor::resized()
{
    const float s = std::min ((float) getWidth() / layout::kW, (float) getHeight() / layout::kH);
    panel->setBounds (0, 0, (int) layout::kW, (int) layout::kH);
    panel->setTransform (juce::AffineTransform::scale (s).translated (((float) getWidth() - layout::kW * s) * 0.5f,
                                                                      ((float) getHeight() - layout::kH * s) * 0.5f));
}

void HarshVoxEditor::setModView (bool on)
{
    panel->modView.setUp (on);
    panel->detail.setVisible (! on);
    panel->classifier.setVisible (! on);
    panel->modPanel.setVisible (on);
    panel->repaint();
}

void HarshVoxEditor::selectSlot (int s)
{
    selected = s;
    panel->chain.setSelected (s);
    panel->detail.setSlot (s);
    if (panel->modPanel.isVisible()) setModView (false);
    panel->repaint (Rc (layout::detailPlate.getX(), layout::detailPlate.getY() - 10.0f, 520.0f, 20.0f).toNearestInt());
}

void HarshVoxEditor::timerCallback()
{
    panel->tick();
}

void HarshVoxEditor::stepPreset (int delta)
{
    const int n = numFactoryPresets();
    int cur = 0;
    for (int i = 0; i < n; ++i) if (factoryPresetName (i) == proc.currentPresetName) cur = i;
    proc.setCurrentProgram ((cur + delta + n) % n);
}

void HarshVoxEditor::savePreset()
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
}

void HarshVoxEditor::showPresetMenu (juce::Component* target)
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
    m.addItem (903, "Save preset...");
    m.addItem (901, "Reveal user preset folder");
    m.addSeparator();
    m.addItem (902, "Export preset as code (copy to clipboard)");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target).withMinimumWidth (260), [this, files] (int r)
    {
        if (r <= 0) return;
        if (r < 900) proc.setCurrentProgram (r - 1);
        else if (r >= 1000) proc.loadPresetFile (files[r - 1000]);
        else if (r == 901) { HarshVoxProcessor::userPresetFolder().createDirectory(); HarshVoxProcessor::userPresetFolder().revealToUser(); }
        else if (r == 902) exportPreset();
        else if (r == 903) savePreset();
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
} // namespace hv
