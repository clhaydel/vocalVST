// Fixed logical layout of the panel (1220 x 760). The whole panel is scaled as one piece, so the baked
// background (PanelArt) and the live components share these coordinates.
#pragma once
#include <juce_graphics/juce_graphics.h>

namespace hv::ui::layout
{
using R = juce::Rectangle<float>;
using P = juce::Point<float>;

constexpr float kW = 1220.0f, kH = 760.0f;
constexpr float kMinScale = 900.0f / kW;      // 900 x 561
constexpr float kMaxScale = 2.0f;             // 2440 x 1520

// dark title bar across the top (name, program selector, icons)
constexpr float headerH = 37.0f;
inline const R header { 0.0f, 0.0f, kW, headerH };
inline const R headerProgram { 483.0f, 5.0f, 255.0f, 28.0f };
inline const float headerIconX[] = { 965.0f, 1001.0f, 1037.0f, 1128.0f, 1163.0f, 1196.0f };   // save, init, dice, limit bell, menu, power
constexpr float headerIconY = 18.5f;

// painted frame below the bar; the separate screwed plates sit on it
inline const R panel { 0.0f, headerH, kW, kH - headerH };
constexpr float CL = 34.0f, CR = 1186.0f, CW = CR - CL;

// top row
inline const R topLeftPlate { 12.0f, 41.0f, 217.0f, 106.0f };
inline const R topMidPlate { 233.0f, 41.0f, 813.0f, 106.0f };
inline const R topRightPlate { 1050.0f, 41.0f, 158.0f, 106.0f };
inline const R dataPlate { 242.0f, 49.0f, 335.0f, 88.0f };
inline const R programWin { 660.0f, 53.0f, 304.0f, 70.0f };          // LCD bezel
inline const P prevBtn { 620.0f, 80.0f }, nextBtn { 1009.0f, 80.0f };
inline const P storeBtn { 1093.0f, 80.0f }, powerLamp { 1158.0f, 80.0f };
constexpr float roundBtnR = 14.0f;

// master section
inline const R masterPlate { 12.0f, 151.0f, 1196.0f, 140.0f };
inline const R vuIn { 37.0f, 163.0f, 140.0f, 121.0f }, vuOut { 1047.0f, 163.0f, 140.0f, 121.0f };
constexpr float knobRowY = 208.0f, masterKnobR = 21.0f;
inline const float masterKnobX[] = { 236.0f, 318.0f, 397.0f, 583.0f, 665.0f };   // Input, Env Atk, Env Rel, Mix, Output
inline const P seedDial { 488.0f, 209.0f };
constexpr float seedR = 28.0f;
inline const P inputModeSw { 768.0f, 222.0f }, modViewSw { 842.0f, 222.0f };
inline const R latencyBox { 896.0f, 196.0f, 100.0f, 30.0f };

// chain
inline const R chainPlate { 12.0f, 295.0f, 1196.0f, 184.0f };
constexpr float bayX0 = 40.0f, bayY = 302.0f, bayW = 137.0f, bayH = 174.0f, bayGap = 7.3f;
inline R bay (int i) { return { bayX0 + (float) i * (bayW + bayGap), bayY, bayW, bayH }; }
inline R moduleFace (R b) { return { b.getX() + 3.0f, b.getY() + 2.0f, b.getWidth() - 9.0f, b.getHeight() - 9.0f }; }
inline R blankPlate (R b) { return { b.getX() + 3.0f, b.getY() + 2.0f, b.getWidth() - 9.0f, b.getHeight() - 9.0f }; }

// selected-module section: left plate (title, module selector, mix / output), right plate (parameters);
// the mod matrix spans both
inline const R detailLeftPlate { 12.0f, 483.0f, 314.0f, 151.0f };
inline const R detailRightPlate { 330.0f, 483.0f, 878.0f, 151.0f };
inline const R detailPlate { CL, 483.0f, CW, 151.0f };
inline const P detailTag { 37.0f, 500.0f };
inline const P moduleDial { 112.0f, 580.0f };
constexpr float moduleDialR = 38.0f;
inline const P detailMix { 214.0f, 560.0f }, detailOut { 284.0f, 560.0f };
constexpr float detailBigR = 17.0f, detailKnobR = 14.0f;
inline const P detailPower { 356.0f, 530.0f }, captureBtn { 356.0f, 590.0f };
constexpr float gridX0 = 412.0f, gridDX = 90.0f, gridRow0 = 520.0f, gridRow1 = 586.0f;
inline const R sendsArea { 872.0f, 500.0f, 170.0f, 126.0f };
inline const P classifier { 1128.0f, 505.0f };
inline const P vacantHint { 650.0f, 560.0f };

// scopes
inline const R scopePlate { 12.0f, 638.0f, 1196.0f, 122.0f };
inline const R scopeL { 39.0f, 644.0f, 769.0f, 90.0f };
inline const R scopeR { 821.0f, 644.0f, 366.0f, 90.0f };
inline R glass (R bezel) { return bezel.reduced (17.0f, 10.0f); }

inline const R plates[] = { topLeftPlate, topMidPlate, topRightPlate, masterPlate, chainPlate, detailLeftPlate, detailRightPlate, scopePlate };

// big knobs that hands wear the paint around (for the finish pass)
struct Ring { P c; float r; };
inline std::vector<Ring> wornKnobs()
{
    std::vector<Ring> v;
    for (float x : masterKnobX) v.push_back ({ { x, knobRowY }, masterKnobR + 4.0f });
    v.push_back ({ seedDial, seedR * 0.8f });
    v.push_back ({ moduleDial, moduleDialR * 0.7f });
    return v;
}
} // namespace hv::ui::layout
