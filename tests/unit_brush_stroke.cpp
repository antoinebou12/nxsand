#include "test_harness.hpp"
#include "sim/brush_stroke.hpp"
#include <cmath>

void run_brush_stroke_tests(TestContext& ctx) {
    using nx::planBrushStroke;
    using nx::BrushStrokePlan;

    // Zero-length stroke: steps clamps to 1, producing two dabs at the same
    // point (degenerate loop emits i=0 and i=steps). Bbox is the single disk.
    {
        const BrushStrokePlan p = planBrushStroke(40, 50, 40, 50, 4);
        CHECK(ctx, p.dabs.size() == 2);
        CHECK(ctx, p.dabs[0].x == 40 && p.dabs[0].y == 50);
        CHECK(ctx, p.dabs[1].x == 40 && p.dabs[1].y == 50);
        CHECK(ctx, p.dirtyMinX == 36 && p.dirtyMaxX == 44);
        CHECK(ctx, p.dirtyMinY == 46 && p.dirtyMaxY == 54);
    }

    // Single-pixel stroke: endpoints captured exactly.
    {
        const BrushStrokePlan p = planBrushStroke(10, 10, 11, 10, 2);
        CHECK(ctx, p.dabs.size() >= 2);
        CHECK(ctx, p.dabs.front().x == 10 && p.dabs.front().y == 10);
        CHECK(ctx, p.dabs.back().x == 11 && p.dabs.back().y == 10);
    }

    // Long diagonal: step count matches ceil(dist / max(1, radius * 0.35));
    // endpoints exact; intermediate dabs monotonic in x and y.
    {
        const int x0 = 0, y0 = 0, x1 = 100, y1 = 100, r = 4;
        const float dist = std::sqrt(float((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)));
        const float spacing = std::max(1.f, float(r) * 0.35f);
        const int expectedSteps = std::max(1, int(std::ceil(dist / spacing)));
        const BrushStrokePlan p = planBrushStroke(x0, y0, x1, y1, r);
        CHECK(ctx, int(p.dabs.size()) == expectedSteps + 1);
        CHECK(ctx, p.dabs.front().x == x0 && p.dabs.front().y == y0);
        CHECK(ctx, p.dabs.back().x == x1 && p.dabs.back().y == y1);
        bool monotonic = true;
        for (size_t i = 1; i < p.dabs.size(); ++i) {
            if (p.dabs[i].x < p.dabs[i - 1].x || p.dabs[i].y < p.dabs[i - 1].y) {
                monotonic = false;
                break;
            }
        }
        CHECK(ctx, monotonic);
    }

    // Doubling radius widens spacing, halving the dab count for the same stroke.
    {
        const BrushStrokePlan small = planBrushStroke(0, 0, 200, 0, 4);
        const BrushStrokePlan big = planBrushStroke(0, 0, 200, 0, 8);
        CHECK(ctx, big.dabs.size() < small.dabs.size());
        // Tolerance: spacing scales with radius (0.35*r), so the ratio is ~2.
        const double ratio = double(small.dabs.size() - 1) / double(big.dabs.size() - 1);
        CHECK(ctx, ratio > 1.6 && ratio < 2.4);
    }

    // Radius=0: spacing clamps to 1; produces dist+1 dabs (no div-by-zero).
    {
        const BrushStrokePlan p = planBrushStroke(0, 0, 10, 0, 0);
        CHECK(ctx, p.dabs.size() == 11);
        CHECK(ctx, p.dirtyMinX == 0 && p.dirtyMaxX == 10);
        CHECK(ctx, p.dirtyMinY == 0 && p.dirtyMaxY == 0);
    }

    // Horizontal stroke bbox.
    {
        const BrushStrokePlan p = planBrushStroke(10, 50, 90, 50, 5);
        CHECK(ctx, p.dirtyMinX == 5 && p.dirtyMaxX == 95);
        CHECK(ctx, p.dirtyMinY == 45 && p.dirtyMaxY == 55);
    }

    // Vertical stroke bbox (mirror of horizontal).
    {
        const BrushStrokePlan p = planBrushStroke(50, 10, 50, 90, 5);
        CHECK(ctx, p.dirtyMinX == 45 && p.dirtyMaxX == 55);
        CHECK(ctx, p.dirtyMinY == 5 && p.dirtyMaxY == 95);
    }
}
