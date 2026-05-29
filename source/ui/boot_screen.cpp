#include "boot_screen.hpp"
#include "../gpu/font_atlas.hpp"
#include "../gpu/render_pipeline.hpp"
#include "theme.hpp"
#include <algorithm>
#include <cmath>

namespace nx {

void drawBootScreen(RenderPipeline& r, FontAtlas& font, int screenW, int screenH, float progress,
                    const char* status) {
    const int W = screenW, H = screenH;
    const float s = theme::uiScale(W, H);
    const float p = std::clamp(progress, 0.f, 1.f);

    const int bands = 5;
    const float bh = float(H) / float(bands);
    for (int i = 0; i < bands; ++i) {
        const float t = float(i) / float(bands - 1);
        const float u = std::fabs(t - 0.5f) * 2.f;
        const float rv = 0.060f + u * 0.010f;
        const float gv = 0.052f + u * 0.008f;
        const float bv = 0.105f + u * 0.018f;
        r.drawSolidRect(0.f, bh * float(i), float(W), bh + 1.f, rv, gv, bv, 1.f, W, H);
    }

    const float titleScale = 1.6f * s;
    font.drawTextCentered(r, float(W) * 0.5f, float(H) * 0.38f, titleScale, theme::APP_TITLE,
                          0.30f, 0.86f, 0.82f, 1.f, W, H);

    const float statusScale = 0.95f * s;
    const char* line = (status && status[0]) ? status : "Loading...";
    font.drawTextCentered(r, float(W) * 0.5f, float(H) * 0.48f, statusScale, line, 0.72f, 0.78f,
                          0.88f, 1.f, W, H);

    const float barW = float(W) * 0.42f;
    const float barH = 10.f * s;
    const float barX = (float(W) - barW) * 0.5f;
    const float barY = float(H) * 0.56f;
    r.drawSolidRect(barX, barY, barW, barH, 0.08f, 0.10f, 0.14f, 0.92f, W, H);
    if (p > 0.001f) {
        r.drawSolidRect(barX, barY, barW * p, barH, 0.30f, 0.79f, 0.77f, 1.f, W, H);
    }

    const float dotY = barY + barH + 18.f * s;
    const float dotR = 4.f * s;
    const float dotGap = 14.f * s;
    const float dotCx = float(W) * 0.5f - dotGap;
    const float pulse = float(std::fmod(progress * 6.0, 3.0));
    for (int i = 0; i < 3; ++i) {
        const float phase = pulse + float(i) * 0.55f;
        const float alpha = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(phase));
        const float cx = dotCx + float(i) * dotGap;
        r.drawSolidRect(cx - dotR, dotY - dotR, dotR * 2.f, dotR * 2.f, 0.30f, 0.79f, 0.77f,
                        alpha, W, H);
    }
}

} // namespace nx
