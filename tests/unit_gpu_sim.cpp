#include "test_harness.hpp"
#include "gpu_test_gl.hpp"
#include "gpu/sim_pipeline.hpp"
#include "gpu/gl_loader.hpp"
#include "sim/materials.hpp"
#include "game/game_settings.hpp"
#include "sim/physics_params.hpp"
#include <cstdlib>
#include <vector>

namespace {

int countMaterial(const std::vector<uint8_t>& g, int w, int h, nx::Material m) {
    int n = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (g[static_cast<size_t>(y * w + x)] == static_cast<uint8_t>(m)) {
                ++n;
            }
        }
    }
    return n;
}

bool readTopDown(nx::SimPipeline& pipe, int w, int h, std::vector<uint8_t>& top) {
    return pipe.readGridTo(top) && static_cast<int>(top.size()) == w * h;
}

uint8_t atTop(const std::vector<uint8_t>& g, int w, int x, int y) {
    return g[static_cast<size_t>(y * w + x)];
}

void setTop(std::vector<uint8_t>& g, int w, int x, int y, nx::Material m) {
    g[static_cast<size_t>(y * w + x)] = static_cast<uint8_t>(m);
}

} // namespace

void run_gpu_sim_tests(TestContext& ctx) {
    GpuTestGl gl;
    if (!gl.init()) {
        const char* skip = std::getenv("NXSAND_SKIP_GPU_TESTS");
        if (skip && (skip[0] == '1' || skip[0] == 'y' || skip[0] == 'Y')) {
            return;
        }
        CHECK(ctx, false);
        return;
    }

    {
        nx::SimPipeline bad;
        CHECK(ctx, !bad.init(16, 16, "shaders/__missing__", nx::SimBackend::Fragment));
    }

    const int w = 32;
    const int h = 32;
    nx::SimPipeline pipe;
    CHECK(ctx, pipe.init(w, h, gl.shaderDir, nx::SimBackend::Fragment));
    CHECK(ctx, pipe.readTexture() != 0u);

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> grid;
        CHECK(ctx, readTopDown(pipe, w, h, grid));
        CHECK(ctx, countMaterial(grid, w, h, nx::MAT_EMPTY) == w * h);
    }

    {
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        in[static_cast<size_t>(4 * w + 6)] = static_cast<uint8_t>(nx::MAT_SAND);
        in[static_cast<size_t>(10 * w + 12)] = static_cast<uint8_t>(nx::MAT_WALL);
        pipe.uploadGridTopDown(in, w, h);
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        CHECK(ctx, out.size() == in.size());
        CHECK(ctx, atTop(out, w, 6, 4) == static_cast<uint8_t>(nx::MAT_SAND));
        CHECK(ctx, atTop(out, w, 12, 10) == static_cast<uint8_t>(nx::MAT_WALL));
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        const int cx = w / 2;
        const int cy = h / 2;
        pipe.paintDisk(cx, cy, 2, nx::MAT_WATER);
        CHECK(ctx, pipe.sampleMaterial(cx, cy) == nx::MAT_WATER);
        std::vector<uint8_t> grid;
        CHECK(ctx, readTopDown(pipe, w, h, grid));
        CHECK(ctx, countMaterial(grid, w, h, nx::MAT_WATER) >= 5);
    }

    {
        pipe.clearAll(nx::MAT_WALL);
        nx::PhysicsParams physics{};
        pipe.step(0u, physics);
        std::vector<uint8_t> grid;
        CHECK(ctx, readTopDown(pipe, w, h, grid));
        CHECK(ctx, countMaterial(grid, w, h, nx::MAT_WALL) == w * h);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int sx = w / 2;
        const int sy = 4;
        in[static_cast<size_t>(sy * w + sx)] = static_cast<uint8_t>(nx::MAT_SAND);
        pipe.uploadGridTopDown(in, w, h);
        nx::PhysicsParams physics{};
        pipe.step(0u, physics);
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        const bool moved =
            atTop(out, w, sx, sy) != static_cast<uint8_t>(nx::MAT_SAND) ||
            atTop(out, w, sx, sy - 1) == static_cast<uint8_t>(nx::MAT_SAND);
        CHECK(ctx, moved);
    }

    {
        pipe.clearAll(nx::MAT_SAND);
        std::vector<uint8_t> grid;
        CHECK(ctx, readTopDown(pipe, w, h, grid));
        CHECK(ctx, countMaterial(grid, w, h, nx::MAT_SAND) == w * h);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        nx::PhysicsParams physics{};
        pipe.step(1u, physics);
        std::vector<uint8_t> grid;
        CHECK(ctx, readTopDown(pipe, w, h, grid));
        CHECK(ctx, countMaterial(grid, w, h, nx::MAT_EMPTY) == w * h);
        CHECK(ctx, pipe.lastPasses() == 4);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int sx = w / 2;
        const int sy = 12;
        for (int x = sx - 2; x <= sx + 2; ++x) {
            setTop(in, w, x, sy - 1, nx::MAT_WALL);
        }
        setTop(in, w, sx, sy, nx::MAT_SAND);
        pipe.uploadGridTopDown(in, w, h);
        nx::PhysicsParams physics{};
        pipe.step(0u, physics);
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        CHECK(ctx, atTop(out, w, sx, sy - 1) == static_cast<uint8_t>(nx::MAT_WALL));
        CHECK(ctx, atTop(out, w, sx, sy - 2) != static_cast<uint8_t>(nx::MAT_SAND));
        int sandInColumn = 0;
        for (int y = 0; y < h; ++y) {
            if (atTop(out, w, sx, y) == static_cast<uint8_t>(nx::MAT_SAND)) {
                ++sandInColumn;
            }
        }
        CHECK(ctx, sandInColumn >= 1);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int wx = w / 2;
        const int wy = 10;
        setTop(in, w, wx, wy, nx::MAT_WATER);
        pipe.uploadGridTopDown(in, w, h);
        nx::PhysicsParams physics{};
        physics.water_flowRate = 1.0f;
        pipe.step(0u, physics);
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        const bool waterMoved =
            atTop(out, w, wx, wy) != static_cast<uint8_t>(nx::MAT_WATER) ||
            atTop(out, w, wx, wy - 1) == static_cast<uint8_t>(nx::MAT_WATER);
        CHECK(ctx, waterMoved);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int sx = w / 2;
        const int sy = 4;
        setTop(in, w, sx, sy, nx::MAT_SAND);
        pipe.uploadGridTopDown(in, w, h);
        nx::PhysicsParams physics{};
        pipe.step(0u, physics);
        std::vector<uint8_t> afterOne;
        CHECK(ctx, readTopDown(pipe, w, h, afterOne));
        int yAfterOne = sy;
        for (int y = 0; y <= sy; ++y) {
            if (atTop(afterOne, w, sx, y) == static_cast<uint8_t>(nx::MAT_SAND)) {
                yAfterOne = y;
            }
        }
        pipe.step(1u, physics);
        std::vector<uint8_t> afterTwo;
        CHECK(ctx, readTopDown(pipe, w, h, afterTwo));
        int yAfterTwo = sy;
        for (int y = 0; y <= sy; ++y) {
            if (atTop(afterTwo, w, sx, y) == static_cast<uint8_t>(nx::MAT_SAND)) {
                yAfterTwo = y;
            }
        }
        CHECK(ctx, yAfterTwo <= yAfterOne);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        const int cx = 10;
        const int cy = 10;
        int dirtyW = 0;
        int dirtyH = 0;
        pipe.paintDisk(cx, cy, 3, nx::MAT_STONE, &dirtyW, &dirtyH);
        CHECK(ctx, dirtyW == 7);
        CHECK(ctx, dirtyH == 7);
        CHECK(ctx, pipe.sampleMaterial(cx, cy) == nx::MAT_STONE);
        std::vector<uint8_t> grid;
        CHECK(ctx, readTopDown(pipe, w, h, grid));
        CHECK(ctx, countMaterial(grid, w, h, nx::MAT_STONE) >= 25);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        const int cx = 12;
        const int cy = 12;
        pipe.paintDisk(cx, cy, 2, nx::MAT_SAND);
        pipe.paintDisk(cx, cy, 2, nx::MAT_EMPTY);
        std::vector<uint8_t> grid;
        CHECK(ctx, readTopDown(pipe, w, h, grid));
        CHECK(ctx, countMaterial(grid, w, h, nx::MAT_SAND) == 0);
        CHECK(ctx, atTop(grid, w, cx, cy) == static_cast<uint8_t>(nx::MAT_EMPTY));
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        CHECK(ctx, pipe.sampleMaterial(-1, 0) == nx::MAT_EMPTY);
        CHECK(ctx, pipe.sampleMaterial(0, -1) == nx::MAT_EMPTY);
        CHECK(ctx, pipe.sampleMaterial(w, 0) == nx::MAT_EMPTY);
        CHECK(ctx, pipe.sampleMaterial(0, h) == nx::MAT_EMPTY);
    }

    {
        constexpr int sw = 8;
        constexpr int sh = 8;
        std::vector<uint8_t> small(static_cast<size_t>(sw * sh), static_cast<uint8_t>(nx::MAT_EMPTY));
        setTop(small, sw, 2, 3, nx::MAT_LAVA);
        pipe.clearAll(nx::MAT_WALL);
        pipe.uploadGridTopDown(small, sw, sh);
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        CHECK(ctx, countMaterial(out, w, h, nx::MAT_LAVA) > 0);
        CHECK(ctx, countMaterial(out, w, h, nx::MAT_EMPTY) > 0);
        CHECK(ctx, countMaterial(out, w, h, nx::MAT_WALL) == 0);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        setTop(in, w, 5, 5, static_cast<nx::Material>(17));  // unknown id
        pipe.uploadGridTopDown(in, w, h);
        nx::PhysicsParams physics{};
        pipe.step(0u, physics);
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        CHECK(ctx, atTop(out, w, 5, 5) == static_cast<uint8_t>(nx::MAT_EMPTY));
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        setTop(in, w, 5, 5, nx::MAT_STEAM);
        pipe.uploadGridTopDown(in, w, h);
        nx::PhysicsParams physics{};
        pipe.step(0u, physics);
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        CHECK(ctx, countMaterial(out, w, h, nx::MAT_STEAM) > 0);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int sx = w / 2;
        const int sy = 8;
        setTop(in, w, sx, sy, nx::MAT_WALL);
        setTop(in, w, sx, sy - 1, nx::MAT_WALL);
        pipe.uploadGridTopDown(in, w, h);
        nx::PhysicsParams physics{};
        pipe.step(0u, physics);
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        CHECK(ctx, atTop(out, w, sx, sy) == static_cast<uint8_t>(nx::MAT_WALL));
        CHECK(ctx, atTop(out, w, sx, sy - 1) == static_cast<uint8_t>(nx::MAT_WALL));
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        pipe.paintDisk(4, 4, 4, nx::MAT_WATER);
        nx::PhysicsParams physics{};
        pipe.step(0u, physics, nx::ActiveTileMode::Conservative);
        CHECK(ctx, pipe.lastPasses() == 4);
        CHECK(ctx, pipe.lastActiveTileMode() == nx::ActiveTileMode::Conservative);
        CHECK(ctx, pipe.lastActiveTileCount() > 0);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        CHECK(ctx, pipe.activeTiles.activeCount() == 0);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        pipe.activeTiles.wakeAll();
        nx::PhysicsParams physics{};
        pipe.step(0u, physics, nx::ActiveTileMode::Conservative);
        CHECK(ctx, pipe.lastActiveTileFallback());
        CHECK(ctx, pipe.lastPasses() == 4);
    }

    {
        pipe.syncSimForSampling();
        const GLuint texAfterSync = pipe.readTexture();
        pipe.paintDisk(20, 20, 1, nx::MAT_OIL);
        CHECK(ctx, pipe.readTexture() != 0u);
        CHECK(ctx, texAfterSync != 0u);
        CHECK(ctx, pipe.sampleMaterial(20, 20) == nx::MAT_OIL);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int lx = w / 2;
        const int ly = h / 2;
        setTop(in, w, lx, ly, nx::MAT_LAVA);
        setTop(in, w, lx - 1, ly, nx::MAT_WALL);
        setTop(in, w, lx + 1, ly, nx::MAT_WALL);
        setTop(in, w, lx, ly - 1, nx::MAT_WALL);
        setTop(in, w, lx, ly + 1, nx::MAT_WALL);
        pipe.uploadGridTopDown(in, w, h);
        pipe.activeTiles.wakeAll();
        nx::PhysicsParams physics{};
        for (uint32_t f = 0; f < 120u; ++f) {
            pipe.step(f, physics);
        }
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        CHECK(ctx, atTop(out, w, lx - 1, ly) == static_cast<uint8_t>(nx::MAT_WALL));
        CHECK(ctx, atTop(out, w, lx + 1, ly) == static_cast<uint8_t>(nx::MAT_WALL));
        CHECK(ctx, atTop(out, w, lx, ly - 1) == static_cast<uint8_t>(nx::MAT_WALL));
        CHECK(ctx, atTop(out, w, lx, ly + 1) == static_cast<uint8_t>(nx::MAT_WALL));
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int wy = 14;
        const int x0 = 6;
        const int x1 = w - 7;
        for (int x = x0; x <= x1; ++x) {
            setTop(in, w, x, wy, nx::MAT_WALL);
            setTop(in, w, x, wy - 1, nx::MAT_FIRE);
        }
        pipe.uploadGridTopDown(in, w, h);
        pipe.activeTiles.wakeAll();
        nx::PhysicsParams physics{};
        physics.fire_speed = 1.0f;
        for (uint32_t f = 0; f < 120u; ++f) {
            pipe.step(f, physics);
        }
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        for (int x = x0; x <= x1; ++x) {
            CHECK(ctx, atTop(out, w, x, wy) == static_cast<uint8_t>(nx::MAT_WALL));
            CHECK(ctx, atTop(out, w, x, wy) != static_cast<uint8_t>(nx::MAT_FIRE));
        }
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int wy = 14;
        const int x0 = 6;
        const int x1 = w - 7;
        for (int x = x0; x <= x1; ++x) {
            setTop(in, w, x, wy, nx::MAT_WALL);
            setTop(in, w, x, wy - 1, nx::MAT_SMOKE);
        }
        pipe.uploadGridTopDown(in, w, h);
        pipe.activeTiles.wakeAll();
        nx::PhysicsParams physics{};
        physics.fire_speed = 1.0f;
        for (uint32_t f = 0; f < 120u; ++f) {
            pipe.step(f, physics);
        }
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        for (int x = x0; x <= x1; ++x) {
            CHECK(ctx, atTop(out, w, x, wy) == static_cast<uint8_t>(nx::MAT_WALL));
            CHECK(ctx, atTop(out, w, x, wy) != static_cast<uint8_t>(nx::MAT_SMOKE));
        }
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int fx = w / 2;
        const int fy = h / 2;
        setTop(in, w, fx, fy, nx::MAT_FIRE);
        pipe.uploadGridTopDown(in, w, h);
        pipe.activeTiles.wakeAll();
        nx::PhysicsParams physics{};
        for (uint32_t f = 0; f < 30u; ++f) {
            pipe.step(f, physics);
        }
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        const int gasCount =
            countMaterial(out, w, h, nx::MAT_FIRE) + countMaterial(out, w, h, nx::MAT_SMOKE);
        CHECK(ctx, gasCount < 20);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int py = 16;
        const int plantX0 = 8;
        setTop(in, w, plantX0 - 2, py, nx::MAT_WALL);
        setTop(in, w, plantX0 - 1, py, nx::MAT_FIRE);
        setTop(in, w, plantX0, py, nx::MAT_PLANT);
        setTop(in, w, plantX0 + 1, py, nx::MAT_WALL);
        setTop(in, w, plantX0 - 1, py - 1, nx::MAT_WALL);
        setTop(in, w, plantX0, py - 1, nx::MAT_WALL);
        setTop(in, w, plantX0 - 1, py + 1, nx::MAT_WALL);
        setTop(in, w, plantX0, py + 1, nx::MAT_WALL);
        pipe.uploadGridTopDown(in, w, h);
        pipe.activeTiles.wakeAll();
        nx::PhysicsParams physics{};
        physics.fire_ignitePlant = 1.0f;
        physics.fire_smokeRate = 0.0f;
        pipe.step(0u, physics);
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        CHECK(ctx, atTop(out, w, plantX0, py) == static_cast<uint8_t>(nx::MAT_FIRE));
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int py = 16;
        const int plantX0 = 8;
        const int plantCount = w - 4 - plantX0;
        for (int x = plantX0; x < w - 4; ++x) {
            setTop(in, w, x, py, nx::MAT_PLANT);
        }
        setTop(in, w, plantX0 - 1, py, nx::MAT_FIRE);
        pipe.uploadGridTopDown(in, w, h);
        pipe.activeTiles.wakeAll();
        nx::PhysicsParams physics{};
        physics.fire_ignitePlant = 0.0f;
        for (uint32_t f = 0; f < 12u; ++f) {
            pipe.step(f, physics);
        }
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        CHECK(ctx, countMaterial(out, w, h, nx::MAT_PLANT) == plantCount);
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int py = 16;
        const int plantX0 = 8;
        setTop(in, w, plantX0 - 1, py, nx::MAT_SMOKE);
        setTop(in, w, plantX0, py, nx::MAT_PLANT);
        pipe.uploadGridTopDown(in, w, h);
        pipe.activeTiles.wakeAll();
        nx::PhysicsParams physics{};
        physics.fire_ignitePlant = 1.0f;
        physics.smoke_fadeRate = 0.0f;
        pipe.step(0u, physics);
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        CHECK(ctx, atTop(out, w, plantX0, py) == static_cast<uint8_t>(nx::MAT_PLANT));
    }

    {
        pipe.clearAll(nx::MAT_EMPTY);
        std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        constexpr int poolW = 5;
        constexpr int poolH = 5;
        const int ox = (w - poolW) / 2;
        const int oy = (h - poolH) / 2;
        for (int y = oy; y < oy + poolH; ++y) {
            for (int x = ox; x < ox + poolW; ++x) {
                setTop(in, w, x, y, nx::MAT_OIL);
            }
        }
        setTop(in, w, ox - 1, oy + poolH / 2, nx::MAT_FIRE);
        pipe.uploadGridTopDown(in, w, h);
        pipe.activeTiles.wakeAll();
        nx::PhysicsParams physics{};
        for (uint32_t f = 0; f < 6u; ++f) {
            pipe.step(f, physics);
        }
        std::vector<uint8_t> out;
        CHECK(ctx, readTopDown(pipe, w, h, out));
        CHECK(ctx, countMaterial(out, w, h, nx::MAT_FIRE) >= 4);
    }

    pipe.shutdown();

    {
        std::string computeErr;
        const bool computeOk = nx::gl::check_compute_support(&computeErr);
        if (computeOk) {
            nx::SimPipeline comp;
            CHECK(ctx, comp.init(w, h, gl.shaderDir, nx::SimBackend::Compute));
            CHECK(ctx, comp.backend() == nx::SimBackend::Compute);
            comp.clearAll(nx::MAT_EMPTY);
            std::vector<uint8_t> in(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
            const int sx = w / 2;
            const int sy = 4;
            in[static_cast<size_t>(sy * w + sx)] = static_cast<uint8_t>(nx::MAT_SAND);
            comp.uploadGridTopDown(in, w, h);
            nx::PhysicsParams physics{};
            comp.step(0u, physics);
            std::vector<uint8_t> out;
            CHECK(ctx, readTopDown(comp, w, h, out));
            const bool moved =
                atTop(out, w, sx, sy) != static_cast<uint8_t>(nx::MAT_SAND) ||
                atTop(out, w, sx, sy - 1) == static_cast<uint8_t>(nx::MAT_SAND);
            CHECK(ctx, moved);
            comp.shutdown();
        }
    }

    gl.shutdown();
}
