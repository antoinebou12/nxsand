#include "test_harness.hpp"
#include "ui/layout.hpp"
#include "ui/material_wheel.hpp"
#include "sim/materials.hpp"
#include <cmath>

void run_material_wheel_tests(TestContext& ctx) {
    const nx::PlayRegion play =
        nx::getPlayRegionForScene(1280, 720, 640, 360, true, false, false, 1.f);
    CHECK(ctx, play.w > 8 && play.h > 8);

    const float expectCx = float(play.x) + float(play.w) * 0.5f;
    const float expectCy = float(play.y) + float(play.h) * 0.5f;
    const nx::MaterialWheelLayout wl =
        nx::materialWheelLayout(1280, 720, 1.f, &play);
    CHECK(ctx, std::fabs(wl.cx - expectCx) < 0.01f);
    CHECK(ctx, std::fabs(wl.cy - expectCy) < 0.01f);
    CHECK(ctx, wl.rad > wl.minPickDist);

    const int n = nx::selectorMaterialCount();
    CHECK(ctx, n > 0);
    const float pickR = wl.minPickDist + (wl.rad - wl.minPickDist) * 0.5f;
    const int idxTop = nx::materialWheelIndexFromPointer(wl.cx, wl.cy - pickR, wl, n);
    CHECK(ctx, idxTop == 0);

    const nx::MaterialWheelLayout screenOnly = nx::materialWheelLayout(1280, 720, 1.f, nullptr);
    CHECK(ctx, std::fabs(screenOnly.cx - 640.f) < 0.01f);
    CHECK(ctx, std::fabs(screenOnly.cy - 720.f * 0.42f) < 0.01f);
    CHECK(ctx, std::fabs(screenOnly.cx - wl.cx) > 1.f || std::fabs(screenOnly.cy - wl.cy) > 1.f);
}
