#include "perf_overlay.hpp"
#include "../gpu/render_pipeline.hpp"
#include "../gpu/font_atlas.hpp"
#include "../gpu/perf_stats.hpp"
#include "theme.hpp"
#include <algorithm>
#include <cstdio>

namespace nx {

void drawPerfOverlay(RenderPipeline& r, FontAtlas& font, const PerfStats& perf,
                     const DebugSettings& debug, int screenW, int screenH) {
    if (debug.profilerHud == ProfilerHud::Off) return;

    char line1[160];
    char line2[160];
    char line3[160];
    char line4[160];

    const double frameMs = perf.lastFrameMs > 0.0 ? perf.lastFrameMs : perf.frameMs;
    const double simMs = perf.lastFrameMs > 0.0 ? perf.lastSimMs : perf.simMs;
    const double paintMs = perf.lastFrameMs > 0.0 ? perf.lastPaintMs : perf.paintMs;
    const double worldMs = perf.lastFrameMs > 0.0 ? perf.lastWorldRenderMs : perf.worldRenderMs;
    const double uiMs = perf.lastFrameMs > 0.0 ? perf.lastUiMs : perf.uiMs;
    const double presentMs = perf.lastFrameMs > 0.0 ? perf.lastPresentMs : perf.presentMs;
    const double otherMs = perf.lastFrameMs > 0.0
                               ? perf.lastOtherMs
                               : std::max(0.0, frameMs - simMs - paintMs - worldMs - uiMs - presentMs);
    const int passes = perf.lastFrameMs > 0.0 ? perf.lastFragmentPasses : perf.fragmentPasses;
    const int cmds = perf.lastFrameMs > 0.0 ? perf.lastBrushCommandCount : perf.brushCommandCount;
    const int dirtyW = perf.lastFrameMs > 0.0 ? perf.lastPaintDirtyW : perf.paintDirtyW;
    const int dirtyH = perf.lastFrameMs > 0.0 ? perf.lastPaintDirtyH : perf.paintDirtyH;
    const bool paintHeld = perf.lastFrameMs > 0.0 ? perf.lastPaintHeld : perf.paintHeld;
    const bool eraseHeld = perf.lastFrameMs > 0.0 ? perf.lastEraseHeld : perf.eraseHeld;
    const int brushMaterial = perf.lastFrameMs > 0.0 ? perf.lastBrushMaterial : perf.brushMaterial;
    const int brushRadius = perf.lastFrameMs > 0.0 ? perf.lastBrushRadius : perf.brushRadius;
    const int activeMode = perf.lastFrameMs > 0.0 ? perf.lastActiveTileMode : perf.activeTileMode;
    const int activeCount = perf.lastFrameMs > 0.0 ? perf.lastActiveTileCount : perf.activeTileCount;
    const bool activeFallback = perf.lastFrameMs > 0.0 ? perf.lastActiveTileFallback
                                                       : perf.activeTileFallback;
    const char* activeLabel = activeMode == 0 ? "off" : activeMode == 1 ? "stable" : "fast";

    std::snprintf(line1, sizeof(line1),
                  "%.0f fps  frame %.1f  other %.1f  accounted %.1f",
                  perf.fps, frameMs, otherMs, simMs + paintMs + worldMs + uiMs + presentMs);

    std::snprintf(line2, sizeof(line2), "sim %.1f  paint %.1f  world %.1f  ui %.1f  present %.1f",
                  simMs, paintMs, worldMs, uiMs, presentMs);

    std::snprintf(line3, sizeof(line3), "grid %dx%d  substeps %d  passes %d  %s  tiles %s:%d%s",
                  perf.simW, perf.simH, perf.substeps, passes,
                  perf.presetLabel ? perf.presetLabel : "?", activeLabel, activeCount,
                  activeFallback ? " fallback" : "");

    std::snprintf(line4, sizeof(line4),
                  "paint %s erase %s  mat %d  r %d  cmds %d  dirty %dx%d",
                  paintHeld ? "on" : "off", eraseHeld ? "on" : "off",
                  brushMaterial, brushRadius, cmds, dirtyW, dirtyH);

    const float s = theme::uiScale(screenW, screenH);
    const float pad = 8.f * s;
    const float x0 = 18.f * s;
    const float y0 = 48.f * s;
    const float lineH = 16.f * s;
    const int lines = debug.profilerHud == ProfilerHud::Full ? 4 : 2;
    const float boxH = lineH * float(lines) + pad * 2.f;
    const float boxW = 620.f * s;

    r.drawSolidRect(x0, y0, boxW, boxH, 0.02f, 0.03f, 0.05f, 0.75f, screenW, screenH);
    font.drawText(r, x0 + 6.f * s, y0 + 4.f * s, 0.72f * s, line1, 0.75f, 0.88f, 0.95f, 1.f,
                  screenW, screenH);
    font.drawText(r, x0 + 6.f * s, y0 + 4.f * s + lineH, 0.68f * s, line2, 0.55f, 0.65f, 0.75f,
                  1.f, screenW, screenH);
    if (lines >= 3) {
        font.drawText(r, x0 + 6.f * s, y0 + 4.f * s + lineH * 2.f, 0.68f * s, line3, 0.55f,
                      0.65f, 0.75f, 1.f, screenW, screenH);
        font.drawText(r, x0 + 6.f * s, y0 + 4.f * s + lineH * 3.f, 0.68f * s, line4, 0.55f,
                      0.65f, 0.75f, 1.f, screenW, screenH);
    }
}

} // namespace nx
