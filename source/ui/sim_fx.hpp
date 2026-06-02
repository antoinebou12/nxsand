#pragma once
#include "../gpu/render_pipeline.hpp"
#include "../gpu/sim_pipeline.hpp"
#include "layout.hpp"

namespace nx {

void resetSimExplosionFx();
void notifySimExplosionWatch(int x0, int y0, int x1, int y1);
void expandSimExplosionWatch(int gridW, int gridH, int marginCells);
void tickSimExplosionFx(SimPipeline& pipe, const PlayRegion& pr, int gridW, int gridH,
                        uint32_t simTick);
void drawSimExplosionFx(RenderPipeline& r, const PlayRegion& pr, int gridW, int gridH, int screenW,
                        int screenH);

} // namespace nx
