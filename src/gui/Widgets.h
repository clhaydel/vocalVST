#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"
#include "MilDraw.h"
#include "BlockFont.h"
#include "plugin/PluginProcessor.h"

namespace hv::ui
{
/** Popup menus, dialogs and tooltips (the panel itself is drawn by the components). */
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    juce::Font getPopupMenuFont() override { return uiFont (13.0f); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return uiFont (std::min (13.0f, (float) h * 0.55f)); }
    juce::Font getAlertWindowMessageFont() override { return uiFont (14.0f); }
    juce::Font getAlertWindowTitleFont() override { return uiFont (17.0f); }
    juce::Font getLabelFont (juce::Label&) override { return uiFont (14.0f); }
};

struct KnobLook
{
    float r = 14.0f;
    bool scale = true;          // painted tick scale
    bool numbers = false;       // 0..10 numerals on the scale
    bool label = true, valueTag = true;
    bool seedDial = false;      // knurled multi-turn dial (global Seed)
    float labelSize = 8.0f, tagSize = 7.2f;
};

/** Rotary control bound to one processor parameter (by global index). Black bakelite knob, painted scale,
    stencil label and an engraved value tag; slot parameters also show their live (reactive/modulated) value. */
class ParamKnob final : public juce::Component
{
public:
    ParamKnob (HarshVoxProcessor& p, int paramIndex, int slot = -1, int liveIndex = -1, KnobLook look = {});
    /** Position by the knob centre, in parent coordinates. */
    void placeAt (juce::Point<float> centre);
    void sync();
    void paint (juce::Graphics&) override;
    void resized() override;
    void setLabel (const juce::String& s) { label = s; repaint(); }
    juce::String currentName() const;

private:
    class KnobSlider : public juce::Slider
    {
    public:
        void paint (juce::Graphics&) override {}
    };
    void configureSteps();
    juce::Point<float> centre() const;
    float topSpace() const;
    float labelOffset() const;
    float tagOffset() const;
    HarshVoxProcessor& proc;
    int index, slot, liveIdx;
    KnobLook look;
    KnobSlider slider;
    juce::String label, shownText;
    bool syncing = false, dragging = false, showLive = false;
    int steps = 0, lastType = -1;
    float liveValue = -1.0f, shownValue = -1.0f;
};

/** Bat-handle toggle switch. Bound to a parameter (2-state) or driven by a callback. */
class ToggleSwitch final : public juce::Component, public juce::SettableTooltipClient
{
public:
    /** Parameter-bound: up = normalised value >= 0.5. */
    ToggleSwitch (HarshVoxProcessor& p, int paramIndex, float size = 1.0f);
    /** Free-standing (UI state). */
    explicit ToggleSwitch (float size = 1.0f);
    void setTexts (const juce::String& upText, const juce::String& downText, const juce::String& name);
    void placeAt (juce::Point<float> centre);
    void setUp (bool u);
    bool isUp() const { return up; }
    void sync();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void (bool)> onChange;
    float nameSize = 7.8f;
    bool nameOnTop = false;     // name above the up / down legends (master switches)

private:
    float topSpace() const;
    HarshVoxProcessor* proc = nullptr;
    int index = -1;
    float size;
    bool up = false;
    juce::String upText, downText, name;
};

/** Round push button (chrome bezel, coloured cap) with an optional painted label. */
class PushButton final : public juce::Button
{
public:
    PushButton (juce::Colour cap, float radius, const juce::String& label = {}, bool labelRight = false);
    void placeAt (juce::Point<float> centre);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    float labelGap = 11.0f, labelSize = 7.8f;
    int arrow = 0;              // -1 / +1: small triangle before / after the label

private:
    juce::Colour cap;
    float r;
    juce::String text;
    bool right;
};

/** Black engraved tag that acts as a button (sends, menus). */
class TagButton final : public juce::Button
{
public:
    TagButton() : juce::Button ({}) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    float textSize = 7.6f;
    juce::Colour textColour = palette::tagText;
};

/** Flat white icon in the title bar. */
class IconButton final : public juce::Button
{
public:
    enum class Kind { save, init, dice, bell, menu, power, up, down };
    explicit IconButton (Kind k) : juce::Button ({}), kind (k) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void setLit (bool l) { if (l != lit) { lit = l; repaint(); } }

private:
    Kind kind;
    bool lit = false;
};

/** Jewel indicator lamp. */
class Lamp final : public juce::Component
{
public:
    Lamp (juce::Colour c, float radius, const juce::String& label = {}) : col (c), r (radius), text (label) { setInterceptsMouseClicks (false, false); }
    void placeAt (juce::Point<float> centre);
    void setLit (bool l) { if (l != lit) { lit = l; repaint(); } }
    void paint (juce::Graphics&) override;
    float labelGap = 9.0f;

private:
    juce::Colour col;
    float r;
    juce::String text;
    bool lit = false;
};

/** Cream-faced VU meter fed from atomic peak values (exchanged to 0 on read). 0 dBFS = +3 VU. */
class VuMeter final : public juce::Component
{
public:
    VuMeter (std::atomic<float>* a, std::atomic<float>* b, const juce::String& title) : srcA (a), srcB (b), name (title) { setInterceptsMouseClicks (false, false); }
    void setArea (juce::Rectangle<float> r);
    void tick();
    bool hot() const { return hotCount > 0; }
    void paint (juce::Graphics&) override;

private:
    std::atomic<float>* srcA;
    std::atomic<float>* srcB;
    juce::String name;
    juce::Rectangle<float> area;
    float needle = -20.0f;
    int hotCount = 0;
};
} // namespace hv::ui
