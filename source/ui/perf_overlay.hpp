#pragma once
#include "../game/game_settings.hpp"

namespace nx {

class RenderPipeline;
class FontAtlas;
struct PerfStats;
struct PlayRegion;

void drawPerfOverlay(RenderPipeline& r, FontAtlas& font, const PerfStats& perf,
                     const DebugSettings& debug, const PlayRegion& pr, bool paletteVisible,
                     int screenW, int screenH, float uiScale = 1.f);

} // namespace nx
