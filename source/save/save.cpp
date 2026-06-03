#include "save.hpp"
#include "base64.hpp"
#include "save_paths.hpp"
#include "../game/app.hpp"
#include "../sim/materials.hpp"
#include "../sim/sim_state.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>
#include <chrono>

namespace nx {

bool saveGame(App& app, int slot) {
    migrateLegacySaveData();
#if defined(__SWITCH__)
    if (!ensureSwitchStorageReady())
        return false;
#endif
    if (!app.simPipeline || !app.simPipeline->ready()) return false;
    std::string dir = saveDirectory();
    ensureDirectoryExists(dir);
    std::vector<uint8_t> raw;
    if (!app.simPipeline->readGridTo(raw) || raw.size() != static_cast<size_t>(app.sim.grid_w * app.sim.grid_h))
        return false;
    flipGlRowsToTopDown(raw, app.sim.grid_w, app.sim.grid_h);

    nlohmann::json j;
    j["version"] = 1;
    j["width"] = app.sim.grid_w;
    j["height"] = app.sim.grid_h;
    j["grid"] = base64Encode(raw);
    j["brushX"] = app.sim.brush_x;
    j["brushY"] = app.sim.brush_y;
    j["material"] = static_cast<int>(app.sim.brush_mat);
    j["brushRadius"] = app.sim.brush_radius;
    const auto now = std::chrono::system_clock::now();
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();

    std::ostringstream path;
    path << dir << "slot-" << slot << ".json";
    // Compact dump (no indent): a 640x360 grid base64s to ~230kB and indent=2 ballooned
    // that significantly. Switch SD writes are slow enough that this is felt on every save.
    return atomicWriteFile(path.str(), j.dump());
}

bool loadGame(App& app, int slot) {
    migrateLegacySaveData();
#if defined(__SWITCH__)
    if (!ensureSwitchStorageReady())
        return false;
#endif
    std::ostringstream path;
    path << saveDirectory() << "slot-" << slot << ".json";
    std::ifstream f(path.str());
    if (!f) return false;
    std::stringstream buffer;
    buffer << f.rdbuf();
    nlohmann::json j = nlohmann::json::parse(buffer.str(), nullptr, false);
    if (j.is_discarded()) return false;
    std::string b64s = j.value("grid", std::string{});
    std::vector<uint8_t> dec;
    if (!base64Decode(b64s, dec)) return false;

    int w = j.value("width", 0);
    int h = j.value("height", 0);
    if (w <= 0 || h <= 0) {
        // nxsand reference saves: 160x90 only, no width/height fields
        if (static_cast<int>(dec.size()) == NXSAND_CLASSIC_W * NXSAND_CLASSIC_H) {
            w = NXSAND_CLASSIC_W;
            h = NXSAND_CLASSIC_H;
        } else if (static_cast<int>(dec.size()) == app.sim.grid_w * app.sim.grid_h) {
            w = app.sim.grid_w;
            h = app.sim.grid_h;
        } else {
            return false;
        }
    }
    if (static_cast<int>(dec.size()) != w * h) return false;

    app.simPipeline->uploadGridTopDown(dec, w, h);
    app.sim.gridHasMatter = false;
    for (uint8_t cell : dec) {
        if (cell != MAT_EMPTY) {
            app.sim.gridHasMatter = true;
            break;
        }
    }
    app.sim.sleeping = false;
    app.sim.brush_x = std::clamp(j.value("brushX", app.sim.grid_w / 2), 0, app.sim.grid_w - 1);
    app.sim.brush_y = std::clamp(j.value("brushY", app.sim.grid_h / 2), 0, app.sim.grid_h - 1);
    app.sim.brush_mat = sanitizeBrushMaterial(j.value("material", static_cast<int>(MAT_SAND)));
    app.sim.brush_radius = std::clamp(app.settings.controls.brushRadius, 1, 64);
    return true;
}

SlotMeta getSlotMeta(int slot) {
    migrateLegacySaveData();
    SlotMeta meta;
    std::ostringstream path;
    path << saveDirectory() << "slot-" << slot << ".json";
    std::ifstream f(path.str());
    if (!f) return meta;
    std::stringstream buffer;
    buffer << f.rdbuf();
    nlohmann::json j = nlohmann::json::parse(buffer.str(), nullptr, false);
    if (j.is_discarded() || !j.contains("grid")) return meta;
    meta.empty = false;
    meta.timestampMs = j.value("timestamp", int64_t{0});
    return meta;
}

} // namespace nx
