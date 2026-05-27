#include "engine_settings.hpp"
#include "app.hpp"
#include "benchmark_scene.hpp"
#include <SDL2/SDL.h>
#include "../save/settings_io.hpp"
#include "../sim/sim_state.hpp"
#include <cstdio>
#include <algorithm>

namespace nx {

const char* engineTabLabel(EngineTab tab) {
    switch (tab) {
        case EngineTab::Performance: return "Performance";
        case EngineTab::Visuals: return "Visuals";
        case EngineTab::Controls: return "Controls";
        case EngineTab::Accessibility: return "Accessibility";
        case EngineTab::Display: return "Display";
        case EngineTab::Debug: return "Debug";
        default: return "";
    }
}

int engineTabRowCount(EngineTab tab) {
    switch (tab) {
        case EngineTab::Performance: return 6;
        case EngineTab::Visuals: return 4;
        case EngineTab::Controls: return 4;
        case EngineTab::Accessibility: return 3;
        case EngineTab::Display: return 3;
        case EngineTab::Debug: return 4;
        default: return 0;
    }
}

static void fmtSimSize(char* buf, size_t n, int w, int h) {
    if (w > 0 && h > 0)
        std::snprintf(buf, n, "%dx%d", w, h);
    else
        std::snprintf(buf, n, "Auto");
}

const char* engineTabRowLabel(EngineTab tab, int row, const GameSettings& settings, char* buf,
                              size_t bufSize) {
    const auto& p = settings.performance;
    const auto& v = settings.visuals;
    const auto& c = settings.controls;
    const auto& a = settings.accessibility;
    const auto& di = settings.display;
    const auto& d = settings.debug;

    switch (tab) {
        case EngineTab::Performance:
            switch (row) {
                case 0:
                    std::snprintf(buf, bufSize, "Preset: %s", perfPresetLabel(p.mode));
                    break;
                case 1:
                    std::snprintf(buf, bufSize, "Target FPS: %d", p.targetFps);
                    break;
                case 2: {
                    char sz[32];
                    fmtSimSize(sz, sizeof(sz), p.simWidth, p.simHeight);
                    std::snprintf(buf, bufSize, "Sim size: %s", sz);
                    break;
                }
                case 3:
                    std::snprintf(buf, bufSize, "Substeps: %d",
                                  p.substeps > 0 ? p.substeps : effectiveSubsteps(p,
#if defined(__SWITCH__)
                                                                                  true
#else
                                                                                  false
#endif
                                  ));
                    break;
                case 4:
                    std::snprintf(buf, bufSize, "Dynamic resolution: %s",
                                  p.dynamicResolution ? "On" : "Off");
                    break;
                case 5:
                    std::snprintf(buf, bufSize, "Active tiles: %s",
                                  p.activeTiles == ActiveTileMode::Off          ? "Off"
                                  : p.activeTiles == ActiveTileMode::Conservative ? "Stable fallback"
                                                                                  : "Fast Experimental");
                    break;
            }
            break;
        case EngineTab::Visuals:
            switch (row) {
                case 0:
                    std::snprintf(buf, bufSize, "Palette mode: %d", v.paletteMode);
                    break;
                case 1:
                    std::snprintf(buf, bufSize, "AO: %s",
                                  v.ao == VisualAo::Off ? "Off" : v.ao == VisualAo::Low ? "Low" : "High");
                    break;
                case 2:
                    std::snprintf(buf, bufSize, "Glow/Bloom: %s",
                                  v.glowEnabled ? "On" : "Off");
                    break;
                case 3:
                    std::snprintf(buf, bufSize, "Flicker: %s", v.flicker ? "On" : "Off");
                    break;
            }
            break;
        case EngineTab::Controls:
            switch (row) {
                case 0:
                    std::snprintf(buf, bufSize, "Cursor speed: %.2f", c.cursorSpeed);
                    break;
                case 1:
                    std::snprintf(buf, bufSize, "Brush radius: %d", c.brushRadius);
                    break;
                case 2:
                    std::snprintf(buf, bufSize, "Deadzone: %.2f", c.deadzone);
                    break;
                case 3:
                    std::snprintf(buf, bufSize, "Rumble: %s",
                                  c.rumble == RumbleLevel::Off     ? "Off"
                                  : c.rumble == RumbleLevel::Low   ? "Low"
                                  : c.rumble == RumbleLevel::Medium ? "Medium"
                                                                    : "High");
                    break;
            }
            break;
        case EngineTab::Accessibility:
            switch (row) {
                case 0:
                    std::snprintf(buf, bufSize, "UI scale: %.0f%%", a.uiScale * 100.f);
                    break;
                case 1:
                    std::snprintf(buf, bufSize, "Reduce flashing: %s", a.reduceFlashing ? "On" : "Off");
                    break;
                case 2:
                    std::snprintf(buf, bufSize, "Toggle paint: %s", a.togglePaint ? "On" : "Off");
                    break;
            }
            break;
        case EngineTab::Display:
            switch (row) {
                case 0:
                    std::snprintf(buf, bufSize, "Menu: %s", menuChromeLabel(di.menuChrome));
                    break;
                case 1:
                    std::snprintf(buf, bufSize, "Screen orientation: %s",
                                  screenOrientationLabel(di.orientation));
                    break;
                case 2:
                    std::snprintf(buf, bufSize, "Fullscreen simulation: %s",
                                  di.fullscreenSim ? "On" : "Off");
                    break;
            }
            break;
        case EngineTab::Debug:
            switch (row) {
                case 0:
                    std::snprintf(buf, bufSize, "Profiler HUD: %s", profilerHudLabel(d.profilerHud));
                    break;
                case 1:
                    std::snprintf(buf, bufSize, "Show active tiles: %s",
                                  d.showActiveTiles ? "On" : "Off");
                    break;
                case 2:
                    std::snprintf(buf, bufSize, "Material ID view: %s",
                                  d.showMaterialIds ? "On" : "Off");
                    break;
                case 3:
                    std::snprintf(buf, bufSize, "Benchmark scene: %d (confirm to load)",
                                  d.benchmarkScene);
                    break;
            }
            break;
        default: break;
    }
    return buf;
}

void applySettingsToRuntime(App& app) { app.applyRuntimeSettings(); }

void adjustEngineTabRow(App& app, EngineTab tab, int row, int dir) {
    auto& s = app.settings;
    switch (tab) {
        case EngineTab::Performance:
            switch (row) {
                case 0: {
                    int m = static_cast<int>(s.performance.mode) + dir;
                    m = std::clamp(m, 0, static_cast<int>(PerfPreset::Manual));
                    s.performance.mode = static_cast<PerfPreset>(m);
                    if (s.performance.mode != PerfPreset::Manual) {
#if defined(__SWITCH__)
                        applyPerfPreset(s.performance, s.performance.mode, true);
#else
                        applyPerfPreset(s.performance, s.performance.mode, false);
#endif
                    }
                    break;
                }
                case 1:
                    s.performance.targetFps = (dir > 0) ? 60 : 30;
                    SDL_GL_SetSwapInterval(s.performance.targetFps == 60 ? 1 : 2);
                    break;
                case 2: {
                    static const int kSizes[][2] = {{384, 216}, {480, 270}, {640, 360},
                                                    {720, 405}, {960, 540}};
                    int idx = 2;
                    for (int i = 0; i < 5; ++i) {
                        if (s.performance.simWidth == kSizes[i][0] &&
                            s.performance.simHeight == kSizes[i][1]) {
                            idx = i;
                            break;
                        }
                    }
                    idx = std::clamp(idx + dir, 0, 4);
                    s.performance.simWidth = kSizes[idx][0];
                    s.performance.simHeight = kSizes[idx][1];
                    s.performance.mode = PerfPreset::Manual;
                    break;
                }
                case 3:
                    s.performance.substeps = std::clamp(
                        (s.performance.substeps > 0 ? s.performance.substeps : 2) + dir, 1, 2);
                    s.performance.mode = PerfPreset::Manual;
                    break;
                case 4:
                    s.performance.dynamicResolution = !s.performance.dynamicResolution;
                    break;
                case 5: {
                    int m = static_cast<int>(s.performance.activeTiles) + dir;
                    m = std::clamp(m, 0, static_cast<int>(ActiveTileMode::Aggressive));
                    s.performance.activeTiles = static_cast<ActiveTileMode>(m);
                    break;
                }
            }
            break;
        case EngineTab::Visuals:
            switch (row) {
                case 0:
                    s.visuals.paletteMode = std::clamp(s.visuals.paletteMode + dir, 0, 2);
                    break;
                case 1: {
                    int m = static_cast<int>(s.visuals.ao) + dir;
                    m = std::clamp(m, 0, static_cast<int>(VisualAo::High));
                    s.visuals.ao = static_cast<VisualAo>(m);
                    break;
                }
                case 2:
                    s.visuals.glowEnabled = !s.visuals.glowEnabled;
                    break;
                case 3:
                    s.visuals.flicker = !s.visuals.flicker;
                    break;
            }
            break;
        case EngineTab::Controls:
            switch (row) {
                case 0:
                    s.controls.cursorSpeed =
                        std::clamp(s.controls.cursorSpeed + dir * 0.25f, 0.25f, 3.f);
                    break;
                case 1:
                    s.controls.brushRadius = std::clamp(s.controls.brushRadius + dir, 1, 64);
                    break;
                case 2:
                    s.controls.deadzone = std::clamp(s.controls.deadzone + dir * 0.05f, 0.05f, 0.4f);
                    break;
                case 3: {
                    int m = static_cast<int>(s.controls.rumble) + dir;
                    m = std::clamp(m, 0, static_cast<int>(RumbleLevel::High));
                    s.controls.rumble = static_cast<RumbleLevel>(m);
                    break;
                }
            }
            break;
        case EngineTab::Accessibility:
            switch (row) {
                case 0:
                    s.accessibility.uiScale =
                        std::clamp(s.accessibility.uiScale + dir * 0.1f, 0.9f, 1.5f);
                    break;
                case 1:
                    s.accessibility.reduceFlashing = !s.accessibility.reduceFlashing;
                    break;
                case 2:
                    s.accessibility.togglePaint = !s.accessibility.togglePaint;
                    break;
            }
            break;
        case EngineTab::Display:
            switch (row) {
                case 0: {
                    int m = static_cast<int>(s.display.menuChrome) + dir;
                    m = std::clamp(m, 0, static_cast<int>(MenuChrome::Minimal));
                    s.display.menuChrome = static_cast<MenuChrome>(m);
                    break;
                }
                case 1: {
#if defined(__SWITCH__)
                    s.display.orientation = ScreenOrientation::Landscape;
#else
                    int m = static_cast<int>(s.display.orientation) + dir;
                    m = std::clamp(m, 0, static_cast<int>(ScreenOrientation::Portrait));
                    s.display.orientation = static_cast<ScreenOrientation>(m);
#endif
                    break;
                }
                case 2:
                    s.display.fullscreenSim = !s.display.fullscreenSim;
                    break;
            }
            break;
        case EngineTab::Debug:
            switch (row) {
                case 0: {
                    int m = static_cast<int>(s.debug.profilerHud) + dir;
                    m = std::clamp(m, 0, static_cast<int>(ProfilerHud::Full));
                    s.debug.profilerHud = static_cast<ProfilerHud>(m);
                    break;
                }
                case 1:
                    s.debug.showActiveTiles = !s.debug.showActiveTiles;
                    break;
                case 2:
                    s.debug.showMaterialIds = !s.debug.showMaterialIds;
                    if (s.debug.showMaterialIds) s.visuals.paletteMode = 2;
                    else if (s.visuals.paletteMode == 2) s.visuals.paletteMode = 0;
                    break;
                case 3:
                    s.debug.benchmarkScene = std::clamp(s.debug.benchmarkScene + dir, 0, 2);
                    break;
            }
            break;
        default: break;
    }
    markGameSettingsDirty();
    applySettingsToRuntime(app);
}

} // namespace nx
