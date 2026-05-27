#pragma once
#include "../game/game_settings.hpp"

namespace nx {

class RenderPipeline;
class FontAtlas;
struct PerfStats;

void drawPerfOverlay(RenderPipeline& r, FontAtlas& font, const PerfStats& perf,
                     const DebugSettings& debug, int screenW, int screenH);

} // namespace nx
