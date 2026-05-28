#include "physics_params_io.hpp"
#include "save_paths.hpp"
#include <fstream>

#include <nlohmann/json.hpp>

namespace nx {

static bool g_dirty = false;

static std::string physicsPath() {
    return saveDirectory() + "physics.json";
}

bool loadPhysicsParams(PhysicsParams& out) {
    std::ifstream f(physicsPath());
    if (!f) return false;
    try {
        nlohmann::json j = nlohmann::json::parse(f, nullptr, false);
        if (j.is_discarded()) return false;
        auto g = [&](const char* k, float& v) {
            auto it = j.find(k);
            if (it == j.end() || !it->is_number()) return;
            v = static_cast<float>(it->get<double>());
        };
        g("fire_speed", out.fire_speed);
        g("fire_smokeRate", out.fire_smokeRate);
        g("fire_ignitePlant", out.fire_ignitePlant);
        g("fire_igniteOil", out.fire_igniteOil);
        g("fire_spreadRate", out.fire_spreadRate);
        g("smoke_fadeRate", out.smoke_fadeRate);
        g("smoke_driftRate", out.smoke_driftRate);
        g("water_flowRate", out.water_flowRate);
        g("water_levelRate", out.water_levelRate);
        g("acid_flowRate", out.acid_flowRate);
        g("acid_wallCorrode", out.acid_wallCorrode);
        g("acid_stoneCorrode", out.acid_stoneCorrode);
        g("plant_growthRate", out.plant_growthRate);
        g("plant_wallSupport", out.plant_wallSupport);
        g("lava_flowRate", out.lava_flowRate);
        g("lava_spreadRate", out.lava_spreadRate);
        g("lava_igniteGas", out.lava_igniteGas);
        g("oil_igniteRate", out.oil_igniteRate);
        g("oil_floatRate", out.oil_floatRate);
        g("ice_meltRate", out.ice_meltRate);
        g("ice_freezeRate", out.ice_freezeRate);
        g_dirty = false;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool savePhysicsParams(const PhysicsParams& p) {
    ensureDirectoryExists(saveDirectory());

    nlohmann::json j;
    j["fire_speed"] = p.fire_speed;
    j["fire_smokeRate"] = p.fire_smokeRate;
    j["fire_ignitePlant"] = p.fire_ignitePlant;
    j["fire_igniteOil"] = p.fire_igniteOil;
    j["fire_spreadRate"] = p.fire_spreadRate;
    j["smoke_fadeRate"] = p.smoke_fadeRate;
    j["smoke_driftRate"] = p.smoke_driftRate;
    j["water_flowRate"] = p.water_flowRate;
    j["water_levelRate"] = p.water_levelRate;
    j["acid_flowRate"] = p.acid_flowRate;
    j["acid_wallCorrode"] = p.acid_wallCorrode;
    j["acid_stoneCorrode"] = p.acid_stoneCorrode;
    j["plant_growthRate"] = p.plant_growthRate;
    j["plant_wallSupport"] = p.plant_wallSupport;
    j["lava_flowRate"] = p.lava_flowRate;
    j["lava_spreadRate"] = p.lava_spreadRate;
    j["lava_igniteGas"] = p.lava_igniteGas;
    j["oil_igniteRate"] = p.oil_igniteRate;
    j["oil_floatRate"] = p.oil_floatRate;
    j["ice_meltRate"] = p.ice_meltRate;
    j["ice_freezeRate"] = p.ice_freezeRate;

    if (!atomicWriteFile(physicsPath(), j.dump(2))) return false;
    g_dirty = false;
    return true;
}

void markPhysicsParamsDirty() { g_dirty = true; }

void flushPhysicsParamsIfDirty(const PhysicsParams& params) {
    if (!g_dirty) return;
    savePhysicsParams(params);
}

} // namespace nx
