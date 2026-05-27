#include "toast.hpp"
#include "../game/app.hpp"
#include "../gpu/render_pipeline.hpp"
#include "../ui/theme.hpp"

namespace nx {

void drawToastSolid(RenderPipeline& r, App& app) {
    if (!app.toast.active()) return;
    const int W = app.screenW, H = app.screenH;
    float s = theme::uiScale(W, H);
    float bw = float(W) * 0.55f;
    float bh = 56.f * s;
    float x = (float(W) - bw) * 0.5f;
    float y = float(H) - bh - 40.f * s;
    r.drawSolidRect(x, y, bw, bh, 0.12f, 0.16f, 0.22f, 0.9f, W, H);
    app.font.drawText(r, x + 18.f * s, y + 17.f * s, 1.5f * s, app.toast.message,
                      0.95f, 0.95f, 0.98f, 1.f, W, H);
}

} // namespace nx
