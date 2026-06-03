#include "settings_io.hpp"
#include "save_paths.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>

namespace nx {

static bool g_dirty = false;

static std::string settingsPath() {
    return saveDirectory() + "settings.json";
}

static int readEnumInt(const nlohmann::json& j, const char* k, int def) {
    auto it = j.find(k);
    if (it == j.end() || !it->is_number_integer()) return def;
    return it->get<int>();
}

static bool readBool(const nlohmann::json& j, const char* k, bool def) {
    auto it = j.find(k);
    if (it == j.end() || !it->is_boolean()) return def;
    return it->get<bool>();
}

static float readFloat(const nlohmann::json& j, const char* k, float def) {
    auto it = j.find(k);
    if (it == j.end() || !it->is_number()) return def;
    return static_cast<float>(it->get<double>());
}

static int readInt(const nlohmann::json& j, const char* k, int def) {
    auto it = j.find(k);
    if (it == j.end() || !it->is_number_integer()) return def;
    return it->get<int>();
}

static void migrateSettings(GameSettings& s) {
#if defined(__SWITCH__)
    s.display.orientation = ScreenOrientation::Landscape;
#endif
    if (s.version < CURRENT_SETTINGS_VERSION) {
        s.version = CURRENT_SETTINGS_VERSION;
    }
}

static void loadVisualsFromJson(const nlohmann::json& v, VisualSettings& vis) {
    int pm = readInt(v, "paletteMode", vis.paletteMode);
    vis.paletteMode = std::clamp(pm, 0, 2);
    int ao = readEnumInt(v, "ao", static_cast<int>(vis.ao));
    ao = std::clamp(ao, 0, static_cast<int>(VisualAo::High));
    vis.ao = static_cast<VisualAo>(ao);
    int bloom = readEnumInt(v, "bloom", static_cast<int>(vis.bloom));
    bloom = std::clamp(bloom, 0, static_cast<int>(VisualBloom::Low));
    vis.bloom = static_cast<VisualBloom>(bloom);
    vis.flicker = readBool(v, "flicker", false);
    vis.grain = readBool(v, "grain", false);
    int uf = readEnumInt(v, "upscaleFilter", static_cast<int>(vis.upscaleFilter));
    uf = std::clamp(uf, 0, static_cast<int>(UpscaleFilter::Count) - 1);
    vis.upscaleFilter = static_cast<UpscaleFilter>(uf);
    if (readBool(v, "glowEnabled", false) && vis.bloom == VisualBloom::Off) {
        vis.bloom = VisualBloom::Low;
    }
}

bool loadGameSettings(GameSettings& out) {
    migrateLegacySaveData();
    out = defaultGameSettings();
    std::ifstream f(settingsPath());
    if (!f) return false;
    try {
        nlohmann::json j = nlohmann::json::parse(f, nullptr, false);
        if (j.is_discarded()) return false;

        out.version = readInt(j, "version", CURRENT_SETTINGS_VERSION);

        if (j.contains("performance") && j["performance"].is_object()) {
            const auto& p = j["performance"];
            out.performance.mode = static_cast<PerfPreset>(
                readEnumInt(p, "mode", static_cast<int>(PerfPreset::Balanced)));
            out.performance.targetFps = readInt(p, "targetFps", 60);
            out.performance.simWidth = readInt(p, "simWidth", 0);
            out.performance.simHeight = readInt(p, "simHeight", 0);
            out.performance.substeps = readInt(p, "substeps", 2);
            out.performance.dynamicResolution = readBool(p, "dynamicResolution", false);
            out.performance.activeTiles = static_cast<ActiveTileMode>(
                readEnumInt(p, "activeTiles", static_cast<int>(out.performance.activeTiles)));
            out.performance.simBackend = static_cast<SimBackend>(
                readEnumInt(p, "simBackend", static_cast<int>(out.performance.simBackend)));
        }

        if (j.contains("visuals") && j["visuals"].is_object()) {
            loadVisualsFromJson(j["visuals"], out.visuals);
        } else if (j.contains("render") && j["render"].is_object()) {
            loadVisualsFromJson(j["render"], out.visuals);
        }

        if (j.contains("controls") && j["controls"].is_object()) {
            const auto& c = j["controls"];
            out.controls.cursorSpeed = readFloat(c, "cursorSpeed", 1.f);
            out.controls.brushRadius = readInt(c, "brushRadius", 3);
            out.controls.deadzone = readFloat(c, "deadzone", 0.18f);
            out.controls.invertY = readBool(c, "invertY", false);
            out.controls.rumble = static_cast<RumbleLevel>(
                readEnumInt(c, "rumble", static_cast<int>(RumbleLevel::Medium)));
            out.controls.sound = static_cast<SoundLevel>(
                readEnumInt(c, "sound", static_cast<int>(SoundLevel::Medium)));
        }

        if (j.contains("accessibility") && j["accessibility"].is_object()) {
            const auto& a = j["accessibility"];
            out.accessibility.uiScale = readFloat(a, "uiScale", 1.f);
            out.accessibility.reduceFlashing = readBool(a, "reduceFlashing", false);
            out.accessibility.togglePaint = readBool(a, "togglePaint", false);
        }

        if (j.contains("display") && j["display"].is_object()) {
            const auto& di = j["display"];
            out.display.menuChrome = static_cast<MenuChrome>(
                readEnumInt(di, "menuChrome", static_cast<int>(MenuChrome::Full)));
            out.display.orientation = static_cast<ScreenOrientation>(
                readEnumInt(di, "orientation", static_cast<int>(ScreenOrientation::Auto)));
            out.display.fullscreenSim = readBool(di, "fullscreenSim", true);
        }

        if (j.contains("debug") && j["debug"].is_object()) {
            const auto& d = j["debug"];
            out.debug.profilerHud = static_cast<ProfilerHud>(
                readEnumInt(d, "profilerHud", static_cast<int>(ProfilerHud::Compact)));
            out.debug.showActiveTiles = readBool(d, "showActiveTiles", false);
            out.debug.showMaterialIds = readBool(d, "showMaterialIds", false);
            out.debug.benchmarkScene = readInt(d, "benchmarkScene", 0);
        }

        migrateSettings(out);
        g_dirty = false;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool saveGameSettings(const GameSettings& s) {
    if (!ensureSaveDirectoryReady()) return false;

    nlohmann::json j;
    j["version"] = CURRENT_SETTINGS_VERSION;

    j["performance"]["mode"] = static_cast<int>(s.performance.mode);
    j["performance"]["targetFps"] = s.performance.targetFps;
    j["performance"]["simWidth"] = s.performance.simWidth;
    j["performance"]["simHeight"] = s.performance.simHeight;
    j["performance"]["substeps"] = s.performance.substeps;
    j["performance"]["dynamicResolution"] = s.performance.dynamicResolution;
    j["performance"]["activeTiles"] = static_cast<int>(s.performance.activeTiles);
    j["performance"]["simBackend"] = static_cast<int>(s.performance.simBackend);

    j["visuals"]["paletteMode"] = s.visuals.paletteMode;
    j["visuals"]["ao"] = static_cast<int>(s.visuals.ao);
    j["visuals"]["bloom"] = static_cast<int>(s.visuals.bloom);
    j["visuals"]["flicker"] = s.visuals.flicker;
    j["visuals"]["grain"] = s.visuals.grain;
    j["visuals"]["upscaleFilter"] = static_cast<int>(s.visuals.upscaleFilter);

    j["controls"]["cursorSpeed"] = s.controls.cursorSpeed;
    j["controls"]["brushRadius"] = s.controls.brushRadius;
    j["controls"]["deadzone"] = s.controls.deadzone;
    j["controls"]["invertY"] = s.controls.invertY;
    j["controls"]["rumble"] = static_cast<int>(s.controls.rumble);
    j["controls"]["sound"] = static_cast<int>(s.controls.sound);

    j["accessibility"]["uiScale"] = s.accessibility.uiScale;
    j["accessibility"]["reduceFlashing"] = s.accessibility.reduceFlashing;
    j["accessibility"]["togglePaint"] = s.accessibility.togglePaint;

    j["display"]["menuChrome"] = static_cast<int>(s.display.menuChrome);
    j["display"]["orientation"] = static_cast<int>(s.display.orientation);
    j["display"]["fullscreenSim"] = s.display.fullscreenSim;

    j["debug"]["profilerHud"] = static_cast<int>(s.debug.profilerHud);
    j["debug"]["showActiveTiles"] = s.debug.showActiveTiles;
    j["debug"]["showMaterialIds"] = s.debug.showMaterialIds;
    j["debug"]["benchmarkScene"] = s.debug.benchmarkScene;

    if (!atomicWriteFile(settingsPath(), j.dump())) return false;
    g_dirty = false;
    return true;
}

void markGameSettingsDirty() { g_dirty = true; }

bool gameSettingsDirty() { return g_dirty; }

bool flushGameSettingsIfDirty(const GameSettings& settings) {
    if (!g_dirty) return true;
    return saveGameSettings(settings);
}

} // namespace nx
