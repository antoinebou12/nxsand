#include "test_harness.hpp"
#include "sim/physics_settings.hpp"
#include "save/physics_params_io.hpp"
#include "save/save_paths.hpp"
#include <cmath>
#include <cstdio>
#include <filesystem>

void run_physics_tests(TestContext& ctx) {
    CHECK(ctx, nx::settingsMaterialCount() == 16);
    CHECK(ctx, nx::settingsMaterialAt(0) == nx::MAT_FIRE);
    CHECK(ctx, nx::paramCountFor(nx::MAT_FIRE) == 7);
    CHECK(ctx, nx::paramCountFor(nx::MAT_PLANT) == 3);
    CHECK(ctx, nx::paramCountFor(nx::MAT_COAL) == 1);
    CHECK(ctx, nx::paramCountFor(nx::MAT_TNT) == 1);
    CHECK(ctx, nx::paramCountFor(nx::MAT_BRICK) == 2);
    CHECK(ctx, nx::paramSpecAt(nx::MAT_FIRE, 0) != nullptr);
    CHECK(ctx, nx::paramSpecAt(nx::MAT_SAND, 0) != nullptr);
    CHECK(ctx, nx::paramCountFor(nx::MAT_SAND) == 2);
    CHECK(ctx, nx::paramCountFor(nx::MAT_GUNPOWDER) == 2);
    CHECK(ctx, nx::paramCountFor(nx::MAT_SALT) == 1);
    CHECK(ctx, nx::paramCountFor(nx::MAT_METAL) == 2);
    CHECK(ctx, nx::paramCountFor(nx::MAT_WOOD) == 2);
    CHECK(ctx, nx::paramCountFor(nx::MAT_OIL) == 3);

    nx::PhysicsParams p{};
    CHECK(ctx, std::fabs(nx::getParam(p, nx::MAT_FIRE, "fire_speed") - 1.f) < 1e-5f);
    CHECK(ctx, std::fabs(p.fire_smokeRate - 0.070f) < 1e-5f);
    CHECK(ctx, std::fabs(p.fire_ignitePlant - 0.08f) < 1e-5f);
    CHECK(ctx, std::fabs(p.fire_igniteOil - 0.045f) < 1e-5f);
    CHECK(ctx, std::fabs(p.water_levelRate - 0.12f) < 1e-5f);
    CHECK(ctx, std::fabs(p.lava_spreadRate - 0.09f) < 1e-5f);
    CHECK(ctx, std::fabs(p.oil_igniteRate - 0.07f) < 1e-5f);
    CHECK(ctx, std::fabs(p.oil_floatRate - 0.11f) < 1e-5f);
    CHECK(ctx, std::fabs(p.smoke_driftRate - 0.12f) < 1e-5f);
    CHECK(ctx, std::fabs(p.smoke_fadeRate - 0.010f) < 1e-5f);
    CHECK(ctx, std::fabs(p.sand_wetSlideScale - 0.40f) < 1e-5f);
    CHECK(ctx, std::fabs(p.sand_lithifyRate - 0.005f) < 1e-5f);
    CHECK(ctx, std::fabs(p.gunpowder_wetIgniteScale - 0.20f) < 1e-5f);
    CHECK(ctx, std::fabs(p.gunpowder_packBoost - 0.12f) < 1e-5f);
    CHECK(ctx, std::fabs(p.metal_rustRate - 0.003f) < 1e-5f);
    CHECK(ctx, std::fabs(p.metal_sparkRate - 0.10f) < 1e-5f);
    CHECK(ctx, std::fabs(p.oil_coldScale - 0.40f) < 1e-5f);
    CHECK(ctx, std::fabs(p.wood_charRate - 0.04f) < 1e-5f);
    CHECK(ctx, std::fabs(p.ember_spawnRate - 0.004f) < 1e-5f);
    CHECK(ctx, std::fabs(p.ember_fadeRate - 0.28f) < 1e-5f);
    CHECK(ctx, std::fabs(p.ember_igniteWood - 0.005f) < 1e-5f);
    CHECK(ctx, std::fabs(p.salt_dissolveRate - 0.035f) < 1e-5f);
    CHECK(ctx, std::fabs(p.brick_slideScale - 0.30f) < 1e-5f);
    CHECK(ctx, std::fabs(p.brick_cohesionScale - 0.35f) < 1e-5f);

    nx::adjustParam(p, nx::MAT_FIRE, "fire_speed", 100);
    const auto* spec = nx::paramSpecAt(nx::MAT_FIRE, 0);
    CHECK(ctx, spec != nullptr);
    CHECK(ctx, std::fabs(p.fire_speed - spec->maxV) < 1e-5f);

    nx::adjustParam(p, nx::MAT_FIRE, "fire_speed", -1000);
    CHECK(ctx, std::fabs(p.fire_speed - spec->minV) < 1e-5f);

    const std::string path = nx::saveDirectory() + "physics.json";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    nx::PhysicsParams custom{};
    custom.fire_speed = 1.25f;
    custom.ice_meltRate = 0.01f;
    custom.sand_lithifyRate = 0.012f;
    custom.gunpowder_wetIgniteScale = 0.35f;
    custom.metal_rustRate = 0.008f;
    custom.wood_charRate = 0.06f;
    custom.ember_spawnRate = 0.008f;
    custom.ember_fadeRate = 0.35f;
    custom.ember_igniteWood = 0.012f;
    custom.salt_dissolveRate = 0.05f;
    custom.brick_slideScale = 0.22f;
    custom.brick_cohesionScale = 0.5f;
    CHECK(ctx, nx::savePhysicsParams(custom));

    nx::PhysicsParams loaded{};
    CHECK(ctx, nx::loadPhysicsParams(loaded));
    CHECK(ctx, std::fabs(loaded.fire_speed - 1.25f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.ice_meltRate - 0.01f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.sand_lithifyRate - 0.012f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.gunpowder_wetIgniteScale - 0.35f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.metal_rustRate - 0.008f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.wood_charRate - 0.06f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.ember_spawnRate - 0.008f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.ember_fadeRate - 0.35f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.ember_igniteWood - 0.012f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.salt_dissolveRate - 0.05f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.brick_slideScale - 0.22f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.brick_cohesionScale - 0.5f) < 1e-5f);

    std::filesystem::remove(path, ec);
}
