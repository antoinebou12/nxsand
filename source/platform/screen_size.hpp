// Drawable size queries for SDL + OpenGL. On Switch, SDL_GL_GetDrawableSize can report
// portrait (720×1280) while the panel is landscape — orientation picks how to map pixels.
#pragma once
#include <SDL2/SDL.h>
#include <utility>
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

inline void queryDrawableSize(SDL_Window* win, int& w, int& h,
                              ScreenOrientation o = ScreenOrientation::Auto) {
    if (!win) {
        w = h = 0;
        return;
    }
    SDL_GL_GetDrawableSize(win, &w, &h);
    applyDrawableOrientation(w, h, o);
}

} // namespace nx
