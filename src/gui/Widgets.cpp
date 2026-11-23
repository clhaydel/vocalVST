#include "Widgets.h"
#include "BinaryData.h"

namespace hv::ui
{
using namespace params;
using Rc = juce::Rectangle<float>;
using Pt = juce::Point<float>;

namespace
{
juce::Typeface::Ptr face (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); }
}
juce::Font stencilFont (float h)
{
    static auto tf = face (BinaryData::IBMPlexSansCondensedBold_ttf, BinaryData::IBMPlexSansCondensedBold_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}
juce::Font tagFont (float h)
{
    static auto tf = face (BinaryData::IBMPlexMonoBold_ttf, BinaryData::IBMPlexMonoBold_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}
juce::Font uiFont (float h)
{
    static auto tf = face (BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}
juce::Font monoFont (float h)
{
    static auto tf = face (BinaryData::DMMonoRegular_ttf, BinaryData::DMMonoRegular_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    using namespace palette;
    setColour (juce::ResizableWindow::backgroundColourId, menuBg);
    setColour (juce::PopupMenu::backgroundColourId, menuBg);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, textDim);
    setColour (juce::TextButton::buttonColourId, menuEdge);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, text);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TooltipWindow::backgroundColourId, menuBg);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, menuEdge);
    setColour (juce::AlertWindow::backgroundColourId, menuBg);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, menuEdge);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff15160f));
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, menuEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (palette::menuBg);
    g.setColour (palette::menuEdge);
    g.drawRect (0, 0, w, h);
}

//==============================================================================
ParamKnob::ParamKnob (HarshVoxProcessor& p, int paramIndex, int s, int li, KnobLook lk)
    : proc (p), index (paramIndex), slot (s), liveIdx (li), look (lk)
{
    setInterceptsMouseClicks (false, true);
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setRange (0.0, 1.0, 0.0);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    auto* prm = proc.param (index);
    slider.setDoubleClickReturnValue (true, prm->getDefaultValue());
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

Pt ParamKnob::centre() const
{
    const float r = look.r;
    const float top = look.seedDial ? r + 12.0f : (look.scale ? (look.numbers ? r + 19.0f : r + 10.0f) : r + 5.0f);
    return { (float) getWidth() * 0.5f, top };
}

void ParamKnob::placeAt (Pt c)
{
    const float r = look.r;
    const float half = look.numbers || look.seedDial ? r + 19.0f : r + 11.0f;
    const float w = std::max (2.0f * half, r < 13.0f ? 58.0f : 80.0f);
    const float top = look.seedDial ? r + 12.0f : (look.scale ? (look.numbers ? r + 19.0f : r + 10.0f) : r + 5.0f);
    const float labelY = r + (look.numbers ? 17.0f : 10.0f);
    const float tagY = labelY + (look.numbers ? 11.5f : 10.0f);
    const float h = top + tagY + look.tagSize * 0.5f + 6.0f;
    setBounds (juce::Rectangle<float> (c.x - w * 0.5f, c.y - top, w, h).getSmallestIntegerContainer());
}

void ParamKnob::resized()
{
    const auto c = centre();
    const float side = 2.0f * (look.r + 5.0f);
    slider.setBounds (Rc (side, side).withCentre (c).getSmallestIntegerContainer());
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
    else if (look.seedDial) slider.setMouseDragSensitivity (2000);
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
        if (t != lastType) { lastType = t; configureSteps(); repaint(); }
    }
    if (! dragging)
    {
        syncing = true;
        slider.setValue (proc.param (index)->getValue(), juce::dontSendNotification);
        syncing = false;
    }
    float lv = -1.0f;
    bool live = false;
    if (slot >= 0 && liveIdx >= 0)
    {
        lv = proc.engine().slotShared (slot).liveNorm[(size_t) liveIdx].load (std::memory_order_relaxed);
        const float base = proc.param (index)->getValue();
        bool reactive = false;
        if (auto* sp = dynamic_cast<SlotParam*> (proc.param (index)))
            if (auto* spec = sp->spec()) reactive = spec->reactive && spec->continuous();
        const bool on = proc.paramReal (slotIndex (slot, SOn)) >= 0.5f;
        live = on && (reactive || std::abs (lv - base) > 0.003f);
    }
    const float v = (float) slider.getValue();
    const auto txt = proc.param (index)->getText (v, 24);
    if (std::abs (lv - liveValue) > 0.002f || live != showLive || std::abs (v - shownValue) > 1.0e-5f || txt != shownText)
    {
        liveValue = lv; showLive = live; shownValue = v; shownText = txt;
        repaint();
    }
}

void ParamKnob::paint (juce::Graphics& g)
{
    const auto c = centre();
    const float r = look.r;
    const float v = (float) slider.getValue();
    if (look.seedDial)
    {
        static const juce::StringArray digits { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
        mil::bigDial (g, c, r, v * 10.0f, digits);
    }
    else
    {
        if (look.scale) mil::knobScale (g, c, r, look.numbers);
        mil::bakelite (g, c, r, v);
    }
    if (showLive && liveValue >= 0.0f && ! look.seedDial)
    {
        const float a0 = juce::degreesToRadians (135.0f + 270.0f * v), a1 = juce::degreesToRadians (135.0f + 270.0f * liveValue);
        const float rr = r + 2.6f;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rr, rr, 0.0f, std::min (a0, a1) + juce::MathConstants<float>::halfPi,
                           std::max (a0, a1) + juce::MathConstants<float>::halfPi + 0.001f, true);
        g.setColour (palette::red.brighter (0.2f));
        g.strokePath (arc, juce::PathStrokeType (1.8f));
        g.fillEllipse (Rc (3.6f, 3.6f).withCentre ({ c.x + std::cos (a1) * rr, c.y + std::sin (a1) * rr }));
    }
    const float labelY = c.y + r + (look.numbers ? 17.0f : 10.0f);
    if (look.label)
        mil::stencil (g, currentName().toUpperCase(), c.x, labelY, look.labelSize, juce::Justification::centred, palette::ink, 1.0f, 0.05f);
    if (look.valueTag)
        mil::tag (g, { c.x, labelY + (look.numbers ? 11.5f : 10.0f) }, shownText, look.tagSize);
}

//==============================================================================
ToggleSwitch::ToggleSwitch (HarshVoxProcessor& p, int paramIndex, float s) : proc (&p), index (paramIndex), size (s)
{
    juce::StringArray ch;
    if (auto* sp = dynamic_cast<SlotParam*> (p.param (index)))
    {
        if (auto* spec = sp->spec(); spec != nullptr && spec->map == Map::Choice && spec->numChoices() == 2)
            ch = { spec->choices[1], spec->choices[0] };
    }
    else if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (p.param (index)); c != nullptr && c->choices.size() == 2)
        ch = { c->choices[1], c->choices[0] };
    if (ch.isEmpty()) ch = { "ON", "OFF" };
    upText = ch[0].toUpperCase();
    downText = ch[1].toUpperCase();
    sync();
}

ToggleSwitch::ToggleSwitch (float s) : size (s) {}

void ToggleSwitch::setTexts (const juce::String& u, const juce::String& d, const juce::String& n)
{
    upText = u; downText = d; name = n;
    repaint();
}

void ToggleSwitch::placeAt (Pt c)
{
    const bool texts = upText.isNotEmpty() || name.isNotEmpty();
    const float w = texts ? 76.0f : 26.0f * size + 6.0f;
    const float top = texts ? 33.0f * size : 23.0f * size;
    const float h = top + (name.isNotEmpty() ? 46.0f * size : (texts ? 33.0f * size : 12.0f * size));
    setBounds (Rc (c.x - w * 0.5f, c.y - top, w, h).getSmallestIntegerContainer());
}

void ToggleSwitch::setUp (bool u)
{
    if (u != up) { up = u; repaint(); }
}

void ToggleSwitch::sync()
{
    if (proc != nullptr) setUp (proc->param (index)->getValue() >= 0.5f);
}

void ToggleSwitch::mouseDown (const juce::MouseEvent&)
{
    const bool nu = ! up;
    if (proc != nullptr)
    {
        auto* p = proc->param (index);
        p->beginChangeGesture();
        p->setValueNotifyingHost (nu ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
    setUp (nu);
    if (onChange) onChange (nu);
}

void ToggleSwitch::paint (juce::Graphics& g)
{
    const bool texts = upText.isNotEmpty() || name.isNotEmpty();
    const Pt c ((float) getWidth() * 0.5f, texts ? 33.0f * size : 23.0f * size);
    mil::toggle (g, c, up, size);
    if (upText.isNotEmpty()) mil::stencil (g, upText, c.x, c.y - 27.0f * size, 7.4f, juce::Justification::centred, palette::ink, up ? 1.0f : 0.75f, 0.05f);
    if (downText.isNotEmpty()) mil::stencil (g, downText, c.x, c.y + 27.0f * size, 7.4f, juce::Justification::centred, palette::ink, up ? 0.75f : 1.0f, 0.05f);
    if (name.isNotEmpty()) mil::stencil (g, name, c.x, c.y + 39.0f * size, nameSize, juce::Justification::centred, palette::ink, 1.0f, 0.08f);
}

//==============================================================================
PushButton::PushButton (juce::Colour c, float radius, const juce::String& label, bool labelRight)
    : juce::Button (label), cap (c), r (radius), text (label), right (labelRight) {}

void PushButton::placeAt (Pt c)
{
    const float half = r + 6.0f;
    if (right)
    {
        const float tw = text.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (stencilFont (7.8f), text) + 10.0f;
        setBounds (Rc (c.x - half, c.y - half, 2 * half + tw, 2 * half).getSmallestIntegerContainer());
    }
    else
        setBounds (Rc (c.x - std::max (half, 30.0f), c.y - half, 2 * std::max (half, 30.0f), 2 * half + (text.isEmpty() ? 0.0f : 14.0f)).getSmallestIntegerContainer());
}

void PushButton::paintButton (juce::Graphics& g, bool, bool down)
{
    const Pt c = right ? Pt (r + 6.0f, (float) getHeight() * 0.5f) : Pt ((float) getWidth() * 0.5f, r + 6.0f);
    mil::pushButton (g, c, r, cap, down);
    if (text.isEmpty()) return;
    if (right) mil::stencil (g, text, c.x + r + 8.0f, c.y, 7.8f, juce::Justification::left, palette::ink, 1.0f, 0.06f);
    else mil::stencil (g, text, c.x, c.y + r + 11.0f, 7.6f, juce::Justification::centred, palette::ink, 1.0f, 0.06f);
}

//==============================================================================
void TagButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r.translated (0.8f, 1.0f), 2.0f);
    g.setColour (juce::Colour (0xff171714).brighter (down ? 0.25f : (over ? 0.12f : 0.0f)));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (juce::Colour (0x99bfbaa8));
    g.drawRoundedRectangle (r.reduced (1.5f), 1.5f, 0.6f);
    g.setFont (tagFont (textSize));
    g.setColour (textColour);
    g.drawFittedText (getButtonText(), r.reduced (4.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.75f);
}

//==============================================================================
void Lamp::placeAt (Pt c)
{
    const float half = r * 3.2f;
    setBounds (Rc (c.x - half, c.y - half, 2 * half, 2 * half + (text.isEmpty() ? 0.0f : 8.0f)).getSmallestIntegerContainer());
}

void Lamp::paint (juce::Graphics& g)
{
    const Pt c ((float) getWidth() * 0.5f, r * 3.2f);
    mil::jewel (g, c, r, col, lit);
    if (text.isNotEmpty()) mil::stencil (g, text, c.x, c.y + r + 9.0f, 7.4f, juce::Justification::centred);
}

//==============================================================================
void VuMeter::setArea (Rc r)
{
    area = r;
    setBounds (r.expanded (14.0f).getSmallestIntegerContainer());
}

void VuMeter::tick()
{
    auto take = [] (std::atomic<float>* a) { return a != nullptr ? a->exchange (0.0f, std::memory_order_relaxed) : 0.0f; };
    const float pk = std::max (take (srcA), take (srcB));
    const float target = juce::jlimit (-22.0f, 4.0f, juce::Decibels::gainToDecibels (pk, -60.0f) + 3.0f);
    const float prev = needle;
    needle += (target - needle) * (target > needle ? 0.35f : 0.10f);
    if (pk > 0.89f) hotCount = 9;
    else if (hotCount > 0) --hotCount;
    if (std::abs (needle - prev) > 0.02f) repaint();
}

void VuMeter::paint (juce::Graphics& g)
{
    mil::vuMeter (g, area.translated ((float) -getX(), (float) -getY()), needle, name);
}
} // namespace hv::ui
