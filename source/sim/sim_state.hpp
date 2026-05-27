// Simulation grid constants and shared runtime state.
//
// The JS reference (materials.ts / grid.ts) used a 160x90 grid. On Switch the
// GPU sim grid is often smaller than the window (see sim_grid_policy.hpp) and
// is nearest-upscaled in the play region. Saves at other resolutions upscale
// via uploadGridTopDown.
#pragma once
#include <cstdint>
#include <vector>
#include "materials.hpp"

namespace nx {

// Legacy defaults (tests); runtime grid follows screenW x screenH.
constexpr int SIM_W = 512;
constexpr int SIM_H = 288;

// nxsand reference save dimensions.
constexpr int NXSAND_CLASSIC_W = 160;
constexpr int NXSAND_CLASSIC_H = 90;

constexpr int RENDER_W = 1280;
constexpr int RENDER_H = 720;

inline void flipGlRowsToTopDown(std::vector<uint8_t>& v, int w, int h) {
    for (int y = 0; y < h / 2; ++y) {
        const int y2 = h - 1 - y;
        for (int x = 0; x < w; ++x) {
            std::swap(v[static_cast<size_t>(y * w + x)], v[static_cast<size_t>(y2 * w + x)]);
        }
    }
}

struct SimState {
    int   grid_w = RENDER_W;
    int   grid_h = RENDER_H;
    uint32_t tick = 0;
    bool  paused = false;
    bool  sleeping = false;
    bool  paletteHidden = true;

    // Brush
    int      brush_x = RENDER_W / 2;
    int      brush_y = RENDER_H / 2;
    int      brush_radius = 3;
    Material brush_mat = MAT_SAND;
    bool     painting = false;
    bool     erasing  = false;

    int prev_brush_x = -1;
    int prev_brush_y = -1;
};

} // namespace nx
