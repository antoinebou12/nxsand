#pragma once

namespace nx {

class FontAtlas;
class RenderPipeline;

void drawBootScreen(RenderPipeline& r, FontAtlas& font, int screenW, int screenH, float progress,
                    const char* status);

} // namespace nx
