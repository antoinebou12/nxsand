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
        g("ember_spawnRate", out.ember_spawnRate);
        g("ember_fadeRate", out.ember_fadeRate);
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
        g("sand_wetSlideScale", out.sand_wetSlideScale);
        g("sand_lithifyRate", out.sand_lithifyRate);
        g("gunpowder_wetIgniteScale", out.gunpowder_wetIgniteScale);
        g("gunpowder_packBoost", out.gunpowder_packBoost);
        g("metal_rustRate", out.metal_rustRate);
        g("metal_sparkRate", out.metal_sparkRate);
        g("oil_coldScale", out.oil_coldScale);
        g("wood_charRate", out.wood_charRate);
        g("ember_igniteWood", out.ember_igniteWood);
        g("salt_dissolveRate", out.salt_dissolveRate);
        g_dirty = false;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool savePhysicsParams(const PhysicsParams& p) {
    if (!ensureSaveDirectoryReady()) return false;

    nlohmann::json j;
    j["fire_speed"] = p.fire_speed;
    j["fire_smokeRate"] = p.fire_smokeRate;
    j["fire_ignitePlant"] = p.fire_ignitePlant;
    j["fire_igniteOil"] = p.fire_igniteOil;
    j["fire_spreadRate"] = p.fire_spreadRate;
    j["ember_spawnRate"] = p.ember_spawnRate;
    j["ember_fadeRate"] = p.ember_fadeRate;
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
    j["sand_wetSlideScale"] = p.sand_wetSlideScale;
    j["sand_lithifyRate"] = p.sand_lithifyRate;
    j["gunpowder_wetIgniteScale"] = p.gunpowder_wetIgniteScale;
    j["gunpowder_packBoost"] = p.gunpowder_packBoost;
    j["metal_rustRate"] = p.metal_rustRate;
    j["metal_sparkRate"] = p.metal_sparkRate;
    j["oil_coldScale"] = p.oil_coldScale;
    j["wood_charRate"] = p.wood_charRate;
    j["ember_igniteWood"] = p.ember_igniteWood;
    j["salt_dissolveRate"] = p.salt_dissolveRate;

    if (!atomicWriteFile(physicsPath(), j.dump())) return false;
    g_dirty = false;
    return true;
}

void markPhysicsParamsDirty() { g_dirty = true; }

bool physicsParamsDirty() { return g_dirty; }

bool flushPhysicsParamsIfDirty(const PhysicsParams& params) {
    if (!g_dirty) return true;
    return savePhysicsParams(params);
}

} // namespace nx
