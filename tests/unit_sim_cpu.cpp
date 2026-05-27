#include "test_harness.hpp"
#include "sim/cpu_reference.hpp"
#include "sim/materials.hpp"
#include <vector>

void run_sim_cpu_tests(TestContext& ctx) {
    {
        const int w = 8, h = 8;
        std::vector<uint8_t> g(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_SAND));
        nx::cpu_clear(g, w, h, nx::MAT_EMPTY);
        for (uint8_t v : g) {
            CHECK(ctx, v == static_cast<uint8_t>(nx::MAT_EMPTY));
        }
    }

    {
        const int w = 16, h = 16;
        std::vector<uint8_t> g(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int cx = 8, cy = 8;
        g[static_cast<size_t>(cy * w + cx)] = static_cast<uint8_t>(nx::MAT_SAND);
        nx::cpu_margolus_sand_phase(g, w, h, 0, 0, 0);
        CHECK(ctx, g[static_cast<size_t>(cy * w + cx)] != static_cast<uint8_t>(nx::MAT_SAND));
    }

    {
        const int w = 16, h = 16;
        std::vector<uint8_t> g(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int cx = 9, cy = 8;
        g[static_cast<size_t>(cy * w + cx)] = static_cast<uint8_t>(nx::MAT_SAND);
        nx::cpu_margolus_sand_phase(g, w, h, 0, 0, 0);
        CHECK(ctx, g[static_cast<size_t>(cy * w + cx)] == static_cast<uint8_t>(nx::MAT_SAND));
    }

    {
        const int w = 8, h = 8;
        std::vector<uint8_t> g(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int x = 4, y = 4;
        g[static_cast<size_t>(y * w + x)] = static_cast<uint8_t>(nx::MAT_SAND);
        g[static_cast<size_t>((y - 1) * w + x)] = static_cast<uint8_t>(nx::MAT_WATER);
        nx::cpu_margolus_sand_phase(g, w, h, 0, 0, 0);
        CHECK(ctx, g[static_cast<size_t>(y * w + x)] != static_cast<uint8_t>(nx::MAT_SAND));
    }

    {
        const int w = 8, h = 8;
        std::vector<uint8_t> g(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int x = 4, y = 4;
        g[static_cast<size_t>(y * w + x)] = static_cast<uint8_t>(nx::MAT_SAND);
        g[static_cast<size_t>((y - 1) * w + x)] = static_cast<uint8_t>(nx::MAT_WALL);
        nx::cpu_margolus_sand_phase(g, w, h, 0, 0, 0);
        CHECK(ctx, g[static_cast<size_t>(y * w + x)] == static_cast<uint8_t>(nx::MAT_SAND));
    }
}
