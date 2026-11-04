#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Engine.h"

namespace hv
{
/** Generic per-slot parameter whose name and display text follow the module currently in the slot. */
class SlotParam final : public juce::AudioParameterFloat
{
public:
    SlotParam (const params::ParamDef& def, int slot, int index, juce::AudioParameterChoice* typeParam);
    juce::String getName (int maxLen) const override;
    const ParamSpec* spec() const;
    int slot, index;

private:
    juce::AudioParameterChoice* type;
};

class HarshVoxProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    HarshVoxProcessor();
    ~HarshVoxProcessor() override;

    static constexpr const char* kName = "Harsh Vox";

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override { return kName; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- API for the editor / tools (message thread)
    juce::AudioProcessorValueTreeState apvts;
    Engine& engine() noexcept { return eng; }
    juce::RangedAudioParameter* param (int index) const { return plist[(size_t) index]; }
    float paramReal (int index) const { return plist[(size_t) index]->convertFrom0to1 (plist[(size_t) index]->getValue()); }
    void setParamReal (int index, float real);
    void setParamNorm (int index, float norm);

    int  slotType (int slot) const;
    /** Change a slot's module; parameters are reset to the new module's defaults. */
    void setSlotType (int slot, ModuleType type);
    /** Drag-to-reorder: move main slot `from` to position `to`, shifting the others. */
    void moveSlot (int from, int to);
    void copySlot (int from, int to);
    void clearSlot (int slot);
    void requestCapture (int slot) { eng.slotShared (slot).captureRequest.store (true); }

    /** Load the default ("init") state: all params at defaults, all slots empty. */
    void resetToDefaults();
    juce::String currentPresetName { "Init" };
    bool loadPresetFile (const juce::File& f);
    bool savePresetFile (const juce::File& f);
    static juce::File userPresetFolder();

    /** Recompute + report latency now (message thread, or headless tools). */
    void refreshLatency();

private:
    void timerCallback() override;
    struct SlotState { int type; float on, mix, out, p[kParamsPerSlot]; Formants captured; };
    SlotState readSlot (int s) const;
    void writeSlot (int s, const SlotState& st);
    juce::ValueTree capturesToTree() const;
    void capturesFromTree (const juce::ValueTree& t);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::vector<juce::RangedAudioParameter*> plist;
    Engine eng;
    int currentProgram = 0;
    std::array<int, params::kNumSlots> lastTypes {};
    int lastLatency = -1;
    bool prepared = false;
};
} // namespace hv
