#pragma once
#include "../sim/materials.hpp"

namespace nx {
class App;
class RenderPipeline;
struct PlayRegion;

void drawHudSolid(RenderPipeline& r, App& app, const PlayRegion& pr);

} // namespace nx
