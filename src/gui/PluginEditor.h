#pragma once
#include "SlotComponents.h"
#include "AnalysisDisplay.h"
#include "PanelArt.h"

namespace hv
{
/** Military-hardware panel: a fixed 1220 x 760 layout scaled as one piece (fixed aspect ratio). */
class HarshVoxEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HarshVoxEditor (HarshVoxProcessor&);
    ~HarshVoxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void setModView (bool on);

private:
    class Panel;
    void timerCallback() override;
    void selectSlot (int s);
    void showPresetMenu (juce::Component* target);
    void stepPreset (int delta);
    void savePreset();
    void exportPreset();

    HarshVoxProcessor& proc;
    ui::LookAndFeel laf;
    std::unique_ptr<Panel> panel;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;
    int selected = 0;
};
} // namespace hv
