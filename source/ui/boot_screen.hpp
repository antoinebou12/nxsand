#pragma once
#include <cstddef>

namespace nx {

class FontAtlas;
class RenderPipeline;

void drawBootScreen(RenderPipeline& r, FontAtlas& font, int screenW, int screenH, float progress,
                    const char* status);

/// Progress-only overlay when the font atlas is unavailable (no text).
void drawCompileOverlay(RenderPipeline& r, int screenW, int screenH, float progress);

/// Copy status into dst (max len-1), truncating long compile stage strings for handheld UI.
void formatCompileStatus(char* dst, size_t len, const char* status);

} // namespace nx
