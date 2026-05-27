#pragma once

namespace nx {

class RenderPipeline;

void tickMenuBackgroundFx(int tick, int screenW, int screenH);
void drawMenuBackgroundFx(RenderPipeline& r, int screenW, int screenH, int tick);

} // namespace nx
