#include "hud.hpp"
#include "../game/app.hpp"
#include "../gpu/render_pipeline.hpp"
#include "layout.hpp"
#include "theme.hpp"
#include "../sim/materials.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cmath>
#include <cstring>

namespace nx {

namespace {

struct PaletteLayout {
    float y0;
    float palH;
    float rowH;
    float rowSep;
    float r1y;
    float r2y;
    float r1SlotW;
    float r2SlotW;
    float r1StartX;
    float r2StartX;
    float chipH;
    float chipInsetY;
};

static PaletteLayout getPaletteLayout(int W, int H, float s) {
    PaletteLayout l{};
    l.palH = float(theme::getPaletteH(H));
    l.y0 = float(H) - l.palH;
    l.rowSep = 10.f * s;
    l.rowH = (l.palH - l.rowSep) * 0.5f;
    // Switch: keep palette chips away from the bezel / TV overscan band.
#if defined(__SWITCH__)
    float edgePad = 36.f * s;
#else
    float edgePad = 16.f * s;
#endif
    l.r1SlotW = (float(W) - edgePad * 2.f) / float(HUD_PALETTE_ROW1.size());
    l.r1StartX = edgePad;
    l.r1y = l.y0;
    float r2Total = float(W) * 0.68f;
    l.r2SlotW = r2Total / float(HUD_PALETTE_ROW2.size());
    l.r2StartX = (float(W) - l.r2SlotW * float(HUD_PALETTE_ROW2.size())) * 0.5f;
    l.r2y = l.y0 + l.rowH + l.rowSep;
    l.chipH = std::min(l.rowH - 14.f * s, 44.f * s);
    l.chipInsetY = std::max(6.f * s, (l.rowH - l.chipH) * 0.5f);
    return l;
}

static void unpackRgb(uint32_t c, float& r, float& g, float& b) {
    r = float(c & 0xff) / 255.f;
    g = float((c >> 8) & 0xff) / 255.f;
    b = float((c >> 16) & 0xff) / 255.f;
}

static void drawPaletteChip(RenderPipeline& r, FontAtlas& font, float x, float y, float w, float h,
                            Material m, const char* keyHint, bool selected, float s, int W, int H) {
    auto pal = build_palette();
    float rf, gf, bf;
    unpackRgb(pal[static_cast<size_t>(m)], rf, gf, bf);

    if (selected) {
        r.drawSolidRect(x - 3.f * s, y - 3.f * s, w + 6.f * s, h + 6.f * s,
                        0.95f, 0.95f, 0.98f, 0.9f, W, H);
    }
    r.drawSolidRect(x, y, w, h, rf * 0.28f, gf * 0.28f, bf * 0.28f, 0.94f, W, H);

    float sw = std::min(w * 0.35f, h * 0.55f);
    float sx = x + 8.f * s;
    float sy = y + (h - sw) * 0.5f;
    r.drawSolidRect(sx, sy, sw, sw, rf, gf, bf, 1.f, W, H);

    const char* label = material_name(m);
    float labelScale = 0.88f * s;
    if (std::strlen(label) > 6) labelScale = 0.72f * s;
    font.drawText(r, x + w * 0.38f, y + 5.f * s, labelScale, label,
                  selected ? 1.f : 0.85f, selected ? 1.f : 0.9f, selected ? 1.f : 0.95f, 1.f, W, H);
    if (keyHint && keyHint[0]) {
        font.drawText(r, x + w - 18.f * s, y + h - 14.f * s, 0.75f * s, keyHint,
                      0.45f, 0.52f, 0.60f, 0.9f, W, H);
    }
}

template <size_t N>
static void drawPaletteRow(RenderPipeline& r, FontAtlas& font, float rowY, float rowH,
                           float chipY, float chipH, float startX, float slotW, float padX,
                           Material brush, const std::array<HudSlot, N>& row, float s, int W,
                           int H) {
    (void)rowY;
    (void)rowH;
    for (size_t i = 0; i < row.size(); ++i) {
        float x = startX + float(i) * slotW + padX;
        float w = slotW - padX * 2.f;
        drawPaletteChip(r, font, x, chipY, w, chipH, row[i].mat, row[i].keyHint,
                        row[i].mat == brush, s, W, H);
    }
}

} // namespace

void drawHudSolid(RenderPipeline& r, App& app, const PlayRegion& pr) {
    const int W = app.screenW, H = app.screenH;
    float s = theme::uiScale(W, H);

    char title[96];
    std::snprintf(title, sizeof(title), "%s  R%d  %s", theme::APP_TITLE,
                  app.sim.brush_radius, material_name(app.sim.brush_mat));
    const float topH = app.sim.paletteHidden ? 34.f * s : 42.f * s;
    r.drawSolidRect(0, 0, float(W), topH, 0.04f, 0.055f, 0.085f, 0.62f, W, H);
    app.font.drawText(r, 14.f * s, 9.f * s, 0.92f * s, title,
                      0.88f, 0.94f, 1.0f, 0.92f, W, H);
#if defined(__SWITCH__)
    app.font.drawText(r, float(W) - 590.f * s, 9.f * s, 0.66f * s,
                      "X RING  Y SAVE  + MENU  A/ZR PAINT  B/ZL ERASE  L/R SIZE",
                      0.55f, 0.62f, 0.70f, 0.82f, W, H);
#else
    app.font.drawText(r, float(W) - 480.f * s, 9.f * s, 0.74f * s,
                      "Y RING   F5 SAVE   + MENU   ZR PAINT   ZL ERASE   [/] SIZE",
                      0.55f, 0.62f, 0.70f, 0.82f, W, H);
#endif

    if (!app.sim.paletteHidden) {
        PaletteLayout l = getPaletteLayout(W, H, s);
        r.drawSolidRect(0, l.y0, float(W), l.palH, 0.08f, 0.10f, 0.14f, 0.92f, W, H);
        float padX = 5.f * s;
        float r1ChipY = l.r1y + l.chipInsetY;
        float r2ChipY = l.r2y + l.chipInsetY;
        drawPaletteRow(r, app.font, l.r1y, l.rowH, r1ChipY, l.chipH, l.r1StartX, l.r1SlotW, padX,
                       app.sim.brush_mat, HUD_PALETTE_ROW1, s, W, H);
        drawPaletteRow(r, app.font, l.r2y, l.rowH, r2ChipY, l.chipH, l.r2StartX, l.r2SlotW, padX,
                       app.sim.brush_mat, HUD_PALETTE_ROW2, s, W, H);
    }

    const float cellW = float(pr.w) / float(std::max(1, app.sim.grid_w));
    const float cellH = float(pr.h) / float(std::max(1, app.sim.grid_h));
    const float bx = float(pr.x) + (float(app.sim.brush_x) + 0.5f) * cellW;
    const float by = float(pr.y) + (float(app.sim.brush_y) + 0.5f) * cellH;
    const float br = float(app.sim.brush_radius) * std::max(cellW, cellH) + 2.f;

    const bool active = app.input.painting || app.input.erasing;
    const float a = active ? (app.input.erasing ? 0.85f : 0.55f) : 0.22f;
    const float cr = app.input.erasing ? 0.88f : 1.f;
    const float cg = app.input.erasing ? 0.32f : 1.f;
    const float cb = app.input.erasing ? 0.32f : 1.f;

    r.drawSolidRect(bx - br, by - br, br * 2.f, 2.f, cr, cg, cb, a, W, H);
    r.drawSolidRect(bx - br, by + br - 2.f, br * 2.f, 2.f, cr, cg, cb, a, W, H);
    r.drawSolidRect(bx - br, by - br, 2.f, br * 2.f, cr, cg, cb, a, W, H);
    r.drawSolidRect(bx + br - 2.f, by - br, 2.f, br * 2.f, cr, cg, cb, a, W, H);
}

} // namespace nx
