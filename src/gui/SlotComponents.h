#pragma once
#include "Widgets.h"
#include "Layout.h"

namespace hv::ui
{
/** Pop up the module chooser for a slot. */
void showModuleMenu (HarshVoxProcessor& p, int slot, juce::Component* target, std::function<void()> after = {});
/** Three-letter engraving for the module selector dial. */
juce::String moduleAbbrev (int type);

/** One bay of the chain: a plug-in module unit (name plate, key knobs, power, mix) or a blanking plate. */
class BayUnit final : public juce::Component, public juce::DragAndDropTarget
{
public:
    BayUnit (HarshVoxProcessor& p, int slot);
    void tick();
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    std::function<void (int)> onSelect;
    bool selected = false;
    const int slot;

private:
    void rebuild();
    juce::Rectangle<float> face() const;
    juce::Rectangle<float> namePlate() const;
    HarshVoxProcessor& proc;
    std::vector<std::unique_ptr<ParamKnob>> keys;
    std::unique_ptr<ParamKnob> mixKnob;
    ToggleSwitch power;
    int type = -1;
    bool on = true, dragOver = false, dragStarted = false;
};

class ChainView final : public juce::Component, public juce::DragAndDropContainer
{
public:
    explicit ChainView (HarshVoxProcessor& p);
    void tick();
    void setSelected (int s);
    uint8_t occupancy() const;
    std::function<void (int)> onSelect;

private:
    HarshVoxProcessor& proc;
    std::vector<std::unique_ptr<BayUnit>> bays;
};

/** The selected module's full controls (main bay or splitter send), in the lower section of the panel. */
class SlotDetail final : public juce::Component
{
public:
    explicit SlotDetail (HarshVoxProcessor& p);
    void setSlot (int s);
    int  getSlot() const { return slot; }
    juce::String title() const;
    void tick();
    void paint (juce::Graphics&) override;
    std::function<void (int)> onSelectSlot;

private:
    class ModuleDial final : public juce::Component
    {
    public:
        explicit ModuleDial (SlotDetail& d) : owner (d) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        SlotDetail& owner;
    };
    void rebuild();
    bool showSends() const;
    juce::Point<float> L (juce::Point<float> p) const { return p - getPosition().toFloat(); }
    HarshVoxProcessor& proc;
    int slot = 0, type = -1;
    bool sendsShown = false;
    ModuleDial dial { *this };
    std::vector<std::unique_ptr<ParamKnob>> knobs;
    std::vector<std::unique_ptr<ToggleSwitch>> switches;
    std::unique_ptr<ParamKnob> mixKnob, outKnob;
    std::unique_ptr<ToggleSwitch> power;
    PushButton captureButton { palette::red, 8.0f, "CAPTURE", true };
    TagButton backButton;
    std::array<TagButton, 6> sendButtons;
    std::array<int, 6> sendTypes {};
};

class ModMatrixPanel final : public juce::Component
{
public:
    explicit ModMatrixPanel (HarshVoxProcessor& p);
    void tick();
    void paint (juce::Graphics&) override;

private:
    void showTargetMenu (int m);
    juce::String targetName (int m) const;
    HarshVoxProcessor& proc;
    struct Column
    {
        std::unique_ptr<ParamKnob> source, curve, amount, thresh, smooth;
        TagButton target;
        bool active = false;
    };
    std::array<Column, params::kNumMods> cols;
};
} // namespace hv::ui
