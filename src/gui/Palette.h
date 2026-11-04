#pragma once
#include <juce_graphics/juce_graphics.h>

namespace hv::ui::palette
{
// Static Ruin house palette (dark, industrial) + Harsh Vox product accent
inline const juce::Colour bg0     { 0xff0e0e10 };   // outer
inline const juce::Colour bg1     { 0xff141416 };   // header / bars
inline const juce::Colour panel   { 0xff1c1c1f };
inline const juce::Colour panel2  { 0xff222226 };
inline const juce::Colour knob    { 0xff2a2a2e };
inline const juce::Colour edge    { 0xff38383d };
inline const juce::Colour line    { 0xff2a2a2d };
inline const juce::Colour text    { 0xffe8e4dc };   // bone white
inline const juce::Colour textDim { 0xff8a877f };
inline const juce::Colour textFaint { 0xff55534e };
inline const juce::Colour brand   { 0xffff4a2a };   // Static Ruin signal red
inline const juce::Colour accent  { 0xffd42e3e };   // Harsh Vox raw crimson
inline const juce::Colour amber   { 0xffd9932b };   // fricative
inline const juce::Colour live    { 0xffff6a3d };   // live / modulated value rings
} // namespace hv::ui::palette

namespace hv::ui
{
/** Embedded OFL fonts (Space Grotesk Medium for labels, DM Mono for values). */
juce::Font uiFont (float height);
juce::Font monoFont (float height);
} // namespace hv::ui
