#include "game_settings.hpp"
#include "../sim/sim_grid_policy.hpp"
#include <algorithm>
#include <cstdlib>

namespace nx {

GameSettings defaultGameSettings() {
    GameSettings s{};
#if defined(__SWITCH__)
    // Handheld can't sustain Balanced (640x360) on the stock GLES driver.
    s.performance.mode = PerfPreset::BatterySaver;
    s.display.orientation = ScreenOrientation::Landscape;
    applyPerfPreset(s.performance, PerfPreset::BatterySaver, true);
    s.performance.simBackend = SimBackend::Fragment;
    applyPerfPresetVisuals(s.visuals, PerfPreset::BatterySaver);
#else
    applyPerfPreset(s.performance, PerfPreset::Balanced, false);
    s.performance.simBackend = SimBackend::Fragment;
    applyPerfPresetVisuals(s.visuals, PerfPreset::Balanced);
#endif
    return s;
}

void applyPerfPresetVisuals(VisualSettings& vis, PerfPreset preset) {
    switch (preset) {
        case PerfPreset::BatterySaver:
            vis.ao = VisualAo::Off;
            vis.flicker = false;
            vis.bloom = VisualBloom::Off;
            break;
        case PerfPreset::Balanced:
            vis.ao = VisualAo::Low;
            vis.flicker = false;
            break;
        case PerfPreset::Quality:
            vis.ao = VisualAo::Low;
            vis.flicker = false;
            break;
        case PerfPreset::Manual:
            break;
    }
}

void applyPerfPresetPhysics(PhysicsParams& phys, PerfPreset preset) {
    switch (preset) {
        case PerfPreset::BatterySaver:
        case PerfPreset::Balanced:
        case PerfPreset::Quality:
            phys.water_levelRate = 0.12f;
            break;
        case PerfPreset::Manual:
            break;
    }
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

const char* upscaleFilterName(UpscaleFilter f) {
    switch (f) {
        case UpscaleFilter::Nearest: return "nearest";
        case UpscaleFilter::Tent: return "tent";
        case UpscaleFilter::Mitchell: return "mitchell";
        case UpscaleFilter::CatmullRom: return "catmullrom";
        case UpscaleFilter::Lanczos3: return "lanczos3";
        case UpscaleFilter::Count: break;
    }
    return "nearest";
}

const char* paletteModeLabel(int mode) {
    switch (mode) {
        case 0: return "Pretty";
        case 1: return "Fast";
        case 2: return "Classic";
        default: return "Pretty";
    }
}

const char* soundLevelLabel(SoundLevel level) {
    switch (level) {
        case SoundLevel::Off: return "Off";
        case SoundLevel::Low: return "Low";
        case SoundLevel::Medium: return "Medium";
        case SoundLevel::High: return "High";
    }
    return "Medium";
}

void applyPerfPreset(PerformanceSettings& perf, PerfPreset preset, bool onSwitch) {
    perf.mode = preset;
    switch (preset) {
        case PerfPreset::BatterySaver:
            perf.simWidth = 480;
            perf.simHeight = 270;
            perf.substeps = 1;
            break;
        case PerfPreset::Balanced:
            perf.simWidth = 640;
            perf.simHeight = 360;
            perf.substeps = 2;
            break;
        case PerfPreset::Quality:
            perf.simWidth = 720;
            perf.simHeight = 405;
            perf.substeps = 1;
            break;
        case PerfPreset::Manual:
            break;
    }
    (void)onSwitch;
}

int effectiveSubsteps(const PerformanceSettings& perf, bool onSwitch) {
    (void)onSwitch;
    int steps = perf.substeps > 0 ? std::clamp(perf.substeps, 1, 2) : 2;
    const int w = perf.simWidth > 0 ? perf.simWidth : 640;
    const int h = perf.simHeight > 0 ? perf.simHeight : 360;
    if (w * h > 640 * 360) steps = std::min(steps, 1);
    return steps;
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
        const auto tierCap = pickSimGridSize(screenW, screenH);
        int w = std::min({perf.simWidth, screenW, tierCap.first});
        int h = std::min({perf.simHeight, screenH, tierCap.second});
        if (w >= 32 && h >= 32) return {w, h};
    }

    if (perf.mode == PerfPreset::Manual) {
        return presetSimSize(PerfPreset::Balanced, screenW, screenH, false);
    }

    return pickSimGridSize(screenW, screenH);
}

std::pair<int, int> maxSimSizeForPreset(PerfPreset preset) {
    switch (preset) {
        case PerfPreset::BatterySaver: return {480, 270};
        case PerfPreset::Balanced: return {640, 360};
        case PerfPreset::Quality: return {720, 405};
        case PerfPreset::Manual: return {960, 540};
    }
    return {640, 360};
}

} // namespace nx
