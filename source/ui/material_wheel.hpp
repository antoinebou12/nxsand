#pragma once
#include "../sim/materials.hpp"

namespace nx {

class App;
class RenderPipeline;
class FontAtlas;
struct PlayRegion;

/// Map left-stick direction to a segment index (same angular layout as the wheel).
/// Returns -1 when the stick is inside `minStickLen` (no direction selected).
int materialWheelIndexFromStick(float normX, float normY, int segmentCount,
                               float minStickLen = 0.2f);

struct MaterialWheelLayout {
    float cx = 0.f;
    float cy = 0.f;
    float rad = 0.f;
    float minPickDist = 0.f;
};

MaterialWheelLayout materialWheelLayout(int screenW, int screenH, float accessibilityScale,
                                          const PlayRegion* play = nullptr);

/// Screen-pixel pick (drawable coords). Returns -1 when the pointer is off the ring.
int materialWheelIndexFromPointer(float px, float py, const MaterialWheelLayout& layout,
                                  int segmentCount);

void drawMaterialWheel(RenderPipeline& r, FontAtlas& font, App& app, const PlayRegion& play);

} // namespace nx
