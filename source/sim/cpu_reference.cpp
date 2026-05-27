#include "cpu_reference.hpp"

namespace nx {

void cpu_clear(std::vector<uint8_t>& g, int w, int h, Material m) {
    g.assign(static_cast<size_t>(w * h), static_cast<uint8_t>(m));
}

static uint8_t at(const std::vector<uint8_t>& g, int w, int h, int x, int y) {
    if (x < 0 || y < 0 || x >= w || y >= h) return static_cast<uint8_t>(MAT_WALL);
    return g[static_cast<size_t>(y * w + x)];
}

void cpu_margolus_sand_phase(std::vector<uint8_t>& g, int w, int h, int phaseX, int phaseY,
                              uint32_t /*frame*/) {
    std::vector<uint8_t> nxt = g;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if ((x & 1) != phaseX || (y & 1) != phaseY) continue;
            uint8_t self = at(g, w, h, x, y);
            uint8_t dn = at(g, w, h, x, y - 1);
            uint8_t outv = self;
            if (self == MAT_SAND) {
                auto fluid = [](uint8_t m) {
                    return m == MAT_WATER || m == MAT_OIL || m == MAT_LAVA || m == MAT_ACID;
                };
                if (dn == MAT_EMPTY || fluid(dn)) outv = MAT_EMPTY;
            }
            nxt[static_cast<size_t>(y * w + x)] = outv;
        }
    }
    g.swap(nxt);
}

} // namespace nx
