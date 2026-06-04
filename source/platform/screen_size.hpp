// Drawable size queries for SDL + OpenGL. On Switch, SDL_GL_GetDrawableSize can report
// portrait (720×1280) while the panel is landscape. Layout uses swapped landscape pixels;
// switch-sdl2 presents the framebuffer in panel orientation, so UI quads are not manually rotated.
#pragma once
#include <SDL2/SDL.h>
#include <algorithm>
#include "../game/game_settings.hpp"

#if defined(__SWITCH__)
#include <algorithm>
#endif

namespace nx {

inline void applyDrawableOrientation(int& w, int& h, ScreenOrientation o) {
#if defined(__SWITCH__)
    if (w <= 0 || h <= 0) {
        w = 1280;
        h = 720;
        return;
    }
    switch (o) {
        case ScreenOrientation::Auto:
        case ScreenOrientation::Landscape:
        case ScreenOrientation::Portrait:
            if (w < h) std::swap(w, h);
            break;
    }
#else
    (void)o;
#endif
}

/// Native GL framebuffer size (for glViewport).
inline void queryGlFramebufferSize(SDL_Window* win, int& w, int& h) {
    if (!win) {
        w = h = 0;
        return;
    }
    SDL_GL_GetDrawableSize(win, &w, &h);
#if defined(__SWITCH__)
    if (w <= 0 || h <= 0) {
        w = 720;
        h = 1280;
    }
#else
    if (w <= 0 || h <= 0) {
        w = 1280;
        h = 720;
    }
#endif
}

/// Landscape layout size for UI, input, and sim grid policy (1280×720 on Switch handheld).
inline void queryDrawableSize(SDL_Window* win, int& w, int& h,
                              ScreenOrientation o = ScreenOrientation::Auto) {
    queryGlFramebufferSize(win, w, h);
    applyDrawableOrientation(w, h, o);
}

#if defined(__SWITCH__)
/// True when GL storage is portrait (720×1280) but UI layout is landscape (1280×720).
inline bool switchPortraitFramebuffer(int fbW, int fbH, int layoutW, int layoutH) {
    return fbW > 0 && fbH > 0 && layoutW > layoutH && fbW < fbH;
}

/// Map layout top-left pixel to GL framebuffer top-left pixel (90° CW for compositor).
inline void layoutPixelToFramebuffer(int lx, int ly, int fbW, int fbH, bool rotate, int& fx,
                                     int& fy) {
    if (!rotate) {
        fx = lx;
        fy = ly;
        return;
    }
    (void)fbH;
    fx = fbW - ly;
    fy = lx;
}

/// Layout rect (y down) -> OpenGL viewport (origin bottom-left).
inline void layoutRectToGlViewport(int lx, int ly, int lw, int lh, int fbW, int fbH, bool rotate,
                                   int& vx, int& vy, int& vw, int& vh) {
    if (lw <= 0 || lh <= 0) {
        vx = vy = vw = vh = 0;
        return;
    }
    if (!rotate) {
        vx = lx;
        vy = fbH - ly - lh;
        vw = lw;
        vh = lh;
        return;
    }
    vx = fbW - ly - lh;
    vy = fbH - lx - lw;
    vw = lh;
    vh = lw;
}
#endif

} // namespace nx
