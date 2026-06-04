// Play region math.
#pragma once
#include "../sim/sim_state.hpp"
#include "theme.hpp"
#include <algorithm>
#include <cmath>

namespace nx {

struct PlayRegion {
    int x, y;       // top-left of the scaled sandbox in screen pixels
    int w, h;       // scaled size
    int scaleX;     // integer cell -> screen scale
    int scaleY;
};

struct MenuListWindow {
    int visibleRows = 1;
    int firstRow = 0;
};

inline MenuListWindow computeMenuListWindow(int itemCount, int selectedIndex, float available,
                                            float rowH) {
    MenuListWindow w{};
    const int rows = std::max(1, itemCount);
    w.visibleRows = std::max(1, std::min(rows, int(available / std::max(1.f, rowH))));
    const int maxFirst = std::max(0, rows - w.visibleRows);
    const int followPad = std::max(1, w.visibleRows / 3);
    w.firstRow = std::clamp(selectedIndex - followPad, 0, maxFirst);
    if (selectedIndex >= w.firstRow + w.visibleRows) {
        w.firstRow = std::clamp(selectedIndex - w.visibleRows + 1, 0, maxFirst);
    }
    return w;
}

// Title + hint pill band (matches hud.cpp; uses default font line metrics).
inline float playHudMainBandPx(float s) {
#if defined(__SWITCH__)
    const float hintScale = 0.88f;
#else
    const float hintScale = 0.90f;
#endif
    const float titleScale = 0.92f;
    constexpr float kFontLineH = 22.f;
    const float hintPillH = kFontLineH * hintScale * s + 12.f * s;
    const float titleLine = kFontLineH * titleScale * s;
    return std::max(titleLine, hintPillH) + 8.f * s;
}

// Top HUD band height in pixels (matches hud.cpp + optional profiler second line).
inline float playHudTopBarPx(int screenW, int screenH, bool profilerVisible,
                             float uiScale = 1.f) {
    const float s = theme::uiScale(screenW, screenH) * uiScale;
    const float topMargin = 12.f * s;
#if defined(__SWITCH__)
    float top = 28.f * s;
#else
    float top = 46.f * s;
#endif
    top = std::max(top, topMargin + playHudMainBandPx(s));
    if (profilerVisible) top += 18.f * s;
    return top;
}

// HUD band reserved above the sand (desktop). Switch: zero insets — top HUD draws over the sim.
inline std::pair<int, int> playHudInsets(int screenW, int screenH, bool profilerVisible = false,
                                         float uiScale = 1.f) {
#if defined(__SWITCH__)
    (void)screenW;
    (void)screenH;
    (void)profilerVisible;
    (void)uiScale;
    return {0, 0};
#else
    const int top = int(playHudTopBarPx(screenW, screenH, profilerVisible, uiScale));
    return {top, 0};
#endif
}

// withChrome=false: play mode. withChrome=true: legacy top bar inset.
// topInset/bottomInset: fullscreen layout — sand uses the band between HUD chrome (see playHudInsets).
//
// Uses float scaling (not integer) so the sandbox actually fills the available
// area. Previously the integer floor produced scale=1 for a 640x360 sim on a
// 1280x720 screen (since the vertical band is only ~584px tall), leaving the
// sim as a small rectangle inside a mostly-black screen — that looked like
// "the simulation isn't rendering" on Switch.
inline PlayRegion getPlayRegion(int screenW, int screenH, int gridW, int gridH,
                                bool withChrome = false, int topInset = 0, int bottomInset = 0) {
    int topBar = 0;
    int band   = screenH;
    if (withChrome) {
        topBar = theme::getTopBarH(screenH);
        band   = theme::getSandBandH(screenW, screenH);
    } else if (topInset > 0 || bottomInset > 0) {
        topBar = topInset;
        band   = std::max(1, screenH - topInset - bottomInset);
    }
    if (gridW <= 0 || gridH <= 0) {
        return PlayRegion{0, topBar, std::max(1, screenW), std::max(1, band), 1, 1};
    }
    const float fsx = float(screenW) / float(gridW);
    const float fsy = float(band)    / float(gridH);
    const float fscale = std::min(fsx, fsy);
    const int w = std::max(1, int(std::floor(float(gridW) * fscale)));
    const int h = std::max(1, int(std::floor(float(gridH) * fscale)));
    // Report a 1+ integer "scale" for callers that use it for crisp pixel math
    // (brush cursor, overlays). The actual draw rect uses fscale so the sim
    // fills the screen even at non-integer ratios.
    const int reportScale = std::max(1, int(std::round(fscale)));
    return PlayRegion{
        (screenW - w) / 2,
        topBar + (band - h) / 2,
        w, h, reportScale, reportScale
    };
}

inline PlayRegion getPlayRegionForScene(int screenW, int screenH, int gridW, int gridH,
                                        bool fullscreenSim, bool withChrome = false,
                                        bool profilerVisible = false, float uiScale = 1.f) {
    int top = 0;
    int bottom = 0;
    if (fullscreenSim && !withChrome) {
        const auto insets = playHudInsets(screenW, screenH, profilerVisible, uiScale);
        top = insets.first;
        bottom = insets.second;
    }
    return getPlayRegion(screenW, screenH, gridW, gridH, withChrome, top, bottom);
}

// Map drawable pixel (matches glViewport) to simulation cell.
// Aligns with HUD brush ring: brush_y grows toward larger screen Y (downward).
inline bool windowPxToGridCell(int px, int py, const PlayRegion& play, int gridW, int gridH,
                               int& outGx, int& outGy) {
    if (px < play.x || py < play.y || px >= play.x + play.w || py >= play.y + play.h)
        return false;
    const int rx = px - play.x;
    const int ry = py - play.y;
    outGx = int(std::floor(float(rx) / float(play.w) * float(gridW)));
    outGy = int(std::floor(float(ry) / float(play.h) * float(gridH)));
    outGx = std::clamp(outGx, 0, gridW - 1);
    outGy = std::clamp(outGy, 0, gridH - 1);
    return true;
}

} // namespace nx
