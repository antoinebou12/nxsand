#include "test_harness.hpp"
#include "sim/physics_settings.hpp"
#include <cstring>

void run_physics_settings_tests(TestContext& ctx) {
    for (int mi = 0; mi < nx::settingsMaterialCount(); ++mi) {
        const nx::Material m = nx::settingsMaterialAt(mi);
        const int pc = nx::paramCountFor(m);
        for (int pi = 0; pi < pc; ++pi) {
            const nx::ParamSpec* spec = nx::paramSpecAt(m, pi);
            CHECK(ctx, spec != nullptr);
            if (!spec) continue;
            nx::PhysicsParams p{};
            nx::adjustParam(p, m, spec->id, 1);
            const float after = nx::getParam(p, m, spec->id);
            CHECK(ctx, after != 0.f || spec->minV == 0.f);
        }
    }

    nx::PhysicsParams p{};
    p.plant_wallSupport = 1.f;
    char buf[16];
    const nx::ParamSpec* wallSpec = nx::paramSpecAt(nx::MAT_PLANT, 1);
    CHECK(ctx, wallSpec != nullptr);
    if (wallSpec) {
        nx::formatParamValue(buf, sizeof(buf), wallSpec, p.plant_wallSupport);
        CHECK(ctx, std::strcmp(buf, "On") == 0);
        p.plant_wallSupport = 0.f;
        nx::formatParamValue(buf, sizeof(buf), wallSpec, p.plant_wallSupport);
        CHECK(ctx, std::strcmp(buf, "Off") == 0);
    }
}
