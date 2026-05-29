#include "ui_copy.hpp"
#include "theme.hpp"
#include <cstdio>

namespace nx::ui_copy {

void formatSubtitle(char* buf, size_t bufSize) {
    if (!buf || bufSize == 0) return;
#if defined(__SWITCH__)
    std::snprintf(buf, bufSize, "v%s", theme::APP_VERSION);
#else
    std::snprintf(buf, bufSize, "Falling sand sandbox · v%s", theme::APP_VERSION);
#endif
}

const char* menuNavHint(MenuScreen screen, bool hasEnteredPlay) {
    switch (screen) {
        case MenuScreen::Main:
#if defined(__SWITCH__)
            return hasEnteredPlay ? "D-pad / stick choose · A confirm · B resume"
                                  : "D-pad / stick choose · A confirm";
#else
            return hasEnteredPlay ? "WASD/Up-Down choose · Enter confirm · Esc return to sandbox"
                                  : "WASD/Up-Down choose · Enter confirm";
#endif
        case MenuScreen::SettingsEdit:
        case MenuScreen::EngineSettingsTab:
#if defined(__SWITCH__)
            return "D-pad / stick adjust · A confirm · B back";
#else
            return "WASD/Up-Down row · Left/Right adjust · Enter confirm · Esc back";
#endif
        default:
#if defined(__SWITCH__)
            return "D-pad / stick choose · A confirm · B back";
#else
            return "WASD/Up-Down choose · Enter confirm · Esc back";
#endif
    }
}

const char* playHudHint() {
#if defined(__SWITCH__)
    return "X ring · Y save · + menu · A/ZR paint · B/ZL erase · L/R size";
#else
    return "Esc menu · H ring · F5 save · WASD move · Space/LMB paint · Shift/RMB erase · [ ]/wheel size";
#endif
}

const char* materialRingHint() {
#if defined(__SWITCH__)
    return "A select · B cancel · stick aim";
#else
    return "Mouse or WASD/arrows choose · Enter/LMB select · Esc cancel";
#endif
}

} // namespace nx::ui_copy
