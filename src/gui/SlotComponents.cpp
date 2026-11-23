#include "SlotComponents.h"

namespace hv::ui
{
using namespace params;
using Rc = juce::Rectangle<float>;
using Pt = juce::Point<float>;

void showModuleMenu (HarshVoxProcessor& p, int slot, juce::Component* target, std::function<void()> after)
{
    juce::PopupMenu m;
    m.addSectionHeader (slot < kMainSlots ? "BAY " + juce::String (slot + 1).paddedLeft ('0', 2) : "SEND " + juce::String (slotLabel (slot)));
    const int cur = p.slotType (slot);
    for (int t = 1; t < kNumModuleTypes; ++t)
    {
        if (slot >= kMainSlots && t == (int) ModuleType::Splitter) continue;   // no nested splitters
        m.addItem (t, moduleInfo (t).name, true, t == cur);
    }
    m.addSeparator();
    m.addItem (1000, "Clear slot", cur != 0);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target).withMinimumWidth (220),
                     [&p, slot, after] (int r)
                     {
                         if (r <= 0) return;
                         p.setSlotType (slot, r == 1000 ? ModuleType::Empty : (ModuleType) r);
                         if (after) after();
                     });
}

juce::String moduleAbbrev (int t)
{
    static const char* const a[] = { "---", "DRV", "GRL", "FRY", "FRM", "SPL", "SYL", "CUP", "CNT", "RTL", "KZO", "CAR", "SAL", "MOB" };
    return a[std::clamp (t, 0, (int) std::size (a) - 1)];
}

//==============================================================================
BayUnit::BayUnit (HarshVoxProcessor& p, int s) : slot (s), proc (p), power (p, slotIndex (s, SOn), 0.55f)
{
    power.setTexts ({}, {}, {});
    power.placeAt ({ face().getX() + 13.0f, face().getBottom() - 17.0f });
    addChildComponent (power);
    rebuild();
}

Rc BayUnit::face() const { return layout::moduleFace ({ 0, 0, layout::bayW, layout::bayH }); }
Rc BayUnit::namePlate() const { const auto f = face(); return { f.getX() + 18.0f, f.getY() + 3.0f, f.getWidth() - 36.0f, 17.0f }; }

void BayUnit::rebuild()
{
    type = proc.slotType (slot);
    for (auto& k : keys) removeChildComponent (k.get());
    keys.clear();
    if (mixKnob) removeChildComponent (mixKnob.get());
    mixKnob.reset();
    if (type > 0)
    {
        KnobLook kl;
        kl.r = 11.0f; kl.labelSize = 7.2f; kl.tagSize = 6.9f;
        const auto& specs = moduleInfo (type).params;
        for (int k = 0; k < (int) specs.size() && keys.size() < 4; ++k)
            if (specs[(size_t) k].key)
            {
                const int i = (int) keys.size();
                keys.push_back (std::make_unique<ParamKnob> (proc, slotIndex (slot, SP0 + k), slot, k, kl));
                keys.back()->placeAt ({ 37.0f + (float) (i % 2) * 62.0f, 48.0f + (float) (i / 2) * 57.0f });
                addAndMakeVisible (*keys.back());
            }
        KnobLook ml;
        ml.r = 9.0f; ml.label = false; ml.valueTag = false;
        mixKnob = std::make_unique<ParamKnob> (proc, slotIndex (slot, SMix), slot, kParamsPerSlot, ml);
        mixKnob->setLabel ("Mix");
        mixKnob->placeAt ({ face().getRight() - 21.0f, face().getBottom() - 18.0f });
        addAndMakeVisible (*mixKnob);
    }
    power.setVisible (type > 0);
    repaint();
}

void BayUnit::tick()
{
    if (proc.slotType (slot) != type) rebuild();
    for (auto& k : keys) k->sync();
    if (mixKnob) mixKnob->sync();
    power.sync();
    const bool o = proc.paramReal (slotIndex (slot, SOn)) >= 0.5f;
    if (o != on) { on = o; repaint(); }
}

void BayUnit::paint (juce::Graphics& g)
{
    using namespace mil;
    const auto f = face();
    if (type <= 0)
    {
        const float cx = (float) getWidth() * 0.5f, cy = (float) getHeight() * 0.5f;
        stencil (g, "BAY " + juce::String (slot + 1).paddedLeft ('0', 2), cx, cy - 12.0f, 10.0f, juce::Justification::centred, palette::ink, 1.0f, 0.12f);
        stencil (g, "VACANT", cx, cy + 4.0f, 7.8f, juce::Justification::centred, palette::ink, 0.8f, 0.18f);
        g.setColour (palette::ink.withAlpha (0.5f));
        g.drawLine (cx - 26.0f, cy + 16.0f, cx + 26.0f, cy + 16.0f, 0.8f);
        stencil (g, "INSERT MODULE", cx, cy + 26.0f, 7.0f, juce::Justification::centred, palette::ink, 0.75f, 0.08f);
        return;
    }
    thumbscrew (g, { f.getX() + 9.0f, f.getY() + 11.0f });
    thumbscrew (g, { f.getRight() - 9.0f, f.getY() + 11.0f });
    const auto np = namePlate();
    softShadow (g, np, 2.0f, 3, 0.12f, 1.2f, 1.8f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff292926), 0, np.getY(), juce::Colour (0xff0d0d0d), 0, np.getBottom(), false));
    g.fillRoundedRectangle (np, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawLine (np.getX() + 2, np.getY() + 0.5f, np.getRight() - 2, np.getY() + 0.5f, 0.7f);
    g.setFont (tagFont (8.6f));
    g.setColour (selected ? palette::red.brighter (0.3f) : palette::tagText);
    g.drawText (juce::String (slot + 1).paddedLeft ('0', 2), Rc (np.getX() + 4.0f, np.getY(), 16.0f, np.getHeight()), juce::Justification::centredLeft, false);
    g.setFont (stencilFont (8.8f).withExtraKerningFactor (0.06f));
    g.setColour (palette::ink);
    g.drawText (moduleInfo (type).shortName, Rc (np.getX() + 21.0f, np.getY(), np.getWidth() - 34.0f, np.getHeight()), juce::Justification::centredLeft, false);
    jewel (g, { np.getRight() - 7.0f, np.getCentreY() }, 3.0f, juce::Colour (0xffff2e14), on);
    stencil (g, "PWR", f.getX() + 23.0f, f.getBottom() - 17.0f, 7.0f, juce::Justification::left);
    stencil (g, "MIX", f.getRight() - 35.0f, f.getBottom() - 18.0f, 7.0f, juce::Justification::right);
    pullHandle (g, f.getCentreX() - 13.0f, f.getCentreX() + 9.0f, f.getBottom() - 9.0f);
}

void BayUnit::paintOverChildren (juce::Graphics& g)
{
    const auto f = face();
    if (type > 0 && ! on)
    {
        g.setColour (juce::Colour (0x40000000));
        g.fillRoundedRectangle (f.withTrimmedTop (22.0f), 3.0f);
    }
    if (selected || dragOver)
    {
        g.setColour (dragOver ? palette::brand : palette::red);
        g.drawRoundedRectangle ((type > 0 ? f : getLocalBounds().toFloat().reduced (3.0f)).expanded (type > 0 ? 2.0f : 0.0f), 4.0f, 1.6f);
    }
}

void BayUnit::mouseDown (const juce::MouseEvent&)
{
    dragStarted = false;
    if (type <= 0) { showModuleMenu (proc, slot, this); return; }
    if (onSelect) onSelect (slot);
}

void BayUnit::mouseDrag (const juce::MouseEvent& e)
{
    if (type <= 0 || dragStarted || e.getDistanceFromDragStart() < 6 || ! namePlate().expanded (14.0f, 4.0f).contains (e.getMouseDownPosition().toFloat())) return;
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragStarted = true;
        auto img = createComponentSnapshot (getLocalBounds(), true, 1.0f).rescaled (getWidth() / 2, getHeight() / 2);
        c->startDragging ("slot:" + juce::String (slot), this, juce::ScaledImage (img));
    }
}

void BayUnit::mouseUp (const juce::MouseEvent& e)
{
    if (! dragStarted && type > 0 && e.mouseWasClicked() && namePlate().contains (e.getPosition().toFloat()))
        showModuleMenu (proc, slot, this);
}

bool BayUnit::isInterestedInDragSource (const SourceDetails& d) { return d.description.toString().startsWith ("slot:"); }

void BayUnit::itemDropped (const SourceDetails& d)
{
    dragOver = false;
    const int from = d.description.toString().fromFirstOccurrenceOf ("slot:", false, false).getIntValue();
    proc.moveSlot (from, slot);
    if (onSelect) onSelect (slot);
    repaint();
}

//==============================================================================
ChainView::ChainView (HarshVoxProcessor& p) : proc (p)
{
    for (int s = 0; s < kMainSlots; ++s)
    {
        bays.push_back (std::make_unique<BayUnit> (p, s));
        bays.back()->onSelect = [this] (int sl) { if (onSelect) onSelect (sl); };
        const auto b = layout::bay (s).translated (-layout::bayX0, -layout::bayY);
        bays.back()->setBounds (b.toNearestInt());
        addAndMakeVisible (*bays.back());
    }
}

void ChainView::tick() { for (auto& b : bays) b->tick(); }

void ChainView::setSelected (int s)
{
    for (auto& b : bays) { b->selected = b->slot == s; b->repaint(); }
}

uint8_t ChainView::occupancy() const
{
    uint8_t m = 0;
    for (int s = 0; s < kMainSlots; ++s) if (proc.slotType (s) > 0) m |= (uint8_t) (1u << s);
    return m;
}

//==============================================================================
SlotDetail::SlotDetail (HarshVoxProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, true);
    addAndMakeVisible (dial);
    captureButton.setTooltip ("Store the current vocal-tract shape as the Freeze target (saved with the session)");
    captureButton.onClick = [this]
    {
        proc.requestCapture (slot);
        const int k = paramIndex (ModuleType::FormantFreeze, "Target");
        proc.setParamNorm (slotIndex (slot, SP0 + k), moduleInfo (ModuleType::FormantFreeze).params[(size_t) k].toNorm (0.0f));
    };
    addChildComponent (captureButton);
    backButton.setButtonText ("< SPLITTER");
    backButton.onClick = [this]
    {
        for (int s = 0; s < kMainSlots; ++s)
            if (proc.slotType (s) == (int) ModuleType::Splitter) { if (onSelectSlot) onSelectSlot (s); return; }
        if (onSelectSlot) onSelectSlot (0);
    };
    addChildComponent (backButton);
    for (int i = 0; i < 6; ++i)
    {
        auto& b = sendButtons[(size_t) i];
        b.onClick = [this, i]
        {
            const int sub = kMainSlots + i;
            auto* btn = &sendButtons[(size_t) i];
            if (proc.slotType (sub) == 0) { showModuleMenu (proc, sub, btn, [this, sub] { if (onSelectSlot) onSelectSlot (sub); }); return; }
            juce::PopupMenu m;
            m.addItem (1, "Edit");
            m.addItem (2, "Change module...");
            m.addItem (3, "Clear");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (btn), [this, sub, btn] (int r)
            {
                if (r == 1 && onSelectSlot) onSelectSlot (sub);
                if (r == 2) showModuleMenu (proc, sub, btn);
                if (r == 3) proc.clearSlot (sub);
            });
        };
        addChildComponent (b);
    }
}

void SlotDetail::setSlot (int s)
{
    if (s == slot && type >= 0) return;
    slot = s;
    rebuild();
}

juce::String SlotDetail::title() const
{
    const juce::String where = slot < kMainSlots ? "BAY " + juce::String (slot + 1).paddedLeft ('0', 2) : "SEND " + juce::String (slotLabel (slot));
    return where + "  -  " + (type > 0 ? juce::String (moduleInfo (type).name).toUpperCase() : juce::String ("VACANT"));
}

bool SlotDetail::showSends() const
{
    if (type != (int) ModuleType::Splitter || slot >= kMainSlots) return false;
    for (int s = 0; s < slot; ++s)   // only the first splitter in the chain owns the sends
        if (proc.slotType (s) == (int) ModuleType::Splitter && proc.paramReal (slotIndex (s, SOn)) >= 0.5f) return false;
    return true;
}

void SlotDetail::rebuild()
{
    using namespace layout;
    type = proc.slotType (slot);
    for (auto& k : knobs) removeChildComponent (k.get());
    for (auto& k : switches) removeChildComponent (k.get());
    knobs.clear(); switches.clear();
    if (mixKnob) removeChildComponent (mixKnob.get());
    if (outKnob) removeChildComponent (outKnob.get());
    if (power) removeChildComponent (power.get());
    mixKnob.reset(); outKnob.reset(); power.reset();
    sendsShown = showSends();

    dial.setBounds (Rc (2.0f * (moduleDialR + 14.0f), 2.0f * (moduleDialR + 14.0f) + 14.0f)
                        .withCentre (L (moduleDial).translated (0.0f, -7.0f)).getSmallestIntegerContainer());
    if (type > 0)
    {
        KnobLook big;
        big.r = detailBigR; big.numbers = true; big.labelSize = 8.6f; big.tagSize = 7.4f;
        mixKnob = std::make_unique<ParamKnob> (proc, slotIndex (slot, SMix), slot, kParamsPerSlot, big);
        mixKnob->setLabel ("Mix");
        mixKnob->placeAt (L (detailMix));
        outKnob = std::make_unique<ParamKnob> (proc, slotIndex (slot, SOut), -1, -1, big);
        outKnob->setLabel ("Output");
        outKnob->placeAt (L (detailOut));
        addAndMakeVisible (*mixKnob);
        addAndMakeVisible (*outKnob);
        power = std::make_unique<ToggleSwitch> (proc, slotIndex (slot, SOn), 0.6f);
        power->setTexts ({}, {}, {});
        power->placeAt (L (detailPower));
        addAndMakeVisible (*power);

        const auto& specs = moduleInfo (type).params;
        const float dx = sendsShown ? 78.0f : gridDX;
        KnobLook kl;
        kl.r = detailKnobR; kl.labelSize = 7.8f; kl.tagSize = 7.2f;
        for (int k = 0; k < (int) specs.size(); ++k)
        {
            const Pt c = L ({ gridX0 + (float) (k % 6) * dx, k < 6 ? gridRow0 : gridRow1 });
            const auto& sp = specs[(size_t) k];
            if (sp.map == Map::Toggle || (sp.map == Map::Choice && sp.numChoices() == 2))
            {
                auto sw = std::make_unique<ToggleSwitch> (proc, slotIndex (slot, SP0 + k), 0.72f);
                juce::String up = "ON", down = "OFF";
                if (sp.map == Map::Choice) { up = juce::String (sp.choices[1]).toUpperCase(); down = juce::String (sp.choices[0]).toUpperCase(); }
                sw->setTexts (up, down, juce::String (sp.name).toUpperCase());
                sw->nameSize = 7.8f;
                sw->placeAt (c.translated (0.0f, 2.0f));
                addAndMakeVisible (*sw);
                switches.push_back (std::move (sw));
            }
            else
            {
                knobs.push_back (std::make_unique<ParamKnob> (proc, slotIndex (slot, SP0 + k), slot, k, kl));
                knobs.back()->placeAt (c);
                addAndMakeVisible (*knobs.back());
            }
        }
    }
    captureButton.placeAt (L (captureBtn));
    captureButton.setVisible (type == (int) ModuleType::FormantFreeze);
    backButton.setBounds (Rc (90.0f, 18.0f).withCentre (L ({ dividerX + 60.0f, detailPlate.getY() + 2.0f })).toNearestInt());
    backButton.setVisible (slot >= kMainSlots);
    for (int i = 0; i < 6; ++i)
    {
        const auto a = sendsArea.translated (-getX(), -getY());
        const float y = a.getY() + 18.0f + (float) (i / 2) * 38.0f;
        sendButtons[(size_t) i].setBounds (Rc (a.getX() + 66.0f + (float) (i % 2) * 52.0f, y - 9.0f, 48.0f, 18.0f).toNearestInt());
        sendButtons[(size_t) i].setVisible (sendsShown);
    }
    for (auto& t : sendTypes) t = -1;
    repaint();
    if (auto* p = getParentComponent()) p->repaint();
}

void SlotDetail::tick()
{
    if (proc.slotType (slot) != type || showSends() != sendsShown) rebuild();
    for (auto& k : knobs) k->sync();
    for (auto& k : switches) k->sync();
    if (mixKnob) mixKnob->sync();
    if (outKnob) outKnob->sync();
    if (power) power->sync();
    if (sendsShown)
        for (int i = 0; i < 6; ++i)
        {
            const int t = proc.slotType (kMainSlots + i);
            if (t != sendTypes[(size_t) i])
            {
                sendTypes[(size_t) i] = t;
                sendButtons[(size_t) i].setButtonText (t > 0 ? juce::String (moduleInfo (t).shortName) : "+");
            }
        }
}

void SlotDetail::paint (juce::Graphics& g)
{
    using namespace layout;
    const float x = dividerX - (float) getX();
    g.setColour (palette::ink.withAlpha (0.6f));
    g.drawLine (x, 18.0f, x, (float) getHeight() - 12.0f, 0.8f);
    if (type > 0) mil::stencil (g, "PWR", L (detailPower).x - 11.0f, L (detailPower).y, 7.4f, juce::Justification::right);
    if (sendsShown)
    {
        const auto a = sendsArea.translated ((float) -getX(), (float) -getY());
        mil::stencil (g, "SENDS", a.getX(), a.getY(), 8.0f, juce::Justification::left, palette::ink, 1.0f, 0.15f);
        const char* names[] = { "VOICED", "FRICATIVE", "PLOSIVE" };
        for (int p = 0; p < 3; ++p)
            mil::stencil (g, names[p], a.getX(), a.getY() + 18.0f + (float) p * 38.0f, 7.6f, juce::Justification::left, palette::ink, 1.0f, 0.08f);
    }
    if (type <= 0)
        mil::stencil (g, "TURN THE MODULE SELECTOR TO FIT A UNIT IN THIS BAY", L ({ 620.0f, 542.0f }).x, L ({ 620.0f, 542.0f }).y, 9.0f,
                      juce::Justification::centred, palette::ink, 0.8f, 0.12f);
}

void SlotDetail::ModuleDial::paint (juce::Graphics& g)
{
    const Pt c ((float) getWidth() * 0.5f, (float) getHeight() * 0.5f + 7.0f);
    mil::stencil (g, "MODULE", c.x, c.y - layout::moduleDialR - 17.0f, 8.8f, juce::Justification::centred, palette::ink, 1.0f, 0.12f);
    juce::StringArray labels;
    for (int t = 0; t < kNumModuleTypes; ++t) labels.add (moduleAbbrev (t));
    mil::bigDial (g, c, layout::moduleDialR, (float) std::max (0, owner.type) / (float) kNumModuleTypes, labels);
}

void SlotDetail::ModuleDial::mouseDown (const juce::MouseEvent&)
{
    showModuleMenu (owner.proc, owner.slot, this);
}

//==============================================================================
ModMatrixPanel::ModMatrixPanel (HarshVoxProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, true);
    KnobLook kl;
    kl.r = 11.0f; kl.scale = false; kl.labelSize = 7.6f; kl.tagSize = 7.0f;
    const float colW = layout::CW / (float) kNumMods;
    for (int m = 0; m < kNumMods; ++m)
    {
        auto& c = cols[(size_t) m];
        const float x0 = (float) m * colW + 8.0f;
        auto make = [&] (int field, const char* name, Pt at)
        {
            auto k = std::make_unique<ParamKnob> (proc, modIndex (m, field), -1, -1, kl);
            k->setLabel (name);
            k->placeAt (at);
            addAndMakeVisible (*k);
            return k;
        };
        c.source = make (MSrc, "Source", { x0 + 44.0f, 40.0f });
        c.curve  = make (MCurve, "Curve", { x0 + 236.0f, 40.0f });
        c.amount = make (MAmt, "Amount", { x0 + 44.0f, 104.0f });
        c.thresh = make (MThr, "Threshold", { x0 + 140.0f, 104.0f });
        c.smooth = make (MSmooth, "Smooth", { x0 + 236.0f, 104.0f });
        c.target.setBounds (Rc (x0 + 92.0f, 32.0f, 96.0f, 18.0f).toNearestInt());
        c.target.onClick = [this, m] { showTargetMenu (m); };
        addAndMakeVisible (c.target);
    }
}

juce::String ModMatrixPanel::targetName (int m) const
{
    const int tgt = (int) std::lround (proc.paramReal (modIndex (m, MTgt)));
    if (tgt <= 0) return "NONE";
    const int s = (tgt - 1) / kTargetsPerSlot, k = (tgt - 1) % kTargetsPerSlot;
    const int t = proc.slotType (s);
    if (t <= 0) return "(EMPTY)";
    const juce::String pn = k == kParamsPerSlot ? juce::String ("Mix") : (k < (int) moduleInfo (t).params.size() ? juce::String (moduleInfo (t).params[(size_t) k].name) : "?");
    return "S" + juce::String (slotLabel (s)) + " " + pn.toUpperCase();
}

void ModMatrixPanel::showTargetMenu (int m)
{
    juce::PopupMenu menu;
    menu.addItem (1, "None");
    for (int s = 0; s < kNumSlots; ++s)
    {
        const int t = proc.slotType (s);
        if (t <= 0) continue;
        menu.addSectionHeader ("S" + juce::String (slotLabel (s)) + "  " + moduleInfo (t).name);
        const auto& specs = moduleInfo (t).params;
        for (int k = 0; k < (int) specs.size(); ++k)
            if (specs[(size_t) k].continuous())
                menu.addItem (2 + s * kTargetsPerSlot + k, "S" + juce::String (slotLabel (s)) + " " + specs[(size_t) k].name);
        menu.addItem (2 + s * kTargetsPerSlot + kParamsPerSlot, "S" + juce::String (slotLabel (s)) + " Mix");
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&cols[(size_t) m].target).withMinimumWidth (220), [this, m] (int id)
    {
        if (id <= 0) return;
        auto* prm = proc.param (modIndex (m, MTgt));
        prm->beginChangeGesture();
        proc.setParamReal (modIndex (m, MTgt), (float) (id - 1));
        prm->endChangeGesture();
    });
}

void ModMatrixPanel::tick()
{
    for (int m = 0; m < kNumMods; ++m)
    {
        auto& c = cols[(size_t) m];
        for (auto* k : { c.source.get(), c.curve.get(), c.amount.get(), c.thresh.get(), c.smooth.get() }) k->sync();
        const auto name = targetName (m);
        if (c.target.getButtonText() != name) c.target.setButtonText (name);
        const bool act = proc.paramReal (modIndex (m, MSrc)) > 0.5f && proc.paramReal (modIndex (m, MTgt)) > 0.5f;
        if (act != c.active) { c.active = act; repaint(); }
    }
}

void ModMatrixPanel::paint (juce::Graphics& g)
{
    const float colW = layout::CW / (float) kNumMods;
    for (int m = 0; m < kNumMods; ++m)
    {
        const float x0 = (float) m * colW + 8.0f;
        if (m > 0)
        {
            g.setColour (palette::ink.withAlpha (0.45f));
            g.drawLine (x0 - 8.0f, 16.0f, x0 - 8.0f, (float) getHeight() - 12.0f, 0.8f);
        }
        mil::stencil (g, "MOD " + juce::String (m + 1), x0 + 140.0f, 18.0f, 8.6f, juce::Justification::centred, palette::ink, 1.0f, 0.15f);
        mil::jewel (g, { x0 + 172.0f, 18.0f }, 3.2f, palette::green, cols[(size_t) m].active);
        mil::stencil (g, "TARGET", x0 + 140.0f, 61.0f, 7.6f, juce::Justification::centred, palette::ink, 1.0f, 0.05f);
    }
}
} // namespace hv::ui
