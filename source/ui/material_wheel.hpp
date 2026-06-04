#pragma once
#include "../sim/materials.hpp"
#include "layout.hpp"
#include "theme.hpp"
#include <algorithm>
#include <cmath>

namespace nx {

class App;
class RenderPipeline;
class FontAtlas;

/// Map left-stick direction to a segment index (same angular layout as the wheel).
/// Returns -1 when the stick is inside `minStickLen` (no direction selected).
inline int materialWheelIndexFromStick(float normX, float normY, int segmentCount,
                                       float minStickLen = 0.2f) {
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

struct MaterialWheelLayout {
    float cx = 0.f;
    float cy = 0.f;
    float rad = 0.f;
    float minPickDist = 0.f;
};

inline MaterialWheelLayout materialWheelLayout(int screenW, int screenH, float accessibilityScale,
                                               const PlayRegion* play = nullptr) {
    const float s = theme::uiScale(screenW, screenH, accessibilityScale);
    MaterialWheelLayout L{};
    if (play && play->w > 8 && play->h > 8) {
        L.cx = float(play->x) + float(play->w) * 0.5f;
        L.cy = float(play->y) + float(play->h) * 0.5f;
        const float maxRad = std::min(float(play->w), float(play->h)) * 0.38f;
        L.rad = std::min(132.f * s, maxRad);
        L.minPickDist = std::max(40.f * s, L.rad * 0.32f);
    } else {
        L.cx = float(screenW) * 0.5f;
        L.cy = float(screenH) * 0.42f;
        L.rad = 132.f * s;
        L.minPickDist = 52.f * s;
    }
    return L;
}

/// Screen-pixel pick (drawable coords). Returns -1 when the pointer is off the ring.
inline int materialWheelIndexFromPointer(float px, float py, const MaterialWheelLayout& layout,
                                         int segmentCount) {
    const float dx = px - layout.cx;
    const float dy = py - layout.cy;
    const float len = std::hypot(dx, dy);
    if (len < layout.minPickDist || segmentCount <= 0) return -1;
    return materialWheelIndexFromStick(dx / len, -dy / len, segmentCount, 0.01f);
}

void drawMaterialWheel(RenderPipeline& r, FontAtlas& font, App& app, const PlayRegion& play);

} // namespace nx
