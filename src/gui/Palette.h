#pragma once
#include <juce_graphics/juce_graphics.h>

namespace hv::ui::palette
{
// Field-hardware palette: slate-blue enamel, cream knobs, white lettering, signal red.
inline const juce::Colour paint    { 0xff4e5868 };   // panel enamel (slate blue-grey)
inline const juce::Colour paintDk  { 0xff3c4554 };
inline const juce::Colour caseCol  { 0xff262e3b };
inline const juce::Colour bakelite { 0xff101113 };
inline const juce::Colour ink      { 0xffe9ebee };   // painted white lettering
inline const juce::Colour tagText  { 0xffe2e4e8 };   // value tags
inline const juce::Colour silver   { 0xffd6d8dc };
inline const juce::Colour red      { 0xffcc1f17 };
inline const juce::Colour amber    { 0xffffa51a };
inline const juce::Colour green    { 0xff4cd964 };
inline const juce::Colour phosphor { 0xff5cf08c };
inline const juce::Colour black    { 0xff111214 };
inline const juce::Colour header   { 0xff232323 };

// popup menus / dialogs (kept dark for legibility)
inline const juce::Colour menuBg   { 0xff1f232a };
inline const juce::Colour menuEdge { 0xff485263 };
inline const juce::Colour text     { 0xffe8eaee };
inline const juce::Colour textDim  { 0xff979dab };
inline const juce::Colour brand    { 0xffff4a2a };   // Static Ruin signal red (logo)
inline const juce::Colour accent   { 0xffd42e3e };
inline const juce::Colour bg1      { 0xff141416 };   // block-font wordmark backing
} // namespace hv::ui::palette

namespace hv::ui
{
/** Panel lettering: IBM Plex Sans Bold. */
juce::Font stencilFont (float height);
/** Nameplate title: IBM Plex Sans Condensed Bold. */
juce::Font condensedFont (float height);
/** Value tags / readouts: IBM Plex Mono Bold. */
juce::Font tagFont (float height);
/** Popup menus and dialogs (Space Grotesk / DM Mono). */
juce::Font uiFont (float height);
juce::Font monoFont (float height);
} // namespace hv::ui
