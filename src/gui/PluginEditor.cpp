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
juce::String presetLabel (int i) { return juce::String (i + 1).paddedLeft ('0', 2) + "  " + factoryPresetName (i).toUpperCase(); }
int presetIndex (const juce::String& name)
{
    for (int i = 0; i < numFactoryPresets(); ++i) if (factoryPresetName (i) == name) return i;
    return -1;
}

/** Backlit LCD behind the PROGRAM bezel: current preset large, the next one small underneath. */
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
        g.setGradientFill (juce::ColourGradient (juce::Colour::fromFloatRGBA (0.70f, 0.74f, 0.64f, 1), r.getX(), r.getY(),
                                                 juce::Colour::fromFloatRGBA (0.52f, 0.56f, 0.47f, 1), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        // inner shadow from the bezel
        for (int i = 0; i < 5; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.10f));
            g.drawRoundedRectangle (r.reduced ((float) i), 3.0f, 1.0f);
        }
        const int n = numFactoryPresets(), cur = presetIndex (shown);
        const auto ink = juce::Colour::fromFloatRGBA (0.07f, 0.09f, 0.07f, 1.0f);
        g.setFont (tagFont (17.0f));
        g.setColour (ink);
        g.drawFittedText (cur >= 0 ? presetLabel (cur) : "USER  " + shown.toUpperCase(), Rc (r.getX() + 30.0f, r.getY() + 5.0f, r.getWidth() - 60.0f, 24.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.6f);
        if (cur >= 0)
        {
            g.setFont (tagFont (9.0f));
            g.setColour (ink.withAlpha (0.55f));
            g.drawFittedText (presetLabel ((cur + 1) % n), Rc (r.getX() + 30.0f, r.getBottom() - 19.0f, r.getWidth() - 60.0f, 13.0f).toNearestInt(),
                              juce::Justification::centred, 1, 0.7f);
        }
        g.setColour (palette::red);
        g.fillRect (r.getX() + 8.0f, r.getY() + 16.0f, 13.0f, 2.2f);
        g.fillRect (r.getRight() - 21.0f, r.getY() + 16.0f, 13.0f, 2.2f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f),
                                                 r.getX() + r.getWidth() * 0.4f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
    }

private:
    HarshVoxProcessor& proc;
    juce::String shown;
};

/** Preset field in the title bar: up / down steppers, the name, and a menu diamond. */
class HeaderProgram final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit HeaderProgram (HarshVoxProcessor& p) : proc (p)
    {
        addAndMakeVisible (up);
        addAndMakeVisible (down);
        up.setTooltip ("Previous preset");
        down.setTooltip ("Next preset");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    IconButton up { IconButton::Kind::up }, down { IconButton::Kind::down };
    std::function<void()> onClick;
    void resized() override
    {
        up.setBounds (2, 1, 22, getHeight() / 2 - 1);
        down.setBounds (2, getHeight() / 2, 22, getHeight() / 2 - 1);
    }
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void update() { if (proc.currentPresetName != shown) { shown = proc.currentPresetName; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0c0c0d));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (26.0f, 4.0f, 26.0f, r.getBottom() - 4.0f, 1.0f);
        g.setFont (stencilFont (7.4f).withExtraKerningFactor (0.1f));
        g.setColour (palette::textDim);
        g.drawText ("PROGRAM", r.withHeight (11.0f).translated (0.0f, 1.0f), juce::Justification::centred, false);
        const int cur = presetIndex (shown);
        g.setFont (tagFont (11.0f));
        g.setColour (palette::ink);
        g.drawFittedText (cur >= 0 ? presetLabel (cur) : "USER  " + shown.toUpperCase(), Rc (30.0f, 11.0f, r.getWidth() - 56.0f, 16.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.7f);
        juce::Path d;
        d.addQuadrilateral (r.getRight() - 14.0f, 10.0f, r.getRight() - 10.5f, 14.0f, r.getRight() - 14.0f, 18.0f, r.getRight() - 17.5f, 14.0f);
        g.fillPath (d);
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
        : ed (e), proc (p), program (p), headerProgram (p), vuIn (&p.engine().inPeak, nullptr, "INPUT"), vuOut (&p.engine().outPeakL, &p.engine().outPeakR, "OUTPUT"),
          inputMode (p, InputMode, 1.0f), chain (p), detail (p), modPanel (p), analysis (p)
    {
        using namespace layout;
        setOpaque (true);
        // title bar
        headerProgram.setBounds (layout::headerProgram.toNearestInt());
        headerProgram.onClick = [this] { ed.showPresetMenu (&headerProgram); };
        headerProgram.up.onClick = [this] { ed.stepPreset (-1); };
        headerProgram.down.onClick = [this] { ed.stepPreset (1); };
        headerProgram.setTooltip ("Click for presets: factory, user, load / save, export as code");
        addAndMakeVisible (headerProgram);
        IconButton* icons[] = { &iconSave, &iconInit, &iconDice, &iconBell, &iconMenu, &iconPower };
        for (int i = 0; i < 6; ++i)
        {
            icons[i]->setBounds (Rc (26.0f, 26.0f).withCentre ({ headerIconX[i], headerIconY }).toNearestInt());
            addAndMakeVisible (*icons[i]);
        }
        iconSave.setTooltip ("Save the current settings as a user preset (.hvx)");
        iconSave.onClick = [this] { ed.savePreset(); };
        iconInit.setTooltip ("Reset to Init: empties every bay and returns all controls to their defaults");
        iconInit.onClick = [this] { ed.confirmInit(); };
        iconDice.setTooltip ("Roll a new random Seed");
        iconDice.onClick = [this]
        {
            auto* prm = proc.param (Seed);
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (juce::Random::getSystemRandom().nextFloat());
            prm->endChangeGesture();
        };
        iconBell.setTooltip ("Output limiter: lights while the safety limiter is catching peaks");
        iconBell.setInterceptsMouseClicks (false, false);
        iconMenu.setTooltip ("Preset menu");
        iconMenu.onClick = [this] { ed.showPresetMenu (&iconMenu); };
        iconPower.setTooltip ("About Harsh Vox");
        iconPower.onClick = [this] { ed.showAbout(); };

        program.setBounds (programWin.reduced (11.0f, 10.0f).toNearestInt());
        program.onClick = [this] { ed.showPresetMenu (&program); };
        program.setTooltip ("Click for presets: factory, user, load / save, export as code");
        prev.arrow = -1; next.arrow = 1;
        for (auto* b : { &prev, &next, &store }) { b->labelGap = 22.0f; b->labelSize = 9.0f; }
        prev.placeAt (prevBtn);
        next.placeAt (nextBtn);
        store.placeAt (storeBtn);
        prev.onClick = [this] { ed.stepPreset (-1); };
        next.onClick = [this] { ed.stepPreset (1); };
        store.onClick = [this] { ed.savePreset(); };
        store.setTooltip ("Save the current settings as a user preset (.hvx)");
        powerLamp.labelGap = 26.0f;
        powerLamp.placeAt (layout::powerLamp);
        powerLamp.setLit (true);
        vuIn.setArea (layout::vuIn);
        vuOut.setArea (layout::vuOut);
        for (auto* c : { (juce::Component*) &program, (juce::Component*) &prev, (juce::Component*) &next, (juce::Component*) &store,
                         (juce::Component*) &powerLamp, (juce::Component*) &vuIn, (juce::Component*) &vuOut })
            addAndMakeVisible (c);

        KnobLook gl;
        gl.r = masterKnobR; gl.numbers = true; gl.labelSize = 9.6f; gl.tagSize = 8.0f;
        const std::pair<int, const char*> g[] = { { InGain, "Input" }, { EnvAtk, "Env Atk" }, { EnvRel, "Env Rel" }, { Mix, "Mix" }, { OutGain, "Output" } };
        for (int i = 0; i < 5; ++i)
        {
            globals.push_back (std::make_unique<ParamKnob> (proc, g[i].first, -1, -1, gl));
            globals.back()->setLabel (g[i].second);
            globals.back()->placeAt ({ masterKnobX[i], knobRowY });
            addAndMakeVisible (*globals.back());
        }
        KnobLook sl;
        sl.r = seedR; sl.seedDial = true; sl.labelSize = 9.6f; sl.tagSize = 8.0f;
        seed = std::make_unique<ParamKnob> (proc, Seed, -1, -1, sl);
        seed->setLabel ("Seed");
        seed->placeAt (seedDial);
        addAndMakeVisible (*seed);
        for (auto* t : { &inputMode, &modView }) { t->nameOnTop = true; t->nameSize = 9.6f; }
        inputMode.setTexts ("STEREO", "MONO", "INPUT");
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
        classifier.setBounds (Rc (layout::classifier.x - 60.0f, layout::classifier.y - 8.0f, 120.0f, 118.0f).toNearestInt());
        addAndMakeVisible (chain);
        addAndMakeVisible (detail);
        addChildComponent (modPanel);
        addAndMakeVisible (analysis);
        addAndMakeVisible (classifier);
    }

    static Rc titleArea() { return { layout::detailTag.x - 4.0f, layout::detailTag.y - 12.0f, 420.0f, 24.0f }; }

    void paint (juce::Graphics& g) override
    {
        const float k = std::max (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto occ = chain.occupancy();
        if (! art.isValid() || occ != artOcc) renderArt (k, occ);
        else if (std::abs (k - artScale) > 0.01f * artScale) { wantScale = k; startTimer (160); }
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageTransformed (art, juce::AffineTransform::scale (1.0f / artScale));
        // dynamic lettering on the baked panel
        if (! modPanel.isVisible()) mil::sectionTag (g, layout::detailTag.x, layout::detailTag.y, detail.title());
        // latency counter: four drum digits, tenths after the point
        const auto lb = layout::latencyBox;
        const juce::String digits = latencyText.removeCharacters (".").paddedLeft ('0', 4).getLastCharacters (4);
        for (int i = 0; i < 4; ++i)
        {
            const Rc cell (lb.getX() + 5.0f + (float) i * 22.5f + (i == 3 ? 3.0f : 0.0f), lb.getY() + 4.0f, 18.0f, lb.getHeight() - 8.0f);
            juce::ColourGradient drum (juce::Colour::fromFloatRGBA (0.55f, 0.54f, 0.50f, 1), 0, cell.getY(), juce::Colour::fromFloatRGBA (0.55f, 0.54f, 0.50f, 1), 0, cell.getBottom(), false);
            drum.addColour (0.3, juce::Colour::fromFloatRGBA (0.95f, 0.94f, 0.89f, 1));
            drum.addColour (0.7, juce::Colour::fromFloatRGBA (0.95f, 0.94f, 0.89f, 1));
            g.setGradientFill (drum);
            g.fillRoundedRectangle (cell, 1.5f);
            g.setFont (tagFont (14.0f));
            g.setColour (palette::black);
            g.drawText (juce::String::charToString (digits[i]), cell.translated (0.0f, 0.5f), juce::Justification::centred, false);
        }
        g.setColour (palette::ink);
        g.fillEllipse (lb.getX() + 5.0f + 3.0f * 22.5f - 1.5f, lb.getBottom() - 8.0f, 2.6f, 2.6f);
    }

    void tick()
    {
        program.update();
        headerProgram.update();
        vuIn.tick();
        vuOut.tick();
        iconBell.setLit (vuOut.hot());
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
        if (t != shownTitle) { shownTitle = t; repaint (titleArea().toNearestInt()); }
        if (chain.occupancy() != artOcc) repaint();
    }

    HarshVoxEditor& ed;
    HarshVoxProcessor& proc;
    ProgramWindow program;
    HeaderProgram headerProgram;
    IconButton iconSave { IconButton::Kind::save }, iconInit { IconButton::Kind::init }, iconDice { IconButton::Kind::dice },
               iconBell { IconButton::Kind::bell }, iconMenu { IconButton::Kind::menu }, iconPower { IconButton::Kind::power };
    PushButton prev { juce::Colour (0xffa9aeb6), layout::roundBtnR, "PREV" }, next { juce::Colour (0xffa9aeb6), layout::roundBtnR, "NEXT" },
               store { palette::red, layout::roundBtnR - 1.0f, "STORE" };
    Lamp powerLamp { palette::amber, 10.0f, "POWER" };
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
    panel->repaint (Panel::titleArea().toNearestInt());
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

void HarshVoxEditor::confirmInit()
{
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Reset to Init",
                                        "Empty every bay and return all controls to their defaults?", "Reset", "Cancel", this,
                                        juce::ModalCallbackFunction::create ([this] (int r) { if (r == 1) proc.setCurrentProgram (0); }));
}

void HarshVoxEditor::showAbout()
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Harsh Vox  v" HV_VERSION_STRING,
        "Vocal Destruction Set No. 13\nStatic Ruin\n\nFonts: IBM Plex (OFL). Panel textures: ambientCG (CC0).", "OK", this);
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
