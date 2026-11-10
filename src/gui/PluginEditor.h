#pragma once
#include "SlotComponents.h"
#include "AnalysisDisplay.h"

namespace hv
{
class HarshVoxEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HarshVoxEditor (HarshVoxProcessor&);
    ~HarshVoxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectSlot (int s);
    void showPresetMenu();
    void stepPreset (int delta);
    void exportPreset();
    juce::Rectangle<int> headerArea() const { return getLocalBounds().removeFromTop (48); }

    HarshVoxProcessor& proc;
    ui::LookAndFeel laf;
    juce::TooltipWindow tooltips { this, 600 };

    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, modButton { "MOD" }, inputButton { "STEREO" };
    std::vector<std::unique_ptr<ui::ParamKnob>> globals;
    ui::Meter inMeter, outMeter;
    ui::ChainView chain;
    ui::SlotDetail detail;
    ui::ModMatrixPanel modPanel;
    ui::AnalysisDisplay analysis;
    std::unique_ptr<juce::FileChooser> chooser;
    int selected = 0;
    juce::String latencyText;
};
} // namespace hv
