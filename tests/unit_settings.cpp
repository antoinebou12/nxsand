#include "test_harness.hpp"
#include "save/settings_io.hpp"
#include "game/game_settings.hpp"
#include "save/save_paths.hpp"
#include <cmath>
#include <cstdio>
#include <filesystem>

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
    s.performance.simWidth = 720;
    s.performance.simHeight = 405;
    s.performance.substeps = 2;
    s.controls.cursorSpeed = 1.5f;
    s.debug.profilerHud = nx::ProfilerHud::Full;
    s.display.menuChrome = nx::MenuChrome::Minimal;
    s.display.orientation = nx::ScreenOrientation::Portrait;
    s.display.fullscreenSim = false;

    CHECK(ctx, nx::saveGameSettings(s));

    nx::GameSettings loaded = nx::defaultGameSettings();
    CHECK(ctx, nx::loadGameSettings(loaded));
    CHECK(ctx, loaded.performance.mode == nx::PerfPreset::Quality);
    CHECK(ctx, loaded.performance.simWidth == 720);
    CHECK(ctx, loaded.performance.substeps == 2);
    CHECK(ctx, std::fabs(loaded.controls.cursorSpeed - 1.5f) < 1e-5f);
    CHECK(ctx, loaded.debug.profilerHud == nx::ProfilerHud::Full);
    CHECK(ctx, loaded.display.menuChrome == nx::MenuChrome::Minimal);
    CHECK(ctx, loaded.display.orientation == nx::ScreenOrientation::Portrait);
    CHECK(ctx, loaded.display.fullscreenSim == false);

    std::filesystem::remove(path, ec);
}
