#pragma once
#include "../sim/materials.hpp"
#include "../ui/layout.hpp"

namespace nx {

class App;
class RenderPipeline;

void drawBrushCursor(RenderPipeline& r, const App& app, const PlayRegion& pr);

} // namespace nx
