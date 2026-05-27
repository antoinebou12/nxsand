#pragma once
#include "../sim/materials.hpp"

namespace nx {

class App;
class RenderPipeline;
class FontAtlas;

/// Map left-stick direction to a segment index (same angular layout as the wheel).
/// Returns -1 when the stick is inside `minStickLen` (no direction selected).
int materialWheelIndexFromStick(float normX, float normY, int segmentCount,
                               float minStickLen = 0.2f);

void drawMaterialWheel(RenderPipeline& r, FontAtlas& font, App& app);

} // namespace nx
