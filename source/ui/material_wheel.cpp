#include "material_wheel.hpp"
#include "../game/app.hpp"
#include "../gpu/font_atlas.hpp"
#include "../gpu/render_pipeline.hpp"
#include "../sim/materials.hpp"
#include "theme.hpp"
#include <algorithm>
#include <cmath>

namespace nx {

namespace {

void drawDisk(RenderPipeline& r, float cx, float cy, float radius, float cr, float cg, float cb,
              float ca, int W, int H) {
    const int slices = 22;
    for (int i = -slices; i <= slices; ++i) {
        const float y = float(i) / float(slices) * radius;
        const float half = std::sqrt(std::max(0.f, radius * radius - y * y));
        r.drawSolidRect(cx - half, cy + y, half * 2.f, std::max(1.f, radius / float(slices)),
                        cr, cg, cb, ca, W, H);
    }
}

} // namespace

int materialWheelIndexFromStick(float normX, float normY, int segmentCount, float minStickLen) {
    if (segmentCount <= 0) return -1;
    const float len = std::hypot(normX, normY);
    if (len < minStickLen) return -1;
    constexpr float kTwoPi = 6.2831853f;
    const float sector = kTwoPi / float(segmentCount);
    float ang = std::atan2(-normY, normX);
    float u = ang + 1.5707963f;
    while (u < 0.f) u += kTwoPi;
    while (u >= kTwoPi) u -= kTwoPi;
    int i = static_cast<int>(std::floor(u / sector + 0.5f));
    i = (i % segmentCount + segmentCount) % segmentCount;
    return i;
}

void drawMaterialWheel(RenderPipeline& r, FontAtlas& font, App& app) {
    if (!app.menu.materialWheelOpen) return;

    const int W = app.screenW;
    const int H = app.screenH;
    const float s = theme::uiScale(W, H, app.settings.accessibility.uiScale);
    const float cx = float(W) * 0.5f;
    const float cy = float(H) * 0.42f;
    const float rad = 132.f * s;

    const int n = static_cast<int>(PICKER_MATERIALS.size());

    r.drawSolidRect(0.f, 0.f, float(W), float(H), 0.f, 0.f, 0.f, 0.45f, W, H);

    auto pal = build_palette();
    for (int i = 0; i < n; ++i) {
        const float a = (float(i) / float(n)) * 6.2831853f - 1.5707963f;
        const float x = cx + std::cos(a) * rad;
        const float y = cy + std::sin(a) * rad;
        const float chipR = (i == app.menu.materialWheelIndex ? 29.f : 24.f) * s;
        const Material m = PICKER_MATERIALS[static_cast<size_t>(i)];
        const uint32_t c = pal[static_cast<size_t>(m)];
        const float cr = float(c & 0xff) / 255.f;
        const float cg = float((c >> 8) & 0xff) / 255.f;
        const float cb = float((c >> 16) & 0xff) / 255.f;
        const bool sel = (i == app.menu.materialWheelIndex);
        if (sel) {
            drawDisk(r, x, y, chipR + 5.f * s, 0.86f, 0.95f, 1.0f, 0.90f, W, H);
        }
        drawDisk(r, x, y, chipR, cr, cg, cb, 0.96f, W, H);
        font.drawTextCentered(r, x, y + 3.f * s, 0.55f * s, material_short_name(m), 0.02f,
                              0.025f, 0.035f, 0.95f, W, H);
    }
    const Material selected = PICKER_MATERIALS[static_cast<size_t>(
        std::clamp(app.menu.materialWheelIndex, 0, std::max(0, n - 1)))];
    const uint32_t sc = pal[static_cast<size_t>(selected)];
    drawDisk(r, cx, cy, 46.f * s, 0.03f, 0.04f, 0.06f, 0.86f, W, H);
    drawDisk(r, cx, cy, 35.f * s, float(sc & 0xff) / 255.f,
             float((sc >> 8) & 0xff) / 255.f, float((sc >> 16) & 0xff) / 255.f, 0.96f,
             W, H);
    font.drawTextCentered(r, cx, cy + 5.f * s, 0.76f * s, material_name(selected), 0.02f,
                          0.025f, 0.035f, 1.f, W, H);
#if defined(__SWITCH__)
    font.drawTextCentered(r, cx, cy + rad + 28.f * s, 1.0f * s, "A select   B cancel   stick aim",
                          0.85f, 0.9f, 0.95f, 1.f, W, H);
#else
    font.drawTextCentered(r, cx, cy + rad + 28.f * s, 1.0f * s, "Enter select   Esc cancel   stick aim",
                          0.85f, 0.9f, 0.95f, 1.f, W, H);
#endif
}

} // namespace nx
