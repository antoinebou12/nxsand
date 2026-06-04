#include "input.hpp"
#include "switch_face.hpp"
#include "../screen_size.hpp"
#include "ui/layout.hpp"
#include "ui/material_wheel.hpp"
#include "../../sim/materials.hpp"
#include <algorithm>
#include <cmath>
#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace nx {

#if defined(__SWITCH__)
namespace {

PadState g_nxPad{};
bool g_nxPadReady = false;

void ensureNxPad() {
    if (g_nxPadReady) return;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_nxPad);
    g_nxPadReady = true;
}

} // namespace
#endif

void openFirstController(InputState& in) {
#if defined(__SWITCH__)
    ensureNxPad();
#endif
    if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
        in.pad = SDL_GameControllerOpen(0);
    }
}

void closeController(InputState& in) {
    if (in.pad) {
        SDL_GameControllerClose(in.pad);
        in.pad = nullptr;
    }
}

static bool edge(bool down, bool& prev) {
    bool e = down && !prev;
    prev = down;
    return e;
}

namespace {

void logicalToDrawable(SDL_Window* win, int lx, int ly, int* px, int* py, ScreenOrientation o) {
    int winW = 0, winH = 0, dw = 0, dh = 0;
    SDL_GetWindowSize(win, &winW, &winH);
    queryDrawableSize(win, dw, dh, o);
    if (winW <= 0 || winH <= 0 || dw <= 0 || dh <= 0) {
        *px = lx;
        *py = ly;
        return;
    }
    *px = int(std::lround(float(lx) * float(dw) / float(winW)));
    *py = int(std::lround(float(ly) * float(dh) / float(winH)));
}

void normalizedToDrawable(SDL_Window* win, float nx, float ny, int* px, int* py,
                          ScreenOrientation o) {
    int dw = 0, dh = 0;
    queryDrawableSize(win, dw, dh, o);
    if (dw <= 0 || dh <= 0) {
        *px = *py = 0;
        return;
    }
    *px = int(std::floor(nx * float(std::max(1, dw - 1))));
    *py = int(std::floor(ny * float(std::max(1, dh - 1))));
}

} // namespace

void pollInput(InputState& in, bool materialWheelOpen, bool menuActive, SDL_Window* window,
               const PlayRegion* play, int gridW, int gridH, float cursorSpeed, float deadzone,
               bool invertY, ScreenOrientation screenOrientation, float accessibilityUiScale) {
    in.brushDx = in.brushDy = 0;
    in.menuConfirm = in.menuBack = in.menuUp = in.menuDown = false;
    in.menuLeft = in.menuRight = false;
    in.menuUpHeld = in.menuDownHeld = in.menuLeftHeld = in.menuRightHeld = false;
    in.menuPointerActive = false;
    in.menuPointerConfirm = false;
    in.toggleMaterialRing = false;
    in.materialRingConfirm = in.materialRingCancel = false;
    in.clearSandbox = false;
    in.quickSave = false;
    in.brushRadiusDelta = 0;
    in.painting = false;
    in.erasing = false;
    in.pointerSetsBrush = false;
    in.dropper = false;
    in.ringStickX = in.ringStickY = 0.f;
    in.materialWheelHoverIndex = -1;

    const int stickDead = std::max(4000, int(32767.f * deadzone));
    static bool prevFaceA = false, prevFaceB = false;
    static bool prevRingX = false, prevRingY = false;
    static bool prevLStick = false;
    static bool prevL = false, prevR = false;
    static bool prevStart = false;
    static bool prevSelect = false;
    static bool prevQuickSave = false;
    static bool prevRet = false, prevBk = false, prevKUp = false, prevKDn = false, prevKLeft = false,
                prevKRight = false;
#if !defined(__SWITCH__)
    static bool prevKw = false, prevKs = false, prevKa = false, prevKdKey = false;
    static bool prevH = false;
    static bool prevMouseL = false;
#endif
    static bool prevTab = false;
    static bool prevEsc = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) in.quitRequested = true;
#if !defined(__SWITCH__)
        else if (e.type == SDL_MOUSEWHEEL && !menuActive && !materialWheelOpen) {
            if (e.wheel.y > 0)
                in.brushRadiusDelta = 1;
            else if (e.wheel.y < 0)
                in.brushRadiusDelta = -1;
        }
#endif
    }

#if defined(__SWITCH__)
    ensureNxPad();
    padUpdate(&g_nxPad);
    const u64 nxHeld = padGetButtons(&g_nxPad);
    const u64 nxDown = padGetButtonsDown(&g_nxPad);
    const HidAnalogStickState nxLeftStick = padGetStickPos(&g_nxPad, 0);
    const bool nxA = (nxHeld & HidNpadButton_A) != 0;
    const bool nxB = (nxHeld & HidNpadButton_B) != 0;
    const bool nxL = (nxHeld & HidNpadButton_L) != 0;
    const bool nxR = (nxHeld & HidNpadButton_R) != 0;
    const bool nxZL = (nxHeld & HidNpadButton_ZL) != 0;
    const bool nxZR = (nxHeld & HidNpadButton_ZR) != 0;
    const bool nxUp = (nxHeld & HidNpadButton_Up) != 0;
    const bool nxDownBtn = (nxHeld & HidNpadButton_Down) != 0;
    const bool nxLeft = (nxHeld & HidNpadButton_Left) != 0;
    const bool nxRight = (nxHeld & HidNpadButton_Right) != 0;
#endif

    const Uint8* kb = SDL_GetKeyboardState(nullptr);
    bool esc = kb[SDL_SCANCODE_ESCAPE] != 0;
    if (esc && !prevEsc) {
        if (materialWheelOpen)
            in.materialRingCancel = true;
        else if (menuActive)
            in.menuBack = true;
#if defined(__SWITCH__)
        else
            in.quitRequested = true;
#else
        else
            in.openMenu = true;
#endif
    }
    prevEsc = esc;

    bool ret = kb[SDL_SCANCODE_RETURN] != 0;
    bool bk = kb[SDL_SCANCODE_BACKSPACE] != 0;
    bool ku = kb[SDL_SCANCODE_UP] != 0;
    bool kd = kb[SDL_SCANCODE_DOWN] != 0;
    bool kl = kb[SDL_SCANCODE_LEFT] != 0;
    bool kr = kb[SDL_SCANCODE_RIGHT] != 0;
    bool tab = kb[SDL_SCANCODE_TAB] != 0;
#if !defined(__SWITCH__)
    bool kw = kb[SDL_SCANCODE_W] != 0;
    bool ks = kb[SDL_SCANCODE_S] != 0;
    bool ka = kb[SDL_SCANCODE_A] != 0;
    bool kdKey = kb[SDL_SCANCODE_D] != 0;
    bool hKey = kb[SDL_SCANCODE_H] != 0;
#endif

    if (materialWheelOpen && !menuActive) {
        in.menuConfirm = ret && !prevRet;
        in.menuBack = bk && !prevBk;
        in.menuUp = ku && !prevKUp;
        in.menuDown = kd && !prevKDn;
        in.menuLeft = kl && !prevKLeft;
        in.menuRight = kr && !prevKRight;
#if !defined(__SWITCH__)
        if (kw && !prevKw) in.menuUp = true;
        if (ks && !prevKs) in.menuDown = true;
        if (ka && !prevKa) in.menuLeft = true;
        if (kdKey && !prevKdKey) in.menuRight = true;
#endif
    }

    if (menuActive) {
        in.menuConfirm = ret && !prevRet;
        in.menuBack = bk && !prevBk;
        in.menuUpHeld = ku;
        in.menuDownHeld = kd;
        in.menuLeftHeld = kl;
        in.menuRightHeld = kr;
#if !defined(__SWITCH__)
        if (kw) in.menuUpHeld = true;
        if (ks) in.menuDownHeld = true;
        if (ka) in.menuLeftHeld = true;
        if (kdKey) in.menuRightHeld = true;
        if (window) {
            int lx = 0, ly = 0;
            const Uint32 mbtn = SDL_GetMouseState(&lx, &ly);
            const bool mouseL = (mbtn & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
            logicalToDrawable(window, lx, ly, &in.menuPointerX, &in.menuPointerY,
                              screenOrientation);
            in.menuPointerActive = true;
            if (edge(mouseL, prevMouseL)) in.menuPointerConfirm = true;
        }
#endif
#if defined(__SWITCH__)
        if (window) {
            static bool prevMenuTouchDown = false;
            bool touchDown = false;
            for (int ti = 0; ti < SDL_GetNumTouchDevices(); ++ti) {
                const SDL_TouchID tid = SDL_GetTouchDevice(ti);
                if (!tid || SDL_GetNumTouchFingers(tid) <= 0) continue;
                const SDL_Finger* finger = SDL_GetTouchFinger(tid, 0);
                if (!finger) continue;
                normalizedToDrawable(window, finger->x, finger->y, &in.menuPointerX,
                                     &in.menuPointerY, screenOrientation);
                in.menuPointerActive = true;
                touchDown = true;
                break;
            }
            if (edge(touchDown, prevMenuTouchDown)) in.menuPointerConfirm = true;
            prevMenuTouchDown = touchDown;
        }
#endif
#if defined(__SWITCH__)
        if (nxDown & HidNpadButton_A) in.menuConfirm = true;
        if (nxDown & HidNpadButton_B) in.menuBack = true;
        if (nxUp) in.menuUpHeld = true;
        if (nxDownBtn) in.menuDownHeld = true;
        if (nxLeft) in.menuLeftHeld = true;
        if (nxRight) in.menuRightHeld = true;
        {
            const int menuStickDead = std::max(4000, int(32767.f * deadzone));
            if (nxLeftStick.x < -menuStickDead) in.menuLeftHeld = true;
            if (nxLeftStick.x > menuStickDead) in.menuRightHeld = true;
            if (nxLeftStick.y > menuStickDead) in.menuUpHeld = true;
            if (nxLeftStick.y < -menuStickDead) in.menuDownHeld = true;
        }
#endif
        if (in.pad) {
            const bool fa = switch_face::confirm(in.pad);
            const bool fb = switch_face::back(in.pad);
            bool up = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
            bool dn = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            bool lf = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            bool rt = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            const Sint16 ax = SDL_GameControllerGetAxis(in.pad, SDL_CONTROLLER_AXIS_LEFTX);
            const Sint16 ay = SDL_GameControllerGetAxis(in.pad, SDL_CONTROLLER_AXIS_LEFTY);
            const int dead = std::max(12000, int(32767.f * deadzone));
            const bool stickL = ax < -dead;
            const bool stickR = ax > dead;
            const bool stickU = ay < -dead;
            const bool stickD = ay > dead;
            if (edge(fa, prevFaceA)) in.menuConfirm = true;
            if (edge(fb, prevFaceB)) in.menuBack = true;
            if (up) in.menuUpHeld = true;
            if (dn) in.menuDownHeld = true;
            if (lf) in.menuLeftHeld = true;
            if (rt) in.menuRightHeld = true;
            if (stickL) in.menuLeftHeld = true;
            if (stickR) in.menuRightHeld = true;
            if (stickU) in.menuUpHeld = true;
            if (stickD) in.menuDownHeld = true;
            prevFaceA = fa;
            prevFaceB = fb;
        }
        prevRet = ret;
        prevBk = bk;
        prevKUp = ku;
        prevKDn = kd;
        prevKLeft = kl;
        prevKRight = kr;
#if !defined(__SWITCH__)
        prevKw = kw;
        prevKs = ks;
        prevKa = ka;
        prevKdKey = kdKey;
#endif
        prevTab = tab;
        return;
    }

    if (!menuActive && tab && !prevTab) in.toggleMaterialRing = true;

    if (!menuActive && materialWheelOpen && window) {
        static bool prevRingPointerDown = false;
        int px = 0, py = 0;
        bool pointerDown = false;
        bool havePointer = false;

        for (int ti = 0; ti < SDL_GetNumTouchDevices(); ++ti) {
            const SDL_TouchID tid = SDL_GetTouchDevice(ti);
            if (!tid || SDL_GetNumTouchFingers(tid) <= 0) continue;
            const SDL_Finger* finger = SDL_GetTouchFinger(tid, 0);
            if (!finger) continue;
            normalizedToDrawable(window, finger->x, finger->y, &px, &py, screenOrientation);
            havePointer = true;
            pointerDown = true;
            break;
        }
#if !defined(__SWITCH__)
        if (!havePointer) {
            int lx = 0, ly = 0;
            const Uint32 mbtn = SDL_GetMouseState(&lx, &ly);
            pointerDown = (mbtn & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
            logicalToDrawable(window, lx, ly, &px, &py, screenOrientation);
            havePointer = true;
        }
#endif
        if (havePointer) {
            int dw = 0, dh = 0;
            queryDrawableSize(window, dw, dh, screenOrientation);
            if (dw > 0 && dh > 0) {
                const MaterialWheelLayout wl =
                    materialWheelLayout(dw, dh, accessibilityUiScale, play);
                in.materialWheelHoverIndex = materialWheelIndexFromPointer(
                    float(px), float(py), wl, selectorMaterialCount());
            }
            if (edge(pointerDown, prevRingPointerDown) && in.materialWheelHoverIndex >= 0)
                in.materialRingConfirm = true;
        }
        prevRingPointerDown = pointerDown;
    }

#if !defined(__SWITCH__)
    if (!materialWheelOpen) {
        const int keyStep = std::max(1, int(std::lround(2.f * cursorSpeed)));
        if (kl || ka) in.brushDx -= keyStep;
        if (kr || kdKey) in.brushDx += keyStep;
        if (ku || kw) in.brushDy -= keyStep;
        if (kd || ks) in.brushDy += keyStep;
        if (kb[SDL_SCANCODE_SPACE]) in.painting = true;
        if (kb[SDL_SCANCODE_E] || kb[SDL_SCANCODE_LSHIFT] || kb[SDL_SCANCODE_RSHIFT])
            in.erasing = true;
        if (kb[SDL_SCANCODE_LEFTBRACKET]) in.brushRadiusDelta = -1;
        if (kb[SDL_SCANCODE_RIGHTBRACKET]) in.brushRadiusDelta = 1;
        if (edge(hKey, prevH)) in.toggleMaterialRing = true;
        if (kb[SDL_SCANCODE_MINUS]) in.clearSandbox = true;
        const bool quickKey = kb[SDL_SCANCODE_F5] != 0;
        if (edge(quickKey, prevQuickSave)) in.quickSave = true;
    }
#else
    if (!materialWheelOpen) {
        if (kb[SDL_SCANCODE_LEFT] || kb[SDL_SCANCODE_A]) in.brushDx -= 2;
        if (kb[SDL_SCANCODE_RIGHT] || kb[SDL_SCANCODE_D]) in.brushDx += 2;
        if (kb[SDL_SCANCODE_UP] || kb[SDL_SCANCODE_W]) in.brushDy -= 2;
        if (kb[SDL_SCANCODE_DOWN] || kb[SDL_SCANCODE_S]) in.brushDy += 2;
        if (kb[SDL_SCANCODE_SPACE]) in.painting = true;
        if (kb[SDL_SCANCODE_E] || kb[SDL_SCANCODE_LSHIFT]) in.erasing = true;
        if (kb[SDL_SCANCODE_LEFTBRACKET]) in.brushRadiusDelta = -1;
        if (kb[SDL_SCANCODE_RIGHTBRACKET]) in.brushRadiusDelta = 1;
        if (kb[SDL_SCANCODE_H]) in.toggleMaterialRing = true;
        if (kb[SDL_SCANCODE_MINUS]) in.clearSandbox = true;
        const bool quickKey = kb[SDL_SCANCODE_F5] != 0;
        if (edge(quickKey, prevQuickSave)) in.quickSave = true;
    }
#endif

#if defined(__SWITCH__)
    if (!menuActive && materialWheelOpen) {
        const float fx = std::clamp(float(nxLeftStick.x) / 32767.f, -1.f, 1.f);
        const float fy = std::clamp(float(nxLeftStick.y) / 32767.f, -1.f, 1.f);
        const float len = std::hypot(fx, fy);
        if (len > 0.2f) {
            in.ringStickX = fx;
            in.ringStickY = fy;
        }
        if (nxDown & HidNpadButton_A) in.materialRingConfirm = true;
        if (nxDown & HidNpadButton_B) in.materialRingCancel = true;
        if (nxDown & HidNpadButton_X) in.materialRingCancel = true;
        prevTab = tab;
        if (invertY) in.brushDy = -in.brushDy;
        return;
    }
#endif

    if (!menuActive && materialWheelOpen && in.pad) {
        const Sint16 ax = SDL_GameControllerGetAxis(in.pad, SDL_CONTROLLER_AXIS_LEFTX);
        const Sint16 ay = SDL_GameControllerGetAxis(in.pad, SDL_CONTROLLER_AXIS_LEFTY);
        const float fx = std::clamp(float(ax) / 32767.f, -1.f, 1.f);
        const float fy = std::clamp(float(ay) / 32767.f, -1.f, 1.f);
        const float len = std::hypot(fx, fy);
        if (len > 0.2f) {
            in.ringStickX = fx;
            in.ringStickY = fy;
        }
        const bool fa = switch_face::confirm(in.pad);
        const bool fb = switch_face::back(in.pad);
        if (edge(fa, prevFaceA)) in.materialRingConfirm = true;
        if (edge(fb, prevFaceB)) in.materialRingCancel = true;
        prevFaceA = fa;
        prevFaceB = fb;
        prevTab = tab;
        if (invertY) in.brushDy = -in.brushDy;
        return;
    }

#if defined(__SWITCH__)
    {
        const int stickStep = std::max(1, int(std::lround(2.f * cursorSpeed)));
        const int directDead = std::max(4000, int(32767.f * deadzone));
        if (nxLeftStick.x < -directDead) in.brushDx -= stickStep;
        if (nxLeftStick.x > directDead) in.brushDx += stickStep;
        if (nxLeftStick.y > directDead) in.brushDy -= stickStep;
        if (nxLeftStick.y < -directDead) in.brushDy += stickStep;

        if ((nxDown & HidNpadButton_Plus) != 0) in.openMenu = true;
        if ((nxDown & HidNpadButton_Minus) != 0) in.clearSandbox = true;
        if ((nxDown & HidNpadButton_X) != 0) in.toggleMaterialRing = true;
        const bool quickNxY = (nxDown & HidNpadButton_Y) != 0;
        if (edge(quickNxY, prevQuickSave)) in.quickSave = true;
        if ((nxDown & HidNpadButton_StickL) != 0) in.dropper = true;
        if ((nxDown & HidNpadButton_L) != 0) in.brushRadiusDelta = -1;
        if ((nxDown & HidNpadButton_R) != 0) in.brushRadiusDelta = 1;

        if (nxZR || nxA) in.painting = true;
        if (nxZL || nxB) in.erasing = true;

        if (!nxL && !nxR) {
            if (nxUp) in.brushDy -= 1;
            if (nxDownBtn) in.brushDy += 1;
            if (nxLeft) in.brushDx -= 1;
            if (nxRight) in.brushDx += 1;
        }
    }
#endif

    if (in.pad) {
        const bool fa = switch_face::confirm(in.pad);
        const bool fb = switch_face::back(in.pad);
#if defined(__SWITCH__)
        const bool ringX = switch_face::ring(in.pad);
        const bool ringY = false;
#else
        const bool ringX = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_X);
        const bool ringY = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_Y);
#endif
        bool l = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        bool r = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        bool zl = SDL_GameControllerGetAxis(in.pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000;
        bool zr = SDL_GameControllerGetAxis(in.pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000;
        bool st = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_START);
        bool sel = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_BACK);
        bool up = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
        bool dn = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        bool lf = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        bool rt = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

        Sint16 ax = SDL_GameControllerGetAxis(in.pad, SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 ay = SDL_GameControllerGetAxis(in.pad, SDL_CONTROLLER_AXIS_LEFTY);
        const int stickStep = std::max(1, int(std::lround(2.f * cursorSpeed)));
        if (ax < -stickDead) in.brushDx -= stickStep;
        if (ax > stickDead) in.brushDx += stickStep;
        if (ay < -stickDead) in.brushDy -= stickStep;
        if (ay > stickDead) in.brushDy += stickStep;

        if (edge(st, prevStart) && !sel) in.openMenu = true;
        if (edge(sel, prevSelect) && !st) in.clearSandbox = true;
        const bool quickPad = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_Y) != 0;
        if (edge(quickPad, prevQuickSave)) in.quickSave = true;
        if (zr || fa) in.painting = true;
        if (zl || fb) in.erasing = true;
        const bool lStick = SDL_GameControllerGetButton(in.pad, SDL_CONTROLLER_BUTTON_LEFTSTICK);
        if (edge(lStick, prevLStick)) in.dropper = true;
        prevLStick = lStick;
        if (edge(l, prevL) && !up && !dn && !lf && !rt && !fa) in.brushRadiusDelta = -1;
        if (edge(r, prevR) && !up && !dn && !lf && !rt && !fb) in.brushRadiusDelta = 1;
        if (!l && !r) {
            if (up) in.brushDy -= 1;
            if (dn) in.brushDy += 1;
            if (lf) in.brushDx -= 1;
            if (rt) in.brushDx += 1;
            if (edge(ringX, prevRingX)) in.toggleMaterialRing = true;
            if (edge(ringY, prevRingY)) in.toggleMaterialRing = true;
        }

        prevFaceA = fa;
        prevFaceB = fb;
        prevRingX = ringX;
        prevRingY = ringY;
        prevL = l;
        prevR = r;
        prevStart = st;
        prevSelect = sel;
    }

    if (!menuActive && !materialWheelOpen && window && play && gridW > 0 && gridH > 0) {
        bool ptrPaint = false;
        bool ptrErase = false;
        int gx = 0, gy = 0;
        bool mapped = false;

        SDL_TouchID devActive = 0;
        int nFingers = 0;
        for (int ti = 0; ti < SDL_GetNumTouchDevices(); ++ti) {
            SDL_TouchID tid = SDL_GetTouchDevice(ti);
            if (!tid) continue;
            int nf = SDL_GetNumTouchFingers(tid);
            if (nf > 0) {
                devActive = tid;
                nFingers = nf;
                break;
            }
        }

        if (nFingers > 0) {
            const SDL_Finger* finger = SDL_GetTouchFinger(devActive, 0);
            if (finger) {
                int px = 0, py = 0;
                normalizedToDrawable(window, finger->x, finger->y, &px, &py, screenOrientation);
                if (windowPxToGridCell(px, py, *play, gridW, gridH, gx, gy)) {
                    mapped = true;
                    ptrErase = (nFingers >= 2);
                    ptrPaint = !ptrErase;
                }
            }
        } else {
            int lx = 0, ly = 0;
            const Uint32 btn = SDL_GetMouseState(&lx, &ly);
            int px = 0, py = 0;
            logicalToDrawable(window, lx, ly, &px, &py, screenOrientation);
#if !defined(__SWITCH__)
            if (windowPxToGridCell(px, py, *play, gridW, gridH, gx, gy)) {
                mapped = true;
                const bool shiftDown = kb[SDL_SCANCODE_LSHIFT] || kb[SDL_SCANCODE_RSHIFT];
                if ((btn & SDL_BUTTON(SDL_BUTTON_RIGHT))
                    || ((btn & SDL_BUTTON(SDL_BUTTON_LEFT)) && shiftDown))
                    ptrErase = true;
                else if (btn & SDL_BUTTON(SDL_BUTTON_LEFT))
                    ptrPaint = true;
            }
#else
            const bool shiftDown = kb[SDL_SCANCODE_LSHIFT] || kb[SDL_SCANCODE_RSHIFT];
            if ((btn & SDL_BUTTON(SDL_BUTTON_LEFT)) || (btn & SDL_BUTTON(SDL_BUTTON_RIGHT))) {
                if (windowPxToGridCell(px, py, *play, gridW, gridH, gx, gy)) {
                    mapped = true;
                    if ((btn & SDL_BUTTON(SDL_BUTTON_RIGHT))
                        || ((btn & SDL_BUTTON(SDL_BUTTON_LEFT)) && shiftDown))
                        ptrErase = true;
                    else if (btn & SDL_BUTTON(SDL_BUTTON_LEFT))
                        ptrPaint = true;
                }
            }
#endif
        }

        if (mapped) {
            in.pointerSetsBrush = true;
            in.pointerGx = gx;
            in.pointerGy = gy;
            if (ptrPaint) in.painting = true;
            if (ptrErase) in.erasing = true;
        }
    }

    prevRet = ret;
    prevBk = bk;
    prevKUp = ku;
    prevKDn = kd;
    prevKLeft = kl;
    prevKRight = kr;
#if !defined(__SWITCH__)
    prevKw = kw;
    prevKs = ks;
    prevKa = ka;
    prevKdKey = kdKey;
    prevH = hKey;
    if (!materialWheelOpen) {
        int lx = 0, ly = 0;
        const Uint32 mbtn = SDL_GetMouseState(&lx, &ly);
        prevMouseL = (mbtn & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    }
#endif
    prevTab = tab;

    if (invertY) in.brushDy = -in.brushDy;
}

} // namespace nx
