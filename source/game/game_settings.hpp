#pragma once
#include <cstdint>
#include <utility>

namespace nx {

enum class PerfPreset : int {
    BatterySaver = 0,
    Balanced = 1,
    Quality = 2,
    Manual = 3,
};

enum class ProfilerHud : int {
    Off = 0,
    Compact = 1,
    Full = 2,
};

enum class VisualAo : int { Off = 0, Low = 1, High = 2 };
enum class VisualBloom : int { Off = 0, Low = 1 };
enum class RumbleLevel : int { Off = 0, Low = 1, Medium = 2, High = 3 };
enum class ActiveTileMode : int { Off = 0, Conservative = 1, Aggressive = 2 };

/// Main-menu presentation (Load/Save use the same chrome as the main menu).
enum class MenuChrome : int { Full = 0, Minimal = 1 };

/// Switch drawable sizes can be reported as portrait while the panel is landscape; Auto/Landscape
/// normalize to a wide framebuffer. Portrait locks a tall layout when supported.
enum class ScreenOrientation : int { Auto = 0, Landscape = 1, Portrait = 2 };

struct PerformanceSettings {
    PerfPreset mode = PerfPreset::Balanced;
    int targetFps = 60;
    int simWidth = 0;
    int simHeight = 0;
    int substeps = 2;
    bool dynamicResolution = false;
    ActiveTileMode activeTiles = ActiveTileMode::Off;
};

struct VisualSettings {
#if defined(__SWITCH__)
    int paletteMode = 1; // fast palette default on handheld
#else
    int paletteMode = 0;
#endif
    VisualAo ao = VisualAo::Low;
    VisualBloom bloom = VisualBloom::Off;
    bool flicker = true;
    bool grain = false;
    bool glowEnabled = false;
};

struct ControlSettings {
    float cursorSpeed = 1.0f;
    int brushRadius = 3;
    float deadzone = 0.18f;
    bool invertY = false;
    RumbleLevel rumble = RumbleLevel::Medium;
};

struct AccessibilitySettings {
    float uiScale = 1.0f;
    bool reduceFlashing = false;
    bool togglePaint = false;
};

struct DebugSettings {
    ProfilerHud profilerHud = ProfilerHud::Compact;
    bool showActiveTiles = false;
    bool showMaterialIds = false;
    int benchmarkScene = 0;
};

struct DisplaySettings {
    MenuChrome menuChrome = MenuChrome::Full;
    ScreenOrientation orientation = ScreenOrientation::Auto;
    /// When true, the sand viewport fills the drawable area between the top HUD and palette.
    bool fullscreenSim = true;
};

constexpr int CURRENT_SETTINGS_VERSION = 2;

struct GameSettings {
    int version = CURRENT_SETTINGS_VERSION;
    PerformanceSettings performance{};
    VisualSettings visuals{};
    ControlSettings controls{};
    AccessibilitySettings accessibility{};
    DisplaySettings display{};
    DebugSettings debug{};
};

GameSettings defaultGameSettings();
const char* perfPresetLabel(PerfPreset p);
const char* profilerHudLabel(ProfilerHud h);
const char* menuChromeLabel(MenuChrome m);
const char* screenOrientationLabel(ScreenOrientation o);

void applyPerfPreset(PerformanceSettings& perf, PerfPreset preset, bool onSwitch);
int effectiveSubsteps(const PerformanceSettings& perf, bool onSwitch);
std::pair<int, int> presetSimSize(PerfPreset preset, int screenW, int screenH, bool onSwitch);
std::pair<int, int> resolveSimGridSize(int screenW, int screenH, const PerformanceSettings& perf);

} // namespace nx
