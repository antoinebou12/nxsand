#include "hud.hpp"
#include "../game/app.hpp"
#include "../gpu/render_pipeline.hpp"
#include "layout.hpp"
#include "theme.hpp"
#include "menu_chrome.hpp"
#include "ui_copy.hpp"
#include "../sim/materials.hpp"
#include <cstdio>

namespace nx {

namespace {

static void drawPlayTopBar(RenderPipeline& r, FontAtlas& font, App& app, int W, int H, float s,
                           bool profilerOn) {
    char title[96];
    std::snprintf(title, sizeof(title), "%s  R%d  %s", theme::APP_TITLE, app.sim.brush_radius,
                  material_name(app.sim.brush_mat));

#if defined(__SWITCH__)
    const float hintScale = 0.82f * s;
    const float titleScale = 0.84f * s;
    const float topMargin = 8.f * s;
    const float profilerBand = profilerOn ? 16.f * s : 0.f;
    const float topH = playHudTopBarPx(W, H, profilerOn, app.settings.accessibility.uiScale);
#else
    const float hintScale = 0.90f * s;
    const float titleScale = 0.92f * s;
    const float topMargin = 12.f * s;
    const float profilerBand = profilerOn ? 18.f * s : 0.f;
    const float topH = playHudTopBarPx(W, H, profilerOn, app.settings.accessibility.uiScale);
#endif
    const float hintPadY = 6.f * s;
    const float hintLine = float(font.lineH) * hintScale;
    const float hintPillH = hintLine + hintPadY * 2.f;

    r.drawSolidRect(0, 0, float(W), topH, 0.04f, 0.055f, 0.085f, 0.62f, W, H);

    const float bandTop = topMargin;
    const float bandH = std::max(hintPillH, topH - profilerBand - bandTop);
    const float titleY = menuBaselineInBand(bandTop, bandH, font, titleScale);
    font.drawText(r, 14.f * s, titleY, titleScale, title, 0.88f, 0.94f, 1.0f, 0.92f, W, H);

    const char* hint = ui_copy::playHudHint();
    const float hintPadX = 14.f * s;
    const float hintW = font.textWidth(hint, hintScale);
    const float pillW = hintW + hintPadX * 2.f;
    const float pillCx = std::max(pillW * 0.5f + 10.f * s, float(W) - 10.f * s - pillW * 0.5f);
    const float pillTop = bandTop + (bandH - hintPillH) * 0.5f;
    const float hintPillY = pillTop + hintPadY + float(font.baseline) * hintScale;
    drawHintPill(r, font, pillCx, hintPillY, s, hint, W, H);
}

} // namespace

void drawHudSolid(RenderPipeline& r, App& app, const PlayRegion& pr) {
    (void)pr;
    const int W = app.screenW, H = app.screenH;
    float s = theme::uiScale(W, H, app.settings.accessibility.uiScale);
    const bool profilerOn = app.settings.debug.profilerHud != ProfilerHud::Off;
    drawPlayTopBar(r, app.font, app, W, H, s, profilerOn);
}

} // namespace nx
