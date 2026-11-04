#pragma once
#include "Widgets.h"

namespace hv::ui
{
class AnalysisDisplay final : public juce::Component
{
public:
    explicit AnalysisDisplay (HarshVoxProcessor& p);
    void tick (int selectedSlot);
    void paint (juce::Graphics&) override;

private:
    void paintTimeline (juce::Graphics&, juce::Rectangle<float>);
    void paintTract (juce::Graphics&, juce::Rectangle<float>);
    HarshVoxProcessor& proc;
    static constexpr int kLen = 2400;
    std::array<AnalysisTelemetry::Point, kLen> hist {};
    int head = 0, lastRead = 0;
    std::array<float, kEnvPoints> curve {};
    float targetF[kMaxFormants] {}, targetB[kMaxFormants] {};
    int targetSlot = -1;
    Formants live;
};
} // namespace hv::ui
