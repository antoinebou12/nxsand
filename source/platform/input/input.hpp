#pragma once
#include <SDL2/SDL.h>
#include "../../game/game_settings.hpp"
#include "../../ui/layout.hpp"

namespace nx {

struct InputState {
    bool quitRequested = false;
    bool openMenu = false;
    bool menuConfirm = false;
    bool menuBack = false;
    bool menuUp = false;
    bool menuDown = false;
    bool menuLeft = false;
    bool menuRight = false;
    bool menuPointerActive = false;
    bool menuPointerConfirm = false;
    int menuPointerX = 0;
    int menuPointerY = 0;
    bool toggleMaterialRing = false;
    bool materialRingConfirm = false;
    bool materialRingCancel = false;
    bool clearSandbox = false;
    bool quickSave = false;
    bool dropper = false;

    /// Normalized left stick while the material ring is open (angular highlight).
    float ringStickX = 0.f;
    float ringStickY = 0.f;
    /// Desktop: hovered ring segment from mouse (-1 = none).
    int materialWheelHoverIndex = -1;

    int brushDx = 0;
    int brushDy = 0;
    int brushRadiusDelta = 0;
    bool painting = false;
    bool erasing = false;

    bool pointerSetsBrush = false;
    int pointerGx = 0;
    int pointerGy = 0;

    SDL_GameController* pad = nullptr;
};

void pollInput(InputState& in, bool materialWheelOpen, bool menuActive, SDL_Window* window = nullptr,
               const PlayRegion* play = nullptr, int gridW = 0, int gridH = 0,
               float cursorSpeed = 1.f, float deadzone = 0.18f, bool invertY = false,
               ScreenOrientation screenOrientation = ScreenOrientation::Auto,
               float accessibilityUiScale = 1.f);
void openFirstController(InputState& in);
void closeController(InputState& in);

} // namespace nx
