#pragma once
#include <juce_core/juce_core.h>
#include "dsp/ModuleInfo.h"

namespace hv
{
class HarshVoxProcessor;

int numFactoryPresets();
juce::String factoryPresetName (int index);
void applyFactoryPreset (HarshVoxProcessor& p, int index);

/** Fluent helper used by factory presets and tests. */
class PresetBuilder
{
public:
    explicit PresetBuilder (HarshVoxProcessor& proc);
    PresetBuilder& slot (int s, ModuleType t);
    PresetBuilder& set (const char* name, float real);
    PresetBuilder& mix (float pct);
    PresetBuilder& out (float db);
    PresetBuilder& global (int globalParam, float real);
    /** Modulation slot m: source -> slot s parameter `paramName` ("Mix" for slot mix). */
    PresetBuilder& mod (int m, int source, int s, const char* paramName, float amount, int curve = 0, float thr = 0.0f, float smoothMs = 20.0f);

private:
    HarshVoxProcessor& p;
    int cur = 0;
    ModuleType curType = ModuleType::Empty;
};
} // namespace hv
