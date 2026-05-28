#include "menu_chrome.hpp"
#include "menu.hpp"
#include "theme.hpp"
#include "../gpu/font_atlas.hpp"
#include "../gpu/render_pipeline.hpp"
#include <algorithm>
#include <cstring>

namespace nx {

float menuMarkTextScale(float uiScale) {
#if defined(__SWITCH__)
    return 1.35f * uiScale;
#else
    return 1.86f * uiScale;
#endif
}

float menuCrumbTextScale(float uiScale) {
#if defined(__SWITCH__)
    return 0.62f * uiScale;
#else
    return 1.0f * uiScale;
#endif
}

float menuMarkBoxHeight(const FontAtlas& font, float uiScale) {
    const float textScale = menuMarkTextScale(uiScale);
    const float linePx = float(font.lineH) * textScale;
    const float padY = 10.f * uiScale;
    return linePx + padY * 2.f;
}

float menuMarkBandHeight(const FontAtlas& font, float uiScale) {
    return menuMarkBoxHeight(font, uiScale) + 8.f * uiScale;
}

float menuCrumbBandHeight(const FontAtlas& font, float uiScale) {
    const float textScale = menuCrumbTextScale(uiScale);
    const float linePx = float(font.lineH) * textScale;
    const float padY = 8.f * uiScale;
    return linePx + padY * 2.f;
}

float menuBaselineInBand(float boxTop, float bandH, const FontAtlas& font, float textScale) {
    const float linePx = float(font.lineH) * textScale;
    return boxTop + (bandH - linePx) * 0.5f;
}

void drawMenuChromeScrim(RenderPipeline& r, const MenuLayout& L, int W, int H, float topY,
                         float bottomY, float strength) {
    const float a = std::clamp(strength, 0.f, 1.f);
    const float padX = 28.f * L.s;
    const float x = L.panelX - padX;
    const float w = L.panelW + padX * 2.f;
    const float y = topY;
    const float h = std::max(8.f, bottomY - topY);
    r.drawSolidRect(x, y, w, h, 0.05f, 0.06f, 0.10f, 0.94f * a, W, H);
}

void drawMenuGlassPanel(RenderPipeline& r, const MenuLayout& L, int W, int H, float strength) {
    const float a = std::clamp(strength, 0.f, 1.f);
    const float inset = 4.f * L.s;
    const float x = L.panelX + inset;
    const float y = L.panelY + inset;
    const float w = L.panelW - inset * 2.f;
    const float h = L.panelH - inset * 2.f;
    r.drawSolidRect(x - 2.f * L.s, y - 2.f * L.s, w + 4.f * L.s, h + 4.f * L.s, 0.30f, 0.79f,
                    0.77f, 0.22f * a, W, H);
    r.drawSolidRect(x, y, w, h, 0.08f, 0.09f, 0.14f, 0.58f * a, W, H);
    r.drawSolidRect(x, y, w, h, 0.22f, 0.18f, 0.45f, 0.16f * a, W, H);
    r.drawSolidRect(x, y, w, h * 0.22f, 1.f, 1.f, 1.f, 0.06f * a, W, H);
    r.drawSolidRect(x, y + h - 2.f, w, 2.f, 0.30f, 0.92f, 0.77f, 0.28f * a, W, H);
}

void drawMenuHeaderRuleAt(RenderPipeline& r, const MenuLayout& L, float y, int W, int H) {
    r.drawSolidRect(L.panelX + 20.f * L.s, y, L.panelW - 40.f * L.s, 1.f, 0.30f, 0.79f, 0.77f,
                    0.40f, W, H);
}

void drawMenuHeaderRule(RenderPipeline& r, const MenuLayout& L, int W, int H) {
    drawMenuHeaderRuleAt(r, L, L.panelY - 12.f * L.s, W, H);
}

void drawMenuMark(RenderPipeline& r, FontAtlas& font, float cx, float markY, const MenuLayout& L,
                  int W, int H, float strength) {
    const float a = std::clamp(strength, 0.f, 1.f);
    const float textScale = menuMarkTextScale(L.s);
    const float tw = font.textWidth(theme::APP_MARK, textScale);
    const float linePx = float(font.lineH) * textScale;
    const float padX = 22.f * L.s;
    const float padY = 10.f * L.s;
    const float bw = tw + padX * 2.f;
    const float bh = linePx + padY * 2.f;
    const float bx = cx - bw * 0.5f;
    const float by = markY;
    const float textY = by + padY;
    r.drawSolidRect(bx, by, bw, bh, 0.05f, 0.06f, 0.10f, 0.72f * a, W, H);
    r.drawSolidRect(bx, by, bw, 1.f, 0.30f, 0.79f, 0.77f, 0.45f * a, W, H);
    font.drawTextCentered(r, cx, textY, textScale, theme::APP_MARK, 0.30f, 0.86f, 0.82f, 1.f, W,
                          H);
}

static float uiTextLineTopInBox(float boxY, float boxH, const FontAtlas& font, float scale) {
    const float linePx = float(font.lineH) * scale;
    return boxY + (boxH - linePx) * 0.5f;
}

static void fitMenuLabel(char* dst, size_t dstSize, const char* label, float rowW, float textScale) {
    if (!dst || dstSize == 0) return;
    if (!label) label = "";
    const int maxChars = std::max(8, int(rowW / std::max(1.f, textScale * 8.5f)));
    std::snprintf(dst, dstSize, "%s", label);
    const size_t len = std::strlen(dst);
    if (int(len) <= maxChars || maxChars >= int(dstSize) - 1) return;
    const int keep = std::max(5, maxChars - 3);
    dst[keep] = '.';
    dst[keep + 1] = '.';
    dst[keep + 2] = '.';
    dst[keep + 3] = '\0';
}

void drawMenuRow(RenderPipeline& r, FontAtlas& font, int W, int H, const MenuLayout& L, float y,
                 const char* label, bool selected) {
    const float padX = 28.f * L.s;
    const float x = L.panelX + padX;
    const float rowW = L.panelW - padX * 2.f;
    const float rowGap = 4.f * L.s;
    const float rowH = L.rowH - rowGap * 2.f;
    const float rowY = y + rowGap;
    const float a = selected ? 0.98f : 0.85f;
    r.drawSolidRect(x, rowY, rowW, rowH, selected ? 0.17f : 0.075f, selected ? 0.24f : 0.12f,
                    selected ? 0.32f : 0.18f, a, W, H);
    if (!selected) {
        r.drawSolidRect(x + 8.f * L.s, rowY + rowH - 1.f, rowW - 16.f * L.s, 1.f, 0.35f, 0.42f,
                        0.55f, 0.18f, W, H);
    }
    const float barW = 6.f * L.s;
    if (selected) {
        r.drawSolidRect(x, rowY, barW, rowH, 0.30f, 0.79f, 0.77f, 1.f, W, H);
    }
    char fitted[96];
#if defined(__SWITCH__)
    const float textScale = 1.24f * L.s;
#else
    const float textScale = 1.38f * L.s;
#endif
#if defined(__SWITCH__)
    const float labelPad = 12.f * L.s;
#else
    const float labelPad = 14.f * L.s;
#endif
    const float textX = x + barW + labelPad;
    const float textY = uiTextLineTopInBox(rowY, rowH, font, textScale);
    fitMenuLabel(fitted, sizeof(fitted), label, rowW - barW - labelPad - 8.f * L.s, textScale);
    float drawX = textX;
    if (fitted[0] == '<') drawX += 3.f * L.s;
    font.drawText(r, drawX, textY, textScale, fitted, 0.94f, 0.97f, 1.f, 1.f, W, H);
}

void drawHintPill(RenderPipeline& r, FontAtlas& font, float cx, float y, float s, const char* hint,
                  int W, int H, float strength) {
    if (!hint || !hint[0]) return;
    const float a = std::clamp(strength, 0.f, 1.f);
#if defined(__SWITCH__)
    const float scale = 0.88f * s;
#else
    const float scale = 0.90f * s;
#endif
    const float tw = font.textWidth(hint, scale);
    const float padX = 14.f * s;
    const float padY = 6.f * s;
    const float linePx = float(font.lineH) * scale;
    const float ascPx = float(font.baseline) * scale;
    const float bw = tw + padX * 2.f;
    const float bh = linePx + padY * 2.f;
    const float bx = cx - bw * 0.5f;
    const float by = y - padY - ascPx;
    const float textY = by + padY;
    r.drawSolidRect(bx, by, bw, bh, 0.05f, 0.06f, 0.10f, 0.88f * a, W, H);
    r.drawSolidRect(bx, by, bw, 1.f, 0.30f, 0.79f, 0.77f, 0.30f * a, W, H);
    font.drawTextCentered(r, cx, textY, scale, hint, 0.58f, 0.64f, 0.74f, 0.95f, W, H);
}

} // namespace nx
