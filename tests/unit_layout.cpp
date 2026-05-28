#include "test_harness.hpp"
#include "gpu/active_tiles.hpp"
#include "ui/layout.hpp"

void run_layout_tests(TestContext& ctx) {
    const nx::PlayRegion full = nx::getPlayRegion(1280, 720, 160, 90, false);
    CHECK(ctx, full.w > 0 && full.h > 0);
    CHECK(ctx, full.scaleX >= 1 && full.scaleY >= 1);
    CHECK(ctx, full.y == 0);
    CHECK(ctx, full.x + full.w <= 1280);
    CHECK(ctx, full.y + full.h <= 720);

    const nx::PlayRegion play = nx::getPlayRegion(1280, 720, 160, 90, true);
    CHECK(ctx, play.y > 0);
    CHECK(ctx, play.h < full.h);

    int gx = -1, gy = -1;
    const int cx = play.x + play.w / 2;
    const int cy = play.y + play.h / 2;
    CHECK(ctx, nx::windowPxToGridCell(cx, cy, play, 160, 90, gx, gy));
    CHECK(ctx, gx >= 70 && gx <= 90);
    CHECK(ctx, gy >= 40 && gy <= 50);

    CHECK(ctx, !nx::windowPxToGridCell(play.x - 1, cy, play, 160, 90, gx, gy));
    CHECK(ctx, !nx::windowPxToGridCell(cx, play.y - 1, play, 160, 90, gx, gy));

    const nx::PlayRegion fs =
        nx::getPlayRegionForScene(1280, 720, 160, 90, true, false);
    CHECK(ctx, fs.y > 0);
    CHECK(ctx, fs.h < full.h);
    CHECK(ctx, fs.y + fs.h <= 720);

    const nx::MenuListWindow top = nx::computeMenuListWindow(8, 0, 220.f, 46.f);
    CHECK(ctx, top.visibleRows >= 4);
    CHECK(ctx, top.visibleRows < 8);
    CHECK(ctx, top.firstRow == 0);

    const nx::MenuListWindow bottom = nx::computeMenuListWindow(8, 7, 220.f, 46.f);
    CHECK(ctx, bottom.visibleRows == top.visibleRows);
    CHECK(ctx, bottom.firstRow > 0);
    CHECK(ctx, bottom.firstRow + bottom.visibleRows >= 8);

    const nx::MenuListWindow perfTab = nx::computeMenuListWindow(7, 6, 180.f, 46.f);
    CHECK(ctx, perfTab.visibleRows >= 3);
    CHECK(ctx, perfTab.visibleRows < 7);
    CHECK(ctx, perfTab.firstRow > 0);
    CHECK(ctx, perfTab.firstRow + perfTab.visibleRows >= 7);

    const nx::PlayRegion frac =
        nx::getPlayRegionForScene(1280, 720, 480, 270, true, false);
    CHECK(ctx, frac.w > 0 && frac.h > 0);
    const float cellW = float(frac.w) / 480.f;
    const float cellH = float(frac.h) / 270.f;
    const int wantX = 120, wantY = 80;
    const int px = int(float(frac.x) + (float(wantX) + 0.5f) * cellW);
    const int py = int(float(frac.y) + (float(wantY) + 0.5f) * cellH);
    int rx = -1, ry = -1;
    CHECK(ctx, nx::windowPxToGridCell(px, py, frac, 480, 270, rx, ry));
    CHECK(ctx, rx == wantX && ry == wantY);

    nx::ActiveTileMap tiles;
    tiles.reset(128, 96);
    CHECK(ctx, tiles.activeCount() == 0);
    tiles.sleepAll();
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    CHECK(ctx, !tiles.activeBounds(x0, y0, x1, y1, 1));
    tiles.markDisk(64, 48, 4);
    CHECK(ctx, tiles.activeBounds(x0, y0, x1, y1, 1));
    CHECK(ctx, x0 <= 64 && x1 >= 64);
    CHECK(ctx, y0 <= 48 && y1 >= 48);
}
