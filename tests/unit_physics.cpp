#include "test_harness.hpp"
#include "sim/physics_settings.hpp"
#include "save/physics_params_io.hpp"
#include "save/save_paths.hpp"
#include <cmath>
#include <cstdio>
#include <filesystem>

void run_physics_tests(TestContext& ctx) {
    CHECK(ctx, nx::settingsMaterialCount() == 8);
    CHECK(ctx, nx::settingsMaterialAt(0) == nx::MAT_FIRE);
    CHECK(ctx, nx::paramCountFor(nx::MAT_FIRE) == 5);
    CHECK(ctx, nx::paramCountFor(nx::MAT_PLANT) == 2);
    CHECK(ctx, nx::paramSpecAt(nx::MAT_FIRE, 0) != nullptr);
    CHECK(ctx, nx::paramSpecAt(nx::MAT_SAND, 0) == nullptr);

    nx::PhysicsParams p{};
    CHECK(ctx, std::fabs(nx::getParam(p, nx::MAT_FIRE, "fire_speed") - 1.f) < 1e-5f);

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
    CHECK(ctx, nx::savePhysicsParams(custom));

    nx::PhysicsParams loaded{};
    CHECK(ctx, nx::loadPhysicsParams(loaded));
    CHECK(ctx, std::fabs(loaded.fire_speed - 1.25f) < 1e-5f);
    CHECK(ctx, std::fabs(loaded.ice_meltRate - 0.01f) < 1e-5f);

    std::filesystem::remove(path, ec);
}
