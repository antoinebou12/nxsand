// Nintendo face layout vs SDL GameController (often Xbox-oriented labels).
// switch-sdl2 may report SDL 2.0.14+ without positional button enums — probe at compile time.
#pragma once
#include <SDL2/SDL.h>
#include <cstdlib>

namespace nx {
namespace switch_face {

inline bool swapFaceXY() {
    const char* e = SDL_getenv("NXSAND_SWITCH_SWAP_FACE_XY");
    if (!e) e = SDL_getenv("NXENGINE_SWITCH_SWAP_FACE_XY");
    return e && e[0] != '\0' && e[0] != '0';
}

// switch-sdl2 reports buttons using SDL's positional naming where BUTTON_A is the
// BOTTOM face button. On a Switch controller the bottom button is physically labeled
// "B" and the right button is "A". So:
//   Nintendo A (right, confirm) = SDL_CONTROLLER_BUTTON_B
//   Nintendo B (bottom, back)   = SDL_CONTROLLER_BUTTON_A
//   Nintendo X (top, ring)      = SDL_CONTROLLER_BUTTON_Y
//   Nintendo Y (left)           = SDL_CONTROLLER_BUTTON_X
// If a future SDL build does honor positional EAST/SOUTH for the Switch, that maps the
// same way (EAST = right = Switch A), so prefer those when available.
inline bool confirm(SDL_GameController* p) {
#if defined(__SWITCH__)
    // Confirm is the right face button (Nintendo A) -> SDL B.
    // Do NOT use EAST/SOUTH on Switch: different switch-sdl2 builds map them
    // inconsistently, which is the exact reason confirm/back were swapped.
    return SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_B);
#else
    return SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_A);
#endif
}

inline bool back(SDL_GameController* p) {
#if defined(__SWITCH__)
    // Back is the bottom face button (Nintendo B) -> SDL A.
    return SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_A);
#else
    return SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_B);
#endif
}

/// Open material ring (Nintendo X = top face button -> SDL Y).
inline bool ring(SDL_GameController* p) {
#if defined(__SWITCH__)
    if (swapFaceXY())
        return SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_X); // Nintendo Y (left)
    return SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_Y);     // Nintendo X (top)
#else
    (void)p;
    return false;
#endif
}

} // namespace switch_face
} // namespace nx
