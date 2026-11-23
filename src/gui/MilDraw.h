// Drawing primitives for the military-hardware panel (all in logical panel units).
#pragma once
#include "Palette.h"

namespace hv::ui::mil
{
using juce::Graphics;
using Pt = juce::Point<float>;
using Rc = juce::Rectangle<float>;

void softShadow (Graphics&, Rc r, float radius, int spread = 9, float alpha = 0.05f, float dx = 2.5f, float dy = 3.5f);
void aoHalo (Graphics&, Pt c, float r, float strength = 0.30f);
void screw (Graphics&, Pt c, float r, float angle);
void knobScale (Graphics&, Pt c, float r, bool numbers);
/** Black fluted bakelite knob. value01 sets the pointer (135 deg .. 405 deg, clockwise from +x). */
void bakelite (Graphics&, Pt c, float r, float value01, int flutes = 0, bool pointer = true);
void toggle (Graphics&, Pt c, bool up, float size = 1.0f);
void jewel (Graphics&, Pt c, float r, juce::Colour col, bool lit);
void pushButton (Graphics&, Pt c, float r, juce::Colour cap, bool down);
void thumbscrew (Graphics&, Pt c, float r = 4.6f);
void pullHandle (Graphics&, float x0, float x1, float y);

/** Painted lettering, vertically centred on y. */
void stencil (Graphics&, const juce::String& s, float x, float y, float size, juce::Justification j,
              juce::Colour col = palette::ink, float alpha = 1.0f, float tracking = 0.06f);
/** Black engraved value tag centred on c. */
void tag (Graphics&, Pt c, const juce::String& s, float size = 7.2f);
/** Section title on a black tag straddling a plate's top edge. */
void sectionTag (Graphics&, float x, float y, const juce::String& title);

void raisedPlate (Graphics&, Rc r);
void recess (Graphics&, Rc r);
void moduleBody (Graphics&, Rc face);
void blankingPlate (Graphics&, Rc plate);
/** Skirted dial with an engraved rim scale that rotates by `turn` (0..1 = one revolution). */
void bigDial (Graphics&, Pt c, float r, float turn, const juce::StringArray& labels, juce::Colour index = palette::red);
void vuMeter (Graphics&, Rc r, float vu, const juce::String& title);
void crtBezel (Graphics&, Rc r);
void case_ (Graphics&);
} // namespace hv::ui::mil
