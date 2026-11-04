#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"
#include "BlockFont.h"
#include "plugin/PluginProcessor.h"

namespace hv::ui
{
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (12.0f); }
    juce::Font getPopupMenuFont() override { return uiFont (13.0f); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return uiFont (std::min (13.0f, (float) h * 0.55f)); }
};

/** Rotary knob bound to one processor parameter (by global index), with name/value text and an
    outer ring showing the live (input-reactive / modulated) value for slot parameters. */
class ParamKnob final : public juce::Component
{
public:
    ParamKnob (HarshVoxProcessor& p, int paramIndex, int slot = -1, int liveIndex = -1, bool compact = false);
    /** Horizontal value-bar style (for dense rows such as the mod matrix). */
    void setBarStyle (bool b);
    void sync();
    void resized() override;
    void paint (juce::Graphics&) override;
    void setLabel (const juce::String& s) { label = s; repaint(); }
    juce::String currentName() const;
    float liveValue = -1.0f;   // <0: no live ring
    bool  showLive = false;

private:
    class KnobSlider : public juce::Slider
    {
    public:
        ParamKnob* owner = nullptr;
        void paint (juce::Graphics& g) override;
    };
    void configureSteps();
    HarshVoxProcessor& proc;
    int index, slot, liveIdx;
    bool compact;
    KnobSlider slider;
    juce::String label;
    bool syncing = false, dragging = false;
    int steps = 0;
    int lastType = -1;
    bool bar = false;
};

/** Vertical peak meter fed from an atomic peak value (exchanged to 0 on read). */
class Meter final : public juce::Component
{
public:
    explicit Meter (std::atomic<float>* a, std::atomic<float>* b = nullptr) : srcA (a), srcB (b) {}
    void tick();
    void paint (juce::Graphics&) override;

private:
    std::atomic<float>* srcA;
    std::atomic<float>* srcB;
    float levelA = 0, levelB = 0, holdA = 0, holdB = 0;
    int holdCount = 0;
};

/** Small square on/off button drawn as a power toggle. */
class PowerButton final : public juce::ToggleButton
{
public:
    void paintButton (juce::Graphics&, bool over, bool down) override;
};
} // namespace hv::ui
