#pragma once
#include <juce_core/juce_core.h>
#include "dsp/ModuleInfo.h"

namespace hv
{
class HarshVoxProcessor;

int numFactoryPresets();
juce::String factoryPresetName (int index);
void applyFactoryPreset (HarshVoxProcessor& p, int index);

/** C++ source for a factory-preset entry reproducing the processor's current settings
    (paste into kFactory in Presets.cpp). Only values that differ from defaults are emitted. */
juce::String exportPresetCode (HarshVoxProcessor& p, const juce::String& name);

/** Fluent helper used by factory presets and tests. */
class PresetBuilder
{
public:
    explicit PresetBuilder (HarshVoxProcessor& proc);
    PresetBuilder& slot (int s, ModuleType t);
    PresetBuilder& set (const char* name, float real);
    PresetBuilder& mix (float pct);
    PresetBuilder& out (float db);
    PresetBuilder& on (bool enabled);
    /** Captured formant snapshot (Formant Freeze "Capture" target) for the current slot. */
    PresetBuilder& capture (std::initializer_list<float> freqs, std::initializer_list<float> bws);
    PresetBuilder& global (int globalParam, float real);
    /** Modulation slot m: source -> slot s parameter `paramName` ("Mix" for slot mix). */
    PresetBuilder& mod (int m, int source, int s, const char* paramName, float amount, int curve = 0, float thr = 0.0f, float smoothMs = 20.0f);

private:
    HarshVoxProcessor& p;
    int cur = 0;
    ModuleType curType = ModuleType::Empty;
};
} // namespace hv
