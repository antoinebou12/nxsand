#include "game_settings.hpp"
#include "../sim/sim_grid_policy.hpp"
#include <algorithm>
#include <cstdlib>

namespace nx {

GameSettings defaultGameSettings() {
    GameSettings s{};
#if defined(__SWITCH__)
    // Handheld can't sustain Balanced (640x360) on the stock GLES driver. Start at the
    // fast preset and let dynamic resolution raise it only if there's headroom.
    s.performance.mode = PerfPreset::BatterySaver;
    s.performance.dynamicResolution = true;
    s.display.orientation = ScreenOrientation::Landscape;
    applyPerfPreset(s.performance, PerfPreset::BatterySaver, true);
    s.performance.activeTiles = ActiveTileMode::Off;
#endif
    return s;
}

const char* perfPresetLabel(PerfPreset p) {
    switch (p) {
        case PerfPreset::BatterySaver: return "Battery Saver";
        case PerfPreset::Balanced: return "Balanced";
        case PerfPreset::Quality: return "Quality";
        case PerfPreset::Manual: return "Manual";
    }
    return "Balanced";
}

const char* profilerHudLabel(ProfilerHud h) {
    switch (h) {
        case ProfilerHud::Off: return "Off";
        case ProfilerHud::Compact: return "Compact";
        case ProfilerHud::Full: return "Full";
    }
    return "Compact";
}

const char* menuChromeLabel(MenuChrome m) {
    switch (m) {
        case MenuChrome::Full: return "Full";
        case MenuChrome::Minimal: return "Minimal";
    }
    return "Full";
}

const char* screenOrientationLabel(ScreenOrientation o) {
#if defined(__SWITCH__)
    (void)o;
    return "Landscape only";
#else
    switch (o) {
        case ScreenOrientation::Auto: return "Auto";
        case ScreenOrientation::Landscape: return "Landscape";
        case ScreenOrientation::Portrait: return "Portrait";
    }
    return "Auto";
#endif
}

void applyPerfPreset(PerformanceSettings& perf, PerfPreset preset, bool onSwitch) {
    perf.mode = preset;
    switch (preset) {
        case PerfPreset::BatterySaver:
            perf.simWidth = 480;
            perf.simHeight = 270;
            perf.substeps = 2;
            break;
        case PerfPreset::Balanced:
            perf.simWidth = 640;
            perf.simHeight = 360;
            perf.substeps = 2;
            break;
        case PerfPreset::Quality:
            perf.simWidth = 720;
            perf.simHeight = 405;
            perf.substeps = 2;
            break;
        case PerfPreset::Manual:
            break;
    }
}

int effectiveSubsteps(const PerformanceSettings& perf, bool onSwitch) {
    (void)onSwitch;
    if (perf.substeps > 0) return std::clamp(perf.substeps, 1, 2);
    return 2;
}

std::pair<int, int> presetSimSize(PerfPreset preset, int screenW, int screenH, bool onSwitch) {
    (void)onSwitch;
    switch (preset) {
        case PerfPreset::BatterySaver: return {480, 270};
        case PerfPreset::Balanced: return {640, 360};
        case PerfPreset::Quality: return {720, 405};
        case PerfPreset::Manual: break;
    }
    return pickSimGridSize(screenW, screenH);
}

std::pair<int, int> resolveSimGridSize(int screenW, int screenH, const PerformanceSettings& perf) {
    const char* ew = std::getenv("NXSAND_SIM_W");
    const char* eh = std::getenv("NXSAND_SIM_H");
    if (!ew || !eh) {
        ew = std::getenv("NXENGINE_SIM_W");
        eh = std::getenv("NXENGINE_SIM_H");
    }
    if (ew && eh) {
        int w = std::atoi(ew);
        int h = std::atoi(eh);
        if (w >= 32 && h >= 32 && w <= screenW && h <= screenH) {
            return {w, h};
        }
    }

    if (perf.mode != PerfPreset::Manual) {
        auto sz = presetSimSize(perf.mode, screenW, screenH,
#if defined(__SWITCH__)
                                 true
#else
                                 false
#endif
        );
        if (sz.first <= screenW && sz.second <= screenH) return sz;
    }

    if (perf.simWidth > 0 && perf.simHeight > 0) {
        int w = std::min(perf.simWidth, screenW);
        int h = std::min(perf.simHeight, screenH);
        if (w >= 32 && h >= 32) return {w, h};
    }

    return pickSimGridSize(screenW, screenH);
}

} // namespace nx
