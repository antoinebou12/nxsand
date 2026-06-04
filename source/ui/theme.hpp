// UI colors and reference dimensions.
#pragma once
#include <cstdint>

namespace nx::theme {

struct Color { uint8_t r, g, b, a; };
constexpr Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) { return {r,g,b,a}; }

// Palette from theme.ts (UI block).
constexpr Color BG_DEEP      = rgba(0x08, 0x0a, 0x10);
constexpr Color BG_PLAY      = rgba(0x10, 0x14, 0x20);
constexpr Color BG_HUD_TOP   = rgba(0x14, 0x18, 0x24);
constexpr Color BG_HUD_BOT   = rgba(0x10, 0x14, 0x1c);
constexpr Color BG_GLASS     = rgba(0x18, 0x20, 0x30, 0xc8);
constexpr Color ACCENT       = rgba(0x4d, 0xc9, 0xc4);     // teal
constexpr Color ACCENT_2     = rgba(0xf0, 0xb4, 0x3c);     // amber
constexpr Color TEXT         = rgba(0xe6, 0xee, 0xf4);
constexpr Color TEXT_MUTED   = rgba(0x95, 0xa0, 0xae);
constexpr Color ERASE_RED    = rgba(0xe0, 0x52, 0x52);

constexpr const char* APP_TITLE   = "NXSand";
constexpr const char* APP_MARK    = "NXSand";
constexpr const char* APP_VERSION = "0.0.2";

constexpr int BASE_SCREEN_W = 1280;
constexpr int BASE_SCREEN_H = 720;

inline int getTopBarH(int screenH)      { return screenH / 12; }                    // ~60 @ 720
inline int getPaletteH(int screenH)     { return screenH * 3 / 14; }                // 3-row HUD palette
inline int getSandBandH(int screenW, int screenH, bool paletteVisible) {
    (void)screenW;
    int chrome = getTopBarH(screenH) + (paletteVisible ? getPaletteH(screenH) : 0);
    return screenH - chrome;
}

inline float uiScale(int screenW, int screenH, float accessibilityScale = 1.f) {
    float sx = float(screenW) / float(BASE_SCREEN_W);
    float sy = float(screenH) / float(BASE_SCREEN_H);
    float s  = sx < sy ? sx : sy;
    if (s < 0.5f) s = 0.5f;
    if (s > 2.0f) s = 2.0f;
#if defined(__SWITCH__)
    // Handheld GL is often 720×1280; do not shrink HUD/menu below design scale.
    if (s < 1.0f) s = 1.0f;
#endif
    const float a = accessibilityScale < 0.75f ? 0.75f : (accessibilityScale > 1.5f ? 1.5f : accessibilityScale);
    return s * a;
}

} // namespace nx::theme
