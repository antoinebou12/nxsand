#include "brush_cursor.hpp"
#include "../game/app.hpp"
#include "../gpu/render_pipeline.hpp"
#include "../sim/materials.hpp"
#include <algorithm>
#include <cmath>

namespace nx {

void drawBrushCursor(RenderPipeline& r, const App& app, const PlayRegion& pr) {
    const int gx = app.sim.brush_x;
    const int gy = app.sim.brush_y;
    const int rad = app.sim.brush_radius;
    if (pr.w <= 0 || pr.h <= 0 || app.sim.grid_w <= 0 || app.sim.grid_h <= 0) return;

    const float cellW = float(pr.w) / float(app.sim.grid_w);
    const float cellH = float(pr.h) / float(app.sim.grid_h);
    const float cx = float(pr.x) + (float(gx) + 0.5f) * cellW;
    const float cy = float(pr.y) + (float(gy) + 0.5f) * cellH;
    const float rr = float(rad) * std::max(cellW, cellH);

    auto pal = build_palette();
    const uint32_t c = pal[static_cast<size_t>(app.sim.brush_mat)];
    const float mr = float(c & 0xff) / 255.f;
    const float mg = float((c >> 8) & 0xff) / 255.f;
    const float mb = float((c >> 16) & 0xff) / 255.f;

    const bool active = app.input.painting || app.input.erasing;
    const float ring = app.input.erasing ? 0.95f : (active ? 0.82f : 0.35f);
    const float rg = app.input.erasing ? 0.25f : mr;
    const float gg = app.input.erasing ? 0.25f : mg;
    const float bg = app.input.erasing ? 0.25f : mb;

    const int W = app.screenW;
    const int H = app.screenH;
    if (active) {
        r.drawSolidRect(cx - rr, cy - rr, rr * 2.f, rr * 2.f, rg, gg, bg, 0.08f, W, H);
    }
    const int segments = 24;
    for (int i = 0; i < segments; ++i) {
        const float a0 = (float(i) / float(segments)) * 6.2831853f;
        const float a1 = (float(i + 1) / float(segments)) * 6.2831853f;
        const float x0 = cx + std::cos(a0) * rr;
        const float y0 = cy + std::sin(a0) * rr;
        const float x1 = cx + std::cos(a1) * rr;
        const float y1 = cy + std::sin(a1) * rr;
        const float mx = (x0 + x1) * 0.5f;
        const float my = (y0 + y1) * 0.5f;
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const float len = std::sqrt(dx * dx + dy * dy);
        r.drawSolidRect(mx - len * 0.5f, my - 1.5f, len + 1.f, 3.f, rg, gg, bg, ring, W, H);
    }
    const float dot = active ? 5.f : 3.f;
    r.drawSolidRect(cx - dot, cy - dot, dot * 2.f, dot * 2.f, mr, mg, mb, active ? 1.f : 0.9f, W, H);
}

} // namespace nx
