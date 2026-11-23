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

inline const R panel { 14.0f, 6.0f, 1192.0f, 748.0f };
constexpr float CL = 34.0f, CR = 1186.0f, CW = CR - CL;

// header
inline const R dataPlate { 42.0f, 22.0f, 300.0f, 88.0f };
inline const R programWin { 445.0f, 32.0f, 330.0f, 62.0f };
inline const P prevBtn { 399.0f, 63.0f }, nextBtn { 821.0f, 63.0f }, storeBtn { 879.0f, 63.0f };
constexpr float logoCap = 20.0f, logoY = 34.0f;
inline const P powerLamp { CR - 186.0f, 96.0f }, limitLamp { CR - 96.0f, 96.0f };

// master section
inline const R masterPlate { CL, 124.0f, CW, 116.0f };
inline const R vuIn { 44.0f, 136.0f, 118.0f, 94.0f }, vuOut { CR - 128.0f, 136.0f, 118.0f, 94.0f };
constexpr float knobRowY = 172.0f, masterKnobR = 17.0f;
inline const float masterKnobX[] = { 208.0f, 288.0f, 368.0f, 556.0f, 636.0f };   // Input, Env Atk, Env Rel, Mix, Output
inline const P seedDial { 462.0f, 168.0f };
constexpr float seedR = 34.0f;
inline const P inputModeSw { 712.0f, 168.0f }, modViewSw { 780.0f, 168.0f };
inline const R latencyBox { 826.0f, 150.0f, 84.0f, 22.0f };

// chain
inline const R chainPlate { CL, 248.0f, CW, 204.0f };
constexpr float bayX0 = 42.0f, bayY = 260.0f, bayW = 136.0f, bayH = 188.0f, bayGap = 6.0f;
inline R bay (int i) { return { bayX0 + (float) i * (bayW + bayGap), bayY, bayW, bayH }; }
inline R moduleFace (R b) { return { b.getX() + 5.0f, b.getY() + 4.0f, b.getWidth() - 12.0f, b.getHeight() - 12.0f }; }
inline R blankPlate (R b) { return { b.getX() + 7.0f, b.getY() + 8.0f, b.getWidth() - 14.0f, b.getHeight() - 18.0f }; }

// selected-module section (also hosts the mod matrix)
inline const R detailPlate { CL, 466.0f, CW, 152.0f };
inline const P moduleDial { 118.0f, 550.0f };
constexpr float moduleDialR = 48.0f;
inline const P detailMix { 230.0f, 526.0f }, detailOut { 308.0f, 526.0f };
constexpr float detailBigR = 21.0f, detailKnobR = 14.0f;
inline const P detailPower { 212.0f, 597.0f }, captureBtn { 268.0f, 597.0f };
constexpr float dividerX = 358.0f;
constexpr float gridX0 = 404.0f, gridDX = 90.0f, gridRow0 = 500.0f, gridRow1 = 566.0f;
inline const R sendsArea { 872.0f, 482.0f, 170.0f, 126.0f };
inline const P classifier { CR - 65.0f, 484.0f };

// scopes
inline const R scopeL { 44.0f, 630.0f, 760.0f, 104.0f };
inline const R scopeR { 820.0f, 630.0f, CR - 10.0f - 820.0f, 104.0f };
inline R glass (R bezel) { return bezel.reduced (18.0f, 14.0f); }

// big knobs that hands wear the paint around (for the finish pass)
struct Ring { P c; float r; };
inline std::vector<Ring> wornKnobs()
{
    std::vector<Ring> v;
    for (float x : masterKnobX) v.push_back ({ { x, knobRowY }, masterKnobR });
    v.push_back ({ seedDial, seedR * 0.6f });
    v.push_back ({ detailMix, detailBigR });
    v.push_back ({ detailOut, detailBigR });
    return v;
}
} // namespace hv::ui::layout
