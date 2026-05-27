#pragma once
#include "../ui/layout.hpp"

namespace nx {

class App;
class RenderPipeline;

void drawActiveTilesOverlay(RenderPipeline& r, const App& app, const PlayRegion& pr);

} // namespace nx
