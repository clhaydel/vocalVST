#pragma once
#include <juce_graphics/juce_graphics.h>

namespace hv::ui::palette
{
// Military field-set palette: khaki enamel, black bakelite, painted white lettering, signal red.
inline const juce::Colour paint    { 0xff99957a };   // panel enamel
inline const juce::Colour paintDk  { 0xff78745a };
inline const juce::Colour caseCol  { 0xff33342b };
inline const juce::Colour bakelite { 0xff121110 };
inline const juce::Colour ink      { 0xffede8d4 };   // painted white lettering
inline const juce::Colour tagText  { 0xffdcd6be };   // engraved value tags
inline const juce::Colour silver   { 0xffd6d1bf };
inline const juce::Colour red      { 0xffcc1f17 };
inline const juce::Colour amber    { 0xffff9a1a };
inline const juce::Colour green    { 0xff59ff73 };
inline const juce::Colour phosphor { 0xff73ff8c };
inline const juce::Colour black    { 0xff121110 };

// popup menus / dialogs (kept dark for legibility)
inline const juce::Colour menuBg   { 0xff23241e };
inline const juce::Colour menuEdge { 0xff4a4a3e };
inline const juce::Colour text     { 0xffe8e4dc };
inline const juce::Colour textDim  { 0xff9c998c };
inline const juce::Colour brand    { 0xffff4a2a };   // Static Ruin signal red (logo)
inline const juce::Colour accent   { 0xffd42e3e };
inline const juce::Colour bg1      { 0xff141416 };   // block-font wordmark backing
} // namespace hv::ui::palette

namespace hv::ui
{
/** Panel lettering: IBM Plex Sans Condensed Bold. */
juce::Font stencilFont (float height);
/** Value tags / readouts: IBM Plex Mono Bold. */
juce::Font tagFont (float height);
/** Popup menus and dialogs (Space Grotesk / DM Mono). */
juce::Font uiFont (float height);
juce::Font monoFont (float height);
} // namespace hv::ui
