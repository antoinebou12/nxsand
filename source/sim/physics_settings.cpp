#include "physics_settings.hpp"
#include <cstdio>
#include <cstring>

namespace nx {

static constexpr Material kSettingsMats[] = {
    MAT_FIRE, MAT_WATER, MAT_SMOKE, MAT_ACID, MAT_PLANT, MAT_LAVA, MAT_OIL, MAT_ICE,
};

int settingsMaterialCount() {
    return int(sizeof(kSettingsMats) / sizeof(kSettingsMats[0]));
}

Material settingsMaterialAt(int i) {
    if (i < 0 || i >= settingsMaterialCount()) return MAT_FIRE;
    return kSettingsMats[i];
}

static const ParamSpec kFire[] = {
    {"fire_speed", "Rise speed", 0.1f, 2.f, 0.1f, 1.f},
    {"fire_smokeRate", "Smoke out", 0.f, 0.20f, 0.005f, 0.070f},
    {"fire_ignitePlant", "Ignite plant", 0.f, 0.35f, 0.005f, 0.08f},
    {"fire_igniteOil", "Ignite oil", 0.f, 0.25f, 0.005f, 0.045f},
    {"fire_spreadRate", "Drift", 0.f, 0.25f, 0.01f, 0.040f},
};
static const ParamSpec kWater[] = {
    {"water_flowRate", "Spread", 0.05f, 0.85f, 0.025f, 0.85f},
    {"water_levelRate", "Level", 0.f, 0.15f, 0.005f, 0.10f},
};
static const ParamSpec kSmoke[] = {
    {"smoke_fadeRate", "Fade", 0.f, 0.15f, 0.005f, 0.010f},
    {"smoke_driftRate", "Drift", 0.f, 0.7f, 0.025f, 0.12f},
};
static const ParamSpec kAcid[] = {
    {"acid_flowRate", "Spread", 0.05f, 0.70f, 0.025f, 0.28f},
    {"acid_wallCorrode", "Burn wall", 0.f, 0.12f, 0.005f, 0.06f},
    {"acid_stoneCorrode", "Burn stone", 0.f, 0.08f, 0.005f, 0.045f},
};
static const ParamSpec kPlant[] = {
    {"plant_growthRate", "Growth", 0.002f, 0.20f, 0.005f, 0.12f},
    {"plant_wallSupport", "Wall support", 0.f, 1.f, 1.f, 1.f},
};
static const ParamSpec kLava[] = {
    {"lava_flowRate", "Fall", 0.f, 1.f, 0.05f, 0.16f},
    {"lava_spreadRate", "Spread", 0.f, 0.45f, 0.02f, 0.09f},
    {"lava_igniteGas", "Ignite gas", 0.f, 0.2f, 0.01f, 0.08f},
};
static const ParamSpec kOil[] = {
    {"oil_igniteRate", "Ignite", 0.f, 0.2f, 0.01f, 0.07f},
    {"oil_floatRate", "Spread", 0.f, 0.7f, 0.025f, 0.16f},
};
static const ParamSpec kIce[] = {
    {"ice_meltRate", "Melt", 0.f, 0.08f, 0.002f, 0.015f},
    {"ice_freezeRate", "Freeze", 0.f, 0.01f, 0.0005f, 0.030f},
};
static const ParamSpec* specsFor(Material m, int& count) {
    switch (m) {
        case MAT_FIRE: count = 5; return kFire;
        case MAT_WATER: count = 2; return kWater;
        case MAT_SMOKE: count = 2; return kSmoke;
        case MAT_ACID: count = 3; return kAcid;
        case MAT_PLANT: count = 2; return kPlant;
        case MAT_LAVA: count = 3; return kLava;
        case MAT_OIL: count = 2; return kOil;
        case MAT_ICE: count = 2; return kIce;
        default: count = 0; return nullptr;
    }
}

int paramCountFor(Material m) {
    int c = 0;
    specsFor(m, c);
    return c;
}

const ParamSpec* paramSpecAt(Material m, int i) {
    int c = 0;
    const ParamSpec* s = specsFor(m, c);
    if (!s || i < 0 || i >= c) return nullptr;
    return &s[i];
}

static float* ptr(PhysicsParams& p, const char* id) {
    if (!strcmp(id, "fire_speed")) return &p.fire_speed;
    if (!strcmp(id, "fire_smokeRate")) return &p.fire_smokeRate;
    if (!strcmp(id, "fire_ignitePlant")) return &p.fire_ignitePlant;
    if (!strcmp(id, "fire_igniteOil")) return &p.fire_igniteOil;
    if (!strcmp(id, "fire_spreadRate")) return &p.fire_spreadRate;
    if (!strcmp(id, "smoke_fadeRate")) return &p.smoke_fadeRate;
    if (!strcmp(id, "smoke_driftRate")) return &p.smoke_driftRate;
    if (!strcmp(id, "water_flowRate")) return &p.water_flowRate;
    if (!strcmp(id, "water_levelRate")) return &p.water_levelRate;
    if (!strcmp(id, "acid_flowRate")) return &p.acid_flowRate;
    if (!strcmp(id, "acid_wallCorrode")) return &p.acid_wallCorrode;
    if (!strcmp(id, "acid_stoneCorrode")) return &p.acid_stoneCorrode;
    if (!strcmp(id, "plant_growthRate")) return &p.plant_growthRate;
    if (!strcmp(id, "plant_wallSupport")) return &p.plant_wallSupport;
    if (!strcmp(id, "lava_flowRate")) return &p.lava_flowRate;
    if (!strcmp(id, "lava_spreadRate")) return &p.lava_spreadRate;
    if (!strcmp(id, "lava_igniteGas")) return &p.lava_igniteGas;
    if (!strcmp(id, "oil_igniteRate")) return &p.oil_igniteRate;
    if (!strcmp(id, "oil_floatRate")) return &p.oil_floatRate;
    if (!strcmp(id, "ice_meltRate")) return &p.ice_meltRate;
    if (!strcmp(id, "ice_freezeRate")) return &p.ice_freezeRate;
    return nullptr;
}

static const float* ptrC(const PhysicsParams& p, const char* id) {
    return ptr(const_cast<PhysicsParams&>(p), id);
}

float getParam(const PhysicsParams& p, Material /*m*/, const char* id) {
    const float* v = ptrC(p, id);
    return v ? *v : 0.f;
}

void adjustParam(PhysicsParams& p, Material m, const char* id, int direction) {
    const ParamSpec* spec = nullptr;
    int c = 0;
    const ParamSpec* arr = specsFor(m, c);
    for (int i = 0; i < c; ++i) {
        if (!strcmp(arr[i].id, id)) {
            spec = &arr[i];
            break;
        }
    }
    float* v = ptr(p, id);
    if (!spec || !v) return;
    *v += float(direction) * spec->step;
    if (*v < spec->minV) *v = spec->minV;
    if (*v > spec->maxV) *v = spec->maxV;
}

void formatParamValue(char* buf, size_t bufSize, const ParamSpec* spec, float v) {
    if (!buf || bufSize == 0) return;
    if (spec && spec->step >= 1.f) {
        std::snprintf(buf, bufSize, "%s", v >= 0.5f ? "On" : "Off");
    } else {
        std::snprintf(buf, bufSize, "%.3f", v);
    }
}

} // namespace nx
