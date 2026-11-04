#include "SlotComponents.h"

namespace hv::ui
{
using namespace params;

void showModuleMenu (HarshVoxProcessor& p, int slot, juce::Component* target, std::function<void()> after)
{
    juce::PopupMenu m;
    m.addSectionHeader (slot < kMainSlots ? "SLOT " + juce::String (slot + 1) : "SEND " + juce::String (slotLabel (slot)));
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

//==============================================================================
SlotCard::SlotCard (HarshVoxProcessor& p, int s) : slot (s), proc (p)
{
    power.setClickingTogglesState (true);
    power.setTooltip ("Bypass");
    power.onClick = [this] { proc.setParamReal (slotIndex (slot, SOn), power.getToggleState() ? 1.0f : 0.0f); };
    addAndMakeVisible (power);
    rebuild();
}

void SlotCard::rebuild()
{
    type = proc.slotType (slot);
    for (auto& k : keys) removeChildComponent (k.get());
    keys.clear();
    if (mixKnob) removeChildComponent (mixKnob.get());
    mixKnob.reset();
    if (type > 0)
    {
        const auto& specs = moduleInfo (type).params;
        for (int k = 0; k < (int) specs.size() && keys.size() < 4; ++k)
            if (specs[(size_t) k].key)
            {
                keys.push_back (std::make_unique<ParamKnob> (proc, slotIndex (slot, SP0 + k), slot, k, true));
                addAndMakeVisible (*keys.back());
            }
        mixKnob = std::make_unique<ParamKnob> (proc, slotIndex (slot, SMix), slot, kParamsPerSlot, true);
        mixKnob->setLabel ("Mix");
        addAndMakeVisible (*mixKnob);
    }
    power.setVisible (type > 0);
    resized();
    repaint();
}

void SlotCard::tick()
{
    if (proc.slotType (slot) != type) rebuild();
    for (auto& k : keys) k->sync();
    if (mixKnob) mixKnob->sync();
    power.setToggleState (proc.paramReal (slotIndex (slot, SOn)) >= 0.5f, juce::dontSendNotification);
    const float l = proc.engine().slotShared (slot).level.load (std::memory_order_relaxed);
    const float nl = std::max (l, level * 0.85f);
    if (std::abs (nl - level) > 0.002f) { level = nl; repaint (getLocalBounds().removeFromBottom (5)); }
}

void SlotCard::resized()
{
    auto r = getLocalBounds().reduced (6, 0);
    auto h = r.removeFromTop (26);
    power.setBounds (h.removeFromRight (18).withSizeKeepingCentre (16, 16));
    r.removeFromBottom (8);
    if (type <= 0) return;
    auto mixRow = r.removeFromBottom (std::min (78, r.getHeight() / 3));
    if (mixKnob) mixKnob->setBounds (mixRow.withSizeKeepingCentre (std::min (mixRow.getWidth(), 70), mixRow.getHeight()));
    const int n = (int) keys.size();
    if (n == 0) return;
    const int cols = 2, rowsN = (n + 1) / 2;
    const int cw = r.getWidth() / cols, rh = r.getHeight() / rowsN;
    for (int i = 0; i < n; ++i)
        keys[(size_t) i]->setBounds (r.getX() + (i % cols) * cw, r.getY() + (i / cols) * rh, cw, rh);
}

void SlotCard::paint (juce::Graphics& g)
{
    using namespace palette;
    auto r = getLocalBounds().toFloat();
    const bool on = proc.paramReal (slotIndex (slot, SOn)) >= 0.5f;
    g.setColour (panel);
    g.fillRoundedRectangle (r, 5.0f);
    auto h = r.removeFromTop (26.0f);
    g.setColour (bg1);
    g.fillRoundedRectangle (h, 5.0f);
    g.fillRect (h.withTrimmedTop (10.0f));
    // number + module name in block lettering
    const float cap = 8.0f;
    BlockFont::draw (g, juce::String (slot + 1).paddedLeft ('0', 2), h.getX() + 8.0f, h.getCentreY() - cap * 0.5f, cap,
                     type > 0 && on ? accent : textFaint);
    const juce::String name = type > 0 ? moduleInfo (type).shortName : "EMPTY";
    BlockFont::draw (g, name, h.getX() + 30.0f, h.getCentreY() - cap * 0.5f, cap, type > 0 ? (on ? text : textDim) : textFaint);
    if (type <= 0)
    {
        auto body = r.reduced (10.0f);
        g.setColour (line);
        const float dash[] = { 4.0f, 4.0f };
        juce::Path rp; rp.addRectangle (body);
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, rp, dash, 2);
        g.fillPath (dashed);
        BlockFont::drawCentred (g, "+", body.withTrimmedBottom (24.0f), 14.0f, textDim);
        BlockFont::drawCentred (g, "ADD", body.withTrimmedTop (body.getHeight() * 0.5f), 7.0f, textFaint);
    }
    // level strip
    auto lv = getLocalBounds().toFloat().removeFromBottom (4.0f).reduced (6.0f, 0.0f);
    g.setColour (bg0);
    g.fillRect (lv);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    g.setColour (db > -3.0f ? brand : text.withAlpha (0.6f));
    g.fillRect (lv.withWidth (lv.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f)));
}

void SlotCard::paintOverChildren (juce::Graphics& g)
{
    if (selected || dragOver)
    {
        g.setColour (dragOver ? palette::brand : palette::accent);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, dragOver ? 2.0f : 1.5f);
    }
    if (proc.paramReal (slotIndex (slot, SOn)) < 0.5f && type > 0)
    {
        g.setColour (palette::bg0.withAlpha (0.45f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().withTrimmedTop (26.0f), 5.0f);
    }
}

void SlotCard::mouseDown (const juce::MouseEvent& e)
{
    dragStarted = false;
    if (type <= 0) { showModuleMenu (proc, slot, this); return; }
    if (onSelect) onSelect (slot);
    juce::ignoreUnused (e);
}

void SlotCard::mouseDrag (const juce::MouseEvent& e)
{
    if (type <= 0 || dragStarted || e.getDistanceFromDragStart() < 6 || ! headerArea().contains (e.getMouseDownPosition())) return;
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragStarted = true;
        auto img = createComponentSnapshot (getLocalBounds()).rescaled (getWidth() / 2, getHeight() / 2);
        c->startDragging ("slot:" + juce::String (slot), this, juce::ScaledImage (img));
    }
}

void SlotCard::mouseUp (const juce::MouseEvent& e)
{
    if (! dragStarted && type > 0 && headerArea().contains (e.getPosition()) && e.getPosition().x > 26
        && e.getPosition().x < getWidth() - 26 && e.mouseWasClicked())
        showModuleMenu (proc, slot, this);
}

bool SlotCard::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("slot:");
}

void SlotCard::itemDropped (const SourceDetails& d)
{
    dragOver = false;
    const int from = d.description.toString().fromFirstOccurrenceOf ("slot:", false, false).getIntValue();
    proc.moveSlot (from, slot);
    if (onSelect) onSelect (slot);
    repaint();
}

//==============================================================================
ChainView::ChainView (HarshVoxProcessor& p)
{
    for (int s = 0; s < kMainSlots; ++s)
    {
        cards.push_back (std::make_unique<SlotCard> (p, s));
        cards.back()->onSelect = [this] (int sl) { if (onSelect) onSelect (sl); };
        addAndMakeVisible (*cards.back());
    }
}

void ChainView::tick() { for (auto& c : cards) c->tick(); }

void ChainView::setSelected (int s)
{
    for (auto& c : cards) { c->selected = c->slot == s; c->repaint(); }
}

void ChainView::resized()
{
    auto r = getLocalBounds();
    const int gap = 6, n = (int) cards.size();
    const int w = (r.getWidth() - gap * (n - 1)) / n;
    for (int i = 0; i < n; ++i) cards[(size_t) i]->setBounds (r.getX() + i * (w + gap), r.getY(), w, r.getHeight());
}

void ChainView::paint (juce::Graphics& g)
{
    // signal-flow ticks between cards
    g.setColour (palette::textFaint);
    const int n = (int) cards.size();
    for (int i = 0; i + 1 < n; ++i)
    {
        const auto& c = cards[(size_t) i];
        g.fillRect ((float) c->getRight() + 1.0f, (float) c->getY() + 12.0f, 4.0f, 2.0f);
    }
}

//==============================================================================
SlotDetail::SlotDetail (HarshVoxProcessor& p) : proc (p)
{
    power.setClickingTogglesState (true);
    power.onClick = [this] { proc.setParamReal (slotIndex (slot, SOn), power.getToggleState() ? 1.0f : 0.0f); };
    addAndMakeVisible (power);
    moduleButton.onClick = [this] { showModuleMenu (proc, slot, &moduleButton); };
    addAndMakeVisible (moduleButton);
    captureButton.setTooltip ("Store the current vocal-tract shape as the Freeze target (saved with the session)");
    captureButton.onClick = [this]
    {
        proc.requestCapture (slot);
        const int k = paramIndex (ModuleType::FormantFreeze, "Target");
        proc.setParamNorm (slotIndex (slot, SP0 + k), moduleInfo (ModuleType::FormantFreeze).params[(size_t) k].toNorm (0.0f));
    };
    addChildComponent (captureButton);
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
            if (proc.slotType (sub) == 0) { showModuleMenu (proc, sub, &sendButtons[(size_t) i], [this, sub] { if (onSelectSlot) onSelectSlot (sub); }); return; }
            juce::PopupMenu m;
            m.addItem (1, "Edit");
            m.addItem (2, "Change module...");
            m.addItem (3, "Clear");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sendButtons[(size_t) i]), [this, sub, i] (int r)
            {
                if (r == 1 && onSelectSlot) onSelectSlot (sub);
                if (r == 2) showModuleMenu (proc, sub, &sendButtons[(size_t) i]);
                if (r == 3) proc.clearSlot (sub);
            });
        };
        addChildComponent (b);
    }
    rebuild();
}

void SlotDetail::setSlot (int s)
{
    if (s == slot) return;
    slot = s;
    rebuild();
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
    type = proc.slotType (slot);
    for (auto& k : knobs) removeChildComponent (k.get());
    knobs.clear();
    if (mixKnob) removeChildComponent (mixKnob.get());
    if (outKnob) removeChildComponent (outKnob.get());
    mixKnob.reset(); outKnob.reset();
    if (type > 0)
    {
        const auto& specs = moduleInfo (type).params;
        for (int k = 0; k < (int) specs.size(); ++k)
        {
            knobs.push_back (std::make_unique<ParamKnob> (proc, slotIndex (slot, SP0 + k), slot, k));
            addAndMakeVisible (*knobs.back());
        }
        mixKnob = std::make_unique<ParamKnob> (proc, slotIndex (slot, SMix), slot, kParamsPerSlot, true);
        mixKnob->setLabel ("Mix");
        outKnob = std::make_unique<ParamKnob> (proc, slotIndex (slot, SOut), -1, -1, true);
        outKnob->setLabel ("Output");
        addAndMakeVisible (*mixKnob);
        addAndMakeVisible (*outKnob);
    }
    moduleButton.setButtonText (type > 0 ? juce::String (moduleInfo (type).name) : "Choose module...");
    captureButton.setVisible (type == (int) ModuleType::FormantFreeze);
    backButton.setVisible (slot >= kMainSlots);
    const bool sends = showSends();
    for (int i = 0; i < 6; ++i) sendButtons[(size_t) i].setVisible (sends);
    power.setVisible (type > 0);
    resized();
    repaint();
}

void SlotDetail::tick()
{
    if (proc.slotType (slot) != type) rebuild();
    for (auto& k : knobs) k->sync();
    if (mixKnob) mixKnob->sync();
    if (outKnob) outKnob->sync();
    power.setToggleState (proc.paramReal (slotIndex (slot, SOn)) >= 0.5f, juce::dontSendNotification);
    if (showSends())
        for (int i = 0; i < 6; ++i)
        {
            const int t = proc.slotType (kMainSlots + i);
            if (t != sendTypes[(size_t) i] || sendButtons[(size_t) i].getButtonText().isEmpty())
            {
                sendTypes[(size_t) i] = t;
                sendButtons[(size_t) i].setButtonText (t > 0 ? juce::String (moduleInfo (t).shortName) : "+");
            }
        }
}

void SlotDetail::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    auto left = r.removeFromLeft (190);
    left.removeFromTop (22);
    auto row = left.removeFromTop (26);
    power.setBounds (row.removeFromRight (24));
    row.removeFromRight (6);
    moduleButton.setBounds (row);
    left.removeFromTop (6);
    auto extra = left.removeFromBottom (26);
    if (slot >= kMainSlots) backButton.setBounds (extra.removeFromLeft (100));
    captureButton.setBounds (extra.removeFromRight (90));
    left.removeFromBottom (4);
    if (mixKnob) mixKnob->setBounds (left.removeFromLeft (left.getWidth() / 2));
    if (outKnob) outKnob->setBounds (left);

    r.removeFromLeft (14);
    if (showSends())
    {
        auto sends = r.removeFromRight (std::min (300, r.getWidth() / 3));
        sends.removeFromTop (22);
        const int rh = sends.getHeight() / 3;
        for (int p = 0; p < 3; ++p)
        {
            auto rr = sends.removeFromTop (rh).reduced (0, 4);
            rr.removeFromLeft (90);
            const int bw = (rr.getWidth() - 6) / 2;
            sendButtons[(size_t) (p * 2)].setBounds (rr.removeFromLeft (bw).withSizeKeepingCentre (bw, std::min (28, rr.getHeight())));
            rr.removeFromLeft (6);
            sendButtons[(size_t) (p * 2 + 1)].setBounds (rr.withSizeKeepingCentre (bw, std::min (28, rr.getHeight())));
        }
        r.removeFromRight (12);
    }
    const int n = (int) knobs.size();
    if (n == 0) return;
    const int perRow = n > 6 ? (n + 1) / 2 : n;
    const int rowsN = (n + perRow - 1) / perRow;
    const int kw = std::min (96, r.getWidth() / std::max (1, perRow)), kh = r.getHeight() / rowsN;
    for (int i = 0; i < n; ++i)
        knobs[(size_t) i]->setBounds (r.getX() + (i % perRow) * kw, r.getY() + (i / perRow) * kh, kw, kh);
}

void SlotDetail::paint (juce::Graphics& g)
{
    using namespace palette;
    auto r = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 5.0f);
    const juce::String title = slot < kMainSlots ? "SLOT " + juce::String (slot + 1).paddedLeft ('0', 2)
                                                 : "SEND " + juce::String (slotLabel (slot));
    BlockFont::draw (g, title, 12.0f, 12.0f, 9.0f, slot < kMainSlots ? accent : amber);
    if (showSends())
    {
        auto sends = getLocalBounds().reduced (10, 8);
        sends = sends.removeFromRight (std::min (300, (sends.getWidth() - 204) / 3));
        BlockFont::draw (g, "SENDS", (float) sends.getX(), 12.0f, 7.0f, textDim);
        sends.removeFromTop (22);
        const int rh = sends.getHeight() / 3;
        const char* names[] = { "VOICED", "FRICATIVE", "PLOSIVE" };
        for (int p = 0; p < 3; ++p)
        {
            auto rr = sends.removeFromTop (rh);
            BlockFont::draw (g, names[p], (float) rr.getX(), (float) rr.getCentreY() - 3.5f, 7.0f,
                             p == 0 ? text : p == 1 ? amber : brand);
        }
    }
    g.setColour (line);
    g.drawVerticalLine (204, 10.0f, r.getBottom() - 10.0f);
}

//==============================================================================
ModMatrixPanel::ModMatrixPanel (HarshVoxProcessor& p) : proc (p)
{
    for (int m = 0; m < kNumMods; ++m)
    {
        auto& row = rows[(size_t) m];
        for (int s = 0; s < kNumSources; ++s) row.source.addItem (kSourceNames[s], s + 1);
        for (int c = 0; c < 3; ++c) row.curve.addItem (kCurveNames[c], c + 1);
        row.srcAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*proc.param (modIndex (m, MSrc)), row.source);
        row.curveAtt = std::make_unique<juce::ComboBoxParameterAttachment> (*proc.param (modIndex (m, MCurve)), row.curve);
        row.target.onChange = [this, m]
        {
            const int id = rows[(size_t) m].target.getSelectedId();
            if (id > 0) proc.setParamReal (modIndex (m, MTgt), (float) (id - 1));
        };
        row.amount = std::make_unique<ParamKnob> (proc, modIndex (m, MAmt), -1, -1, true);
        row.amount->setLabel ("Amount");
        row.thresh = std::make_unique<ParamKnob> (proc, modIndex (m, MThr), -1, -1, true);
        row.thresh->setLabel ("Threshold");
        row.smooth = std::make_unique<ParamKnob> (proc, modIndex (m, MSmooth), -1, -1, true);
        row.smooth->setLabel ("Smooth");
        row.amount->setBarStyle (true); row.thresh->setBarStyle (true); row.smooth->setBarStyle (true);
        for (juce::Component* c : { (juce::Component*) &row.source, (juce::Component*) &row.target, (juce::Component*) &row.curve,
                                    (juce::Component*) row.amount.get(), (juce::Component*) row.thresh.get(), (juce::Component*) row.smooth.get() })
            addAndMakeVisible (c);
    }
    knownTypes.fill (-1);
    rebuildTargets();
}

void ModMatrixPanel::rebuildTargets()
{
    for (int s = 0; s < kNumSlots; ++s) knownTypes[(size_t) s] = proc.slotType (s);
    for (int m = 0; m < kNumMods; ++m)
    {
        auto& cb = rows[(size_t) m].target;
        cb.clear (juce::dontSendNotification);
        cb.addItem ("None", 1);
        for (int s = 0; s < kNumSlots; ++s)
        {
            const int t = knownTypes[(size_t) s];
            if (t <= 0) continue;
            cb.addSectionHeading ("S" + juce::String (slotLabel (s)) + "  " + moduleInfo (t).name);
            const auto& specs = moduleInfo (t).params;
            for (int k = 0; k < (int) specs.size(); ++k)
                if (specs[(size_t) k].continuous())
                    cb.addItem ("S" + juce::String (slotLabel (s)) + " " + specs[(size_t) k].name, 2 + s * kTargetsPerSlot + k);
            cb.addItem ("S" + juce::String (slotLabel (s)) + " Mix", 2 + s * kTargetsPerSlot + kParamsPerSlot);
        }
    }
}

void ModMatrixPanel::tick()
{
    bool changed = false;
    for (int s = 0; s < kNumSlots; ++s) changed |= proc.slotType (s) != knownTypes[(size_t) s];
    if (changed) rebuildTargets();
    for (int m = 0; m < kNumMods; ++m)
    {
        auto& row = rows[(size_t) m];
        const int id = (int) std::lround (proc.paramReal (modIndex (m, MTgt))) + 1;
        if (row.target.getSelectedId() != id)
        {
            if (row.target.indexOfItemId (id) < 0) row.target.setText ("(empty slot)", juce::dontSendNotification);
            else row.target.setSelectedId (id, juce::dontSendNotification);
        }
        row.amount->sync(); row.thresh->sync(); row.smooth->sync();
    }
}

void ModMatrixPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (22);
    r.removeFromLeft (70);
    const int rh = r.getHeight() / kNumMods;
    for (int m = 0; m < kNumMods; ++m)
    {
        auto row = r.removeFromTop (rh);
        auto& rw = rows[(size_t) m];
        const int ch = std::min (26, row.getHeight() - 4);
        const int unit = row.getWidth() / 10;
        auto place = [&row, ch] (juce::Component& c, int w) { c.setBounds (row.removeFromLeft (w).withSizeKeepingCentre (w - 8, ch)); };
        place (rw.source, unit * 2);
        place (rw.target, unit * 3);
        place (rw.curve, (int) (unit * 1.2));
        const int kw = row.getWidth() / 3;
        place (*rw.amount, kw);
        place (*rw.thresh, kw);
        place (*rw.smooth, row.getWidth());
    }
}

void ModMatrixPanel::paint (juce::Graphics& g)
{
    using namespace palette;
    g.setColour (panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
    BlockFont::draw (g, "MODULATION", 12.0f, 12.0f, 9.0f, accent);
    g.setFont (uiFont (11.0f));
    g.setColour (textDim);
    g.drawText ("input-reactive only: envelope, pitch and phoneme class drive any continuous parameter",
                getLocalBounds().reduced (12, 8).removeFromTop (14).withTrimmedLeft (120), juce::Justification::centredLeft);
    auto r = getLocalBounds().reduced (10, 8);
    r.removeFromTop (22);
    const int rh = r.getHeight() / kNumMods;
    for (int m = 0; m < kNumMods; ++m)
    {
        auto row = r.removeFromTop (rh);
        BlockFont::draw (g, "MOD " + juce::String (m + 1), (float) row.getX() + 4.0f, (float) row.getCentreY() - 3.5f, 7.0f, text);
        if (m > 0) { g.setColour (line); g.drawHorizontalLine (row.getY(), (float) row.getX(), (float) row.getRight()); }
    }
}
} // namespace hv::ui
