#pragma once
#include "Widgets.h"

namespace hv::ui
{
/** Pop up the module chooser for a slot. */
void showModuleMenu (HarshVoxProcessor& p, int slot, juce::Component* target, std::function<void()> after = {});

/** One card in the chain: header (number, module name, bypass), 3-4 key knobs, mix, level strip. */
class SlotCard final : public juce::Component, public juce::DragAndDropTarget
{
public:
    SlotCard (HarshVoxProcessor& p, int slot);
    void tick();
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
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
    juce::Rectangle<int> headerArea() const { return getLocalBounds().removeFromTop (26); }
    HarshVoxProcessor& proc;
    std::vector<std::unique_ptr<ParamKnob>> keys;
    std::unique_ptr<ParamKnob> mixKnob;
    PowerButton power;
    int type = -1;
    bool dragOver = false, dragStarted = false;
    float level = 0.0f;
};

class ChainView final : public juce::Component, public juce::DragAndDropContainer
{
public:
    explicit ChainView (HarshVoxProcessor& p);
    void tick();
    void resized() override;
    void paint (juce::Graphics&) override;
    void setSelected (int s);
    std::function<void (int)> onSelect;

private:
    std::vector<std::unique_ptr<SlotCard>> cards;
};

/** Expanded editor for one slot (main or splitter send): every parameter, plus module extras. */
class SlotDetail final : public juce::Component
{
public:
    explicit SlotDetail (HarshVoxProcessor& p);
    void setSlot (int s);
    int  getSlot() const { return slot; }
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void (int)> onSelectSlot;

private:
    void rebuild();
    bool showSends() const;
    HarshVoxProcessor& proc;
    int slot = 0, type = -1;
    std::vector<std::unique_ptr<ParamKnob>> knobs;
    std::unique_ptr<ParamKnob> mixKnob, outKnob;
    PowerButton power;
    juce::TextButton moduleButton, captureButton { "CAPTURE" }, backButton { "< SPLITTER" };
    std::array<juce::TextButton, 6> sendButtons;
    std::array<int, 6> sendTypes {};
};

class ModMatrixPanel final : public juce::Component
{
public:
    explicit ModMatrixPanel (HarshVoxProcessor& p);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuildTargets();
    HarshVoxProcessor& proc;
    struct Row
    {
        juce::ComboBox source, target, curve;
        std::unique_ptr<juce::ComboBoxParameterAttachment> srcAtt, curveAtt;
        std::unique_ptr<ParamKnob> amount, thresh, smooth;
    };
    std::array<Row, params::kNumMods> rows;
    std::array<int, params::kNumSlots> knownTypes {};
};
} // namespace hv::ui
