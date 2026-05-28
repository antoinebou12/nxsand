#include "save_overlay.hpp"
#include "../gpu/font_atlas.hpp"
#include "../gpu/render_pipeline.hpp"
#include "theme.hpp"
#include <cmath>

namespace nx {

void SaveOverlay::begin(const char* label) {
    label_ = label ? label : "Saving...";
    animTime_ = 0.f;
    showFrames_ = 0;
    active_ = true;
}

void SaveOverlay::end() {
    active_ = false;
    label_.clear();
    showFrames_ = 0;
    animTime_ = 0.f;
}

void SaveOverlay::tick(float dtSec) {
    if (!active_) return;
    animTime_ += dtSec;
    ++showFrames_;
}

bool SaveOverlay::active() const { return active_; }

bool SaveOverlay::readyForIo() const { return active_ && showFrames_ >= 1; }

void SaveOverlay::draw(RenderPipeline& r, FontAtlas& font, int screenW, int screenH,
                       float uiScale) {
#if defined(__SWITCH__)
    (void)r;
    (void)font;
    (void)screenW;
    (void)screenH;
    (void)uiScale;
    return;
#else
    if (!active_) return;
    const int W = screenW, H = screenH;
    float s = theme::uiScale(W, H, uiScale);
    float bw = float(W) * 0.48f;
    float bh = 64.f * s;
    float x = (float(W) - bw) * 0.5f;
    float y = (float(H) - bh) * 0.5f;
    r.drawSolidRect(x, y, bw, bh, 0.10f, 0.14f, 0.20f, 0.92f, W, H);
    r.drawSolidRect(x, y, bw, 2.f, 0.30f, 0.79f, 0.77f, 0.55f, W, H);

    const float textScale = 1.45f * s;
    const float textY = y + 14.f * s;
    font.drawText(r, x + 22.f * s, textY, textScale, label_, 0.94f, 0.97f, 1.f, 1.f, W, H);

    const float dotY = y + bh - 22.f * s;
    const float dotR = 4.f * s;
    const float dotGap = 14.f * s;
    const float dotCx = float(W) * 0.5f - dotGap;
    for (int i = 0; i < 3; ++i) {
        const float phase = animTime_ * 4.f + float(i) * 0.55f;
        const float alpha = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(phase));
        const float cx = dotCx + float(i) * dotGap;
        r.drawSolidRect(cx - dotR, dotY - dotR, dotR * 2.f, dotR * 2.f, 0.30f, 0.79f, 0.77f,
                        alpha, W, H);
    }
#endif
}

} // namespace nx
