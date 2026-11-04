#include "Widgets.h"
#include "BinaryData.h"

namespace hv::ui
{
using namespace params;

juce::Font uiFont (float h)
{
    static auto tf = juce::Typeface::createSystemTypefaceFor (BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}
juce::Font monoFont (float h)
{
    static auto tf = juce::Typeface::createSystemTypefaceFor (BinaryData::DMMonoRegular_ttf, BinaryData::DMMonoRegular_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    using namespace palette;
    setColour (juce::ResizableWindow::backgroundColourId, bg0);
    setColour (juce::PopupMenu::backgroundColourId, panel2);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, textDim);
    setColour (juce::ComboBox::backgroundColourId, knob);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, edge);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::TextButton::buttonColourId, knob);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TooltipWindow::backgroundColourId, panel2);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, edge);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, knob);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, edge);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    using namespace palette;
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = std::min (area.getWidth(), area.getHeight()) * 0.5f - 4.0f;
    const auto c = area.getCentre();
    // body
    g.setColour (knob);
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    g.setColour (edge);
    g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.0f);
    // track + value arc
    const float ar = r + 3.5f;
    juce::Path track;
    track.addCentredArc (c.x, c.y, ar, ar, 0.0f, start, end, true);
    g.setColour (line.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    const float ang = start + pos * (end - start);
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) * 0.5f : start;
    juce::Path val;
    val.addCentredArc (c.x, c.y, ar, ar, 0.0f, std::min (from, ang), std::max (from, ang), true);
    g.setColour (s.isEnabled() ? text.withAlpha (0.85f) : textFaint);
    g.strokePath (val, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    // pointer: square-ended stroke, house style
    const juce::Point<float> p1 (c.x + std::sin (ang) * r * 0.25f, c.y - std::cos (ang) * r * 0.25f);
    const juce::Point<float> p2 (c.x + std::sin (ang) * r * 0.82f, c.y - std::cos (ang) * r * 0.82f);
    g.setColour (text);
    g.drawLine ({ p1, p2 }, std::max (2.0f, r * 0.12f));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace palette;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? accent.withAlpha (down ? 0.7f : 0.9f) : (down ? edge : over ? knob.brighter (0.08f) : knob));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (on ? accent.brighter (0.2f) : edge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.isEnabled() ? palette::text : palette::textFaint);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    using namespace palette;
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (knob);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (edge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path tri;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    tri.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (textDim);
    g.fillPath (tri);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (palette::panel2);
    g.setColour (palette::edge);
    g.drawRect (0, 0, w, h);
}

//==============================================================================
ParamKnob::ParamKnob (HarshVoxProcessor& p, int paramIndex, int s, int li, bool c)
    : proc (p), index (paramIndex), slot (s), liveIdx (li), compact (c)
{
    slider.owner = this;
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setRange (0.0, 1.0, 0.0);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    auto* prm = proc.param (index);
    slider.setDoubleClickReturnValue (true, prm->getDefaultValue());
    slider.getProperties().set ("bipolar", prm->getParameterID().contains ("_amt"));
    slider.onDragStart = [this] { dragging = true; proc.param (index)->beginChangeGesture(); };
    slider.onDragEnd   = [this] { dragging = false; proc.param (index)->endChangeGesture(); };
    slider.onValueChange = [this]
    {
        if (syncing) return;
        auto* prm2 = proc.param (index);
        if (! dragging) prm2->beginChangeGesture();
        prm2->setValueNotifyingHost ((float) slider.getValue());
        if (! dragging) prm2->endChangeGesture();
        repaint();
    };
    addAndMakeVisible (slider);
    configureSteps();
    sync();
}

void ParamKnob::configureSteps()
{
    steps = 0;
    if (auto* sp = dynamic_cast<SlotParam*> (proc.param (index)))
    {
        if (auto* spec = sp->spec())
        {
            if (spec->map == Map::Choice) steps = std::max (1, spec->numChoices() - 1);
            else if (spec->map == Map::Toggle) steps = 1;
            else if (spec->map == Map::Int) steps = (int) (spec->max - spec->min);
            slider.setDoubleClickReturnValue (true, spec->toNorm (spec->def));
        }
    }
    else if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.param (index))) steps = ch->choices.size() - 1;
    else if (auto* in = dynamic_cast<juce::AudioParameterInt*> (proc.param (index))) steps = in->getRange().getLength();
    else if (dynamic_cast<juce::AudioParameterBool*> (proc.param (index)) != nullptr) steps = 1;
    slider.setRange (0.0, 1.0, steps > 0 && steps < 2000 ? 1.0 / steps : 0.0);
    if (steps > 0 && steps <= 8) slider.setMouseDragSensitivity (60 * steps);
    else slider.setMouseDragSensitivity (220);
}

juce::String ParamKnob::currentName() const
{
    if (label.isNotEmpty()) return label;
    if (auto* sp = dynamic_cast<SlotParam*> (proc.param (index)))
        if (auto* spec = sp->spec()) return spec->name;
    return proc.param (index)->getName (32);
}

void ParamKnob::sync()
{
    if (slot >= 0)
    {
        const int t = proc.slotType (slot);
        if (t != lastType) { lastType = t; configureSteps(); }
    }
    if (! dragging)
    {
        syncing = true;
        slider.setValue (proc.param (index)->getValue(), juce::dontSendNotification);
        syncing = false;
    }
    float lv = -1.0f;
    if (slot >= 0 && liveIdx >= 0)
    {
        lv = proc.engine().slotShared (slot).liveNorm[(size_t) liveIdx].load (std::memory_order_relaxed);
        const float base = proc.param (index)->getValue();
        bool reactive = false;
        if (auto* sp = dynamic_cast<SlotParam*> (proc.param (index)))
            if (auto* spec = sp->spec()) reactive = spec->reactive && spec->continuous();
        const bool on = proc.paramReal (slotIndex (slot, SOn)) >= 0.5f;
        showLive = on && (reactive || std::abs (lv - base) > 0.003f);
    }
    if (std::abs (lv - liveValue) > 0.001f) { liveValue = lv; repaint(); }
    slider.repaint();
}

void ParamKnob::setBarStyle (bool b)
{
    bar = b;
    slider.setSliderStyle (b ? juce::Slider::LinearBar : juce::Slider::RotaryVerticalDrag);
    slider.setColour (juce::Slider::trackColourId, palette::edge);
    slider.setColour (juce::Slider::backgroundColourId, palette::knob);
    slider.setInterceptsMouseClicks (true, true);
    resized();
}

void ParamKnob::resized()
{
    if (bar) { slider.setBounds (getLocalBounds()); return; }
    auto r = getLocalBounds();
    const int textH = compact ? 24 : 28;
    r.removeFromBottom (textH);
    const int side = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (side, side));
}

void ParamKnob::KnobSlider::paint (juce::Graphics& g)
{
    if (owner != nullptr && owner->bar)
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (palette::knob);
        g.fillRoundedRectangle (r, 3.0f);
        const bool bip = getProperties().getWithDefault ("bipolar", false);
        const float v = (float) getValue();
        auto fill = bip ? juce::Rectangle<float> (r.getX() + r.getWidth() * std::min (0.5f, v), r.getY(), r.getWidth() * std::abs (v - 0.5f), r.getHeight())
                        : r.withWidth (r.getWidth() * v);
        g.setColour (palette::edge.brighter (0.25f));
        g.fillRoundedRectangle (fill, 3.0f);
        g.setColour (palette::edge);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setFont (uiFont (10.5f));
        g.setColour (palette::textDim);
        g.drawText (owner->currentName().toUpperCase(), r.reduced (6.0f, 0.0f), juce::Justification::centredLeft);
        g.setFont (monoFont (11.0f));
        g.setColour (palette::text);
        g.drawText (owner->proc.param (owner->index)->getText (v, 24), r.reduced (6.0f, 0.0f), juce::Justification::centredRight);
        return;
    }
    juce::Slider::paint (g);
    if (owner == nullptr || ! owner->showLive || owner->liveValue < 0.0f) return;
    // live ring: thin outer arc from the set value to the live value + a marker square
    const auto area = getLocalBounds().toFloat().reduced (4.0f);
    const float r = std::min (area.getWidth(), area.getHeight()) * 0.5f - 4.0f + 7.5f;
    const auto c = area.getCentre();
    const auto rp = getRotaryParameters();
    const float a0 = rp.startAngleRadians + (float) getValue() * (rp.endAngleRadians - rp.startAngleRadians);
    const float a1 = rp.startAngleRadians + owner->liveValue * (rp.endAngleRadians - rp.startAngleRadians);
    juce::Path p;
    p.addCentredArc (c.x, c.y, r, r, 0.0f, std::min (a0, a1), std::max (a0, a1) + 0.001f, true);
    g.setColour (palette::live);
    g.strokePath (p, juce::PathStrokeType (2.0f));
    const juce::Point<float> m (c.x + std::sin (a1) * r, c.y - std::cos (a1) * r);
    g.fillRect (juce::Rectangle<float> (4.0f, 4.0f).withCentre (m));
}

void ParamKnob::paint (juce::Graphics& g)
{
    if (bar) return;
    auto r = getLocalBounds().toFloat();
    const float textH = compact ? 24.0f : 28.0f;
    auto t = r.removeFromBottom (textH);
    g.setColour (palette::textDim);
    g.setFont (uiFont (compact ? 10.5f : 11.5f));
    g.drawFittedText (currentName().toUpperCase(), t.removeFromTop (textH * 0.5f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    g.setColour (palette::text);
    g.setFont (monoFont (compact ? 10.5f : 11.5f));
    const auto txt = proc.param (index)->getText ((float) slider.getValue(), 24);
    g.drawFittedText (txt, t.toNearestInt(), juce::Justification::centred, 1, 0.8f);
}

//==============================================================================
void Meter::tick()
{
    auto take = [] (std::atomic<float>* a) { return a != nullptr ? a->exchange (0.0f, std::memory_order_relaxed) : 0.0f; };
    const float a = take (srcA), b = srcB != nullptr ? take (srcB) : a;
    levelA = std::max (a, levelA * 0.85f);
    levelB = std::max (b, levelB * 0.85f);
    if (a > holdA || b > holdB || ++holdCount > 45) { holdA = std::max (a, holdA * (holdCount > 45 ? 0.0f : 1.0f)); holdB = std::max (b, holdB * (holdCount > 45 ? 0.0f : 1.0f)); holdCount = 0; }
    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    using namespace palette;
    auto r = getLocalBounds().toFloat();
    g.setColour (bg0);
    g.fillRect (r);
    auto toY = [&r] (float lin)
    {
        const float db = juce::Decibels::gainToDecibels (lin, -60.0f);
        return r.getBottom() - r.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    };
    const float bw = (r.getWidth() - 1.0f) * 0.5f;
    for (int ch = 0; ch < 2; ++ch)
    {
        const float lvl = ch == 0 ? levelA : levelB, hold = ch == 0 ? holdA : holdB;
        auto col = r.withWidth (bw).translated ((bw + 1.0f) * (float) ch, 0.0f);
        const float y = toY (lvl);
        // segmented bar
        for (float yy = col.getBottom() - 3.0f; yy > y; yy -= 3.0f)
        {
            const float db = (r.getBottom() - yy) / r.getHeight() * 60.0f - 60.0f;
            g.setColour (db > -3.0f ? brand : db > -12.0f ? amber.withAlpha (0.9f) : text.withAlpha (0.75f));
            g.fillRect (col.getX(), yy, col.getWidth(), 2.0f);
        }
        if (hold > 0.0f)
        {
            g.setColour (text);
            g.fillRect (col.getX(), toY (hold), col.getWidth(), 1.5f);
        }
    }
}

//==============================================================================
void PowerButton::paintButton (juce::Graphics& g, bool over, bool)
{
    using namespace palette;
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float s = std::min (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (s, s);
    const bool on = getToggleState();
    g.setColour (on ? accent : (over ? edge.brighter (0.2f) : edge));
    g.fillRect (r);
    g.setColour (on ? palette::text : palette::textDim);
    g.fillRect (r.reduced (s * 0.3f));
}
} // namespace hv::ui
