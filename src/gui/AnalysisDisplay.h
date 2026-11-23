#pragma once
#include "Widgets.h"
#include "Layout.h"

namespace hv::ui
{
/** The two CRT scopes (envelope / pitch history, LPC tract vs. Formant Freeze target). Bezels are part of
    the baked panel; this component draws the phosphor and spans both glasses. */
class AnalysisDisplay final : public juce::Component
{
public:
    explicit AnalysisDisplay (HarshVoxProcessor& p);
    void tick (int selectedSlot);
    void paint (juce::Graphics&) override;
    const AnalysisTelemetry::Point& last() const { return hist[(size_t) ((head - 1 + kLen) % kLen)]; }

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

/** Voiced / fricative / plosive jewel lamps plus f0 and confidence readouts. */
class ClassifierPanel final : public juce::Component
{
public:
    ClassifierPanel() { setInterceptsMouseClicks (false, false); }
    void update (const AnalysisTelemetry::Point& p);
    void paint (juce::Graphics&) override;

private:
    bool v = false, f = false, pl = false;
    juce::String f0Text = "---", confText = "0.00";
};
} // namespace hv::ui
