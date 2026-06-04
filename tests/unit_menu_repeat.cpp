#include "test_harness.hpp"
#include "../source/platform/input/menu_repeat.hpp"

namespace {

void test_first_frame_fires(TestContext& ctx) {
    nx::MenuRepeatState r;
    auto p = r.tick(0.016f, true, false, false, false);
    CHECK(ctx, p.up);
    CHECK(ctx, !p.down);
}

void test_no_repeat_during_delay(TestContext& ctx) {
    nx::MenuRepeatState r;
    (void)r.tick(0.016f, true, false, false, false);
    float t = 0.f;
    bool any = false;
    while (t < 0.20f) {
        t += 0.016f;
        auto p = r.tick(0.016f, true, false, false, false);
        if (p.up) any = true;
    }
    CHECK(ctx, !any);
}

void test_repeat_after_delay(TestContext& ctx) {
    nx::MenuRepeatState r;
    (void)r.tick(0.016f, true, false, false, false);
    float t = 0.f;
    int pulses = 0;
    while (t < 0.75f) {
        t += 0.016f;
        auto p = r.tick(0.016f, true, false, false, false);
        if (p.up) ++pulses;
    }
    CHECK(ctx, pulses >= 2);
}

void test_release_resets(TestContext& ctx) {
    nx::MenuRepeatState r;
    (void)r.tick(0.016f, true, false, false, false);
    (void)r.tick(0.5f, true, false, false, false);
    (void)r.tick(0.016f, false, false, false, false);
    auto p = r.tick(0.016f, true, false, false, false);
    CHECK(ctx, p.up);
    auto p2 = r.tick(0.016f, true, false, false, false);
    CHECK(ctx, !p2.up);
}

} // namespace

void run_menu_repeat_tests(TestContext& ctx) {
    test_first_frame_fires(ctx);
    test_no_repeat_during_delay(ctx);
    test_repeat_after_delay(ctx);
    test_release_resets(ctx);
}
