#include "test_harness.hpp"
#include "save/settings_io.hpp"
#include "game/game_settings.hpp"
#include "save/save_paths.hpp"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

void run_settings_tests(TestContext& ctx) {
    CHECK(ctx, nx::saveDirectory().find("nxsand") != std::string::npos);
    CHECK(ctx, nx::legacySaveDirectory().find("nxengine") != std::string::npos);
    CHECK(ctx, nx::ensureDirectoryExists(nx::saveDirectory()));
    CHECK(ctx, nx::ensureDirectoryExists(nx::legacySaveDirectory()));

    const std::string legacyProbe = nx::legacySaveDirectory() + "migration-probe.json";
    const std::string migratedProbe = nx::saveDirectory() + "migration-probe.json";
    std::error_code ec;
    std::filesystem::remove(legacyProbe, ec);
    std::filesystem::remove(migratedProbe, ec);
    CHECK(ctx, nx::atomicWriteFile(legacyProbe, "{\"ok\":true}"));
    nx::migrateLegacySaveData();
    CHECK(ctx, std::filesystem::exists(migratedProbe));
    std::filesystem::remove(legacyProbe, ec);
    std::filesystem::remove(migratedProbe, ec);

    const std::string path = nx::saveDirectory() + "settings.json";
    std::filesystem::remove(path, ec);

    nx::GameSettings s = nx::defaultGameSettings();
    s.performance.mode = nx::PerfPreset::Quality;
    s.performance.targetFps = 30;
    s.performance.simWidth = 720;
    s.performance.simHeight = 405;
    s.performance.substeps = 2;
    s.performance.dynamicResolution = true;
    s.performance.activeTiles = nx::ActiveTileMode::Aggressive;
    s.performance.simBackend = nx::SimBackend::Compute;
    s.visuals.paletteMode = 2;
    s.visuals.ao = nx::VisualAo::High;
    s.visuals.bloom = nx::VisualBloom::Low;
    s.visuals.flicker = false;
    s.visuals.grain = true;
    s.visuals.upscaleFilter = nx::UpscaleFilter::Lanczos3;
    s.controls.cursorSpeed = 1.5f;
    s.controls.brushRadius = 11;
    s.controls.deadzone = 0.25f;
    s.controls.invertY = true;
    s.controls.rumble = nx::RumbleLevel::High;
    s.accessibility.uiScale = 1.3f;
    s.accessibility.reduceFlashing = true;
    s.accessibility.togglePaint = true;
    s.debug.profilerHud = nx::ProfilerHud::Full;
    s.debug.showActiveTiles = true;
    s.debug.showMaterialIds = true;
    s.debug.benchmarkScene = 2;
    s.display.menuChrome = nx::MenuChrome::Minimal;
    s.display.orientation = nx::ScreenOrientation::Portrait;
    s.display.fullscreenSim = false;

    CHECK(ctx, nx::saveGameSettings(s));

    nx::GameSettings loaded = nx::defaultGameSettings();
    CHECK(ctx, nx::loadGameSettings(loaded));
    CHECK(ctx, loaded.performance.mode == nx::PerfPreset::Quality);
    CHECK(ctx, loaded.performance.targetFps == 30);
    CHECK(ctx, loaded.performance.simWidth == 720);
    CHECK(ctx, loaded.performance.simHeight == 405);
    CHECK(ctx, loaded.performance.substeps == 2);
    CHECK(ctx, loaded.performance.dynamicResolution == true);
    CHECK(ctx, loaded.performance.activeTiles == nx::ActiveTileMode::Aggressive);
    CHECK(ctx, loaded.performance.simBackend == nx::SimBackend::Compute);
    CHECK(ctx, loaded.visuals.paletteMode == 2);
    CHECK(ctx, loaded.visuals.ao == nx::VisualAo::High);
    CHECK(ctx, loaded.visuals.bloom == nx::VisualBloom::Low);
    CHECK(ctx, loaded.visuals.flicker == false);
    CHECK(ctx, loaded.visuals.grain == true);
    CHECK(ctx, loaded.visuals.upscaleFilter == nx::UpscaleFilter::Lanczos3);
    CHECK(ctx, std::fabs(loaded.controls.cursorSpeed - 1.5f) < 1e-5f);
    CHECK(ctx, loaded.controls.brushRadius == 11);
    CHECK(ctx, std::fabs(loaded.controls.deadzone - 0.25f) < 1e-5f);
    CHECK(ctx, loaded.controls.invertY == true);
    CHECK(ctx, loaded.controls.rumble == nx::RumbleLevel::High);
    CHECK(ctx, std::fabs(loaded.accessibility.uiScale - 1.3f) < 1e-5f);
    CHECK(ctx, loaded.accessibility.reduceFlashing == true);
    CHECK(ctx, loaded.accessibility.togglePaint == true);
    CHECK(ctx, loaded.debug.profilerHud == nx::ProfilerHud::Full);
    CHECK(ctx, loaded.debug.showActiveTiles == true);
    CHECK(ctx, loaded.debug.showMaterialIds == true);
    CHECK(ctx, loaded.debug.benchmarkScene == 2);
    CHECK(ctx, loaded.display.menuChrome == nx::MenuChrome::Minimal);
    CHECK(ctx, loaded.display.orientation == nx::ScreenOrientation::Portrait);
    CHECK(ctx, loaded.display.fullscreenSim == false);

    std::filesystem::remove(path, ec);

    nx::VisualSettings vis{};
    nx::applyPerfPresetVisuals(vis, nx::PerfPreset::Balanced);
    CHECK(ctx, vis.flicker == false);
    CHECK(ctx, vis.ao == nx::VisualAo::Low);
    nx::applyPerfPresetVisuals(vis, nx::PerfPreset::BatterySaver);
    CHECK(ctx, vis.flicker == false);
    CHECK(ctx, vis.ao == nx::VisualAo::Off);

    CHECK(ctx, nx::flushGameSettingsIfDirty(s));
    CHECK(ctx, !nx::gameSettingsDirty());
    nx::markGameSettingsDirty();
    CHECK(ctx, nx::gameSettingsDirty());
    CHECK(ctx, nx::flushGameSettingsIfDirty(s));
    CHECK(ctx, !nx::gameSettingsDirty());

    std::filesystem::remove_all(path, ec);
    std::filesystem::create_directories(path + "/blocked", ec);
    {
        std::ofstream keep(path + "/blocked/keep", std::ios::out);
        keep << "x";
    }
    nx::markGameSettingsDirty();
    CHECK(ctx, !nx::flushGameSettingsIfDirty(s));
    CHECK(ctx, nx::gameSettingsDirty());
    std::filesystem::remove_all(path, ec);
}
