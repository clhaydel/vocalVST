// Static panel artwork: vector drawing + a photo-texture paint finish (CC0 masks, see resources/textures).
// Rendered once per physical scale / bay-occupancy and cached by the editor.
#pragma once
#include "MilDraw.h"
#include "Layout.h"

namespace hv::ui
{
class PanelArt
{
public:
    /** k = physical pixels per logical unit. Bit i of `occupied` = a module unit sits in bay i. */
    static juce::Image render (float k, uint8_t occupied);

private:
    struct ScrewSpot { juce::Point<float> c; float r, angle; bool rusty; };
    static std::vector<ScrewSpot> screws (uint8_t occupied);
    static void finish (juce::Image&, float k, uint8_t occupied, const std::vector<ScrewSpot>&);
    static void drawCrisp (juce::Graphics&, uint8_t occupied, const std::vector<ScrewSpot>&);
};
} // namespace hv::ui
