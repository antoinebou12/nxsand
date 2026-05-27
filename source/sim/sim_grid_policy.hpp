// Handheld-first sim resolution: GPU grid can be smaller than the window and is
// nearest-upscaled in the play region (see docs/NATIVE.md, pickSimGridSize).
#pragma once
#include <cstdlib>
#include <utility>

namespace nx {

// Returns (sim_w, sim_h). 16:9 grids only; clamped to screen and minimum 160x90.
inline std::pair<int, int> pickSimGridSize(int screenW, int screenH) {
    if (screenW <= 0 || screenH <= 0) {
        return {640, 360};
    }
    const char* ew = std::getenv("NXSAND_SIM_W");
    const char* eh = std::getenv("NXSAND_SIM_H");
    if (!ew || !eh) {
        ew = std::getenv("NXENGINE_SIM_W");
        eh = std::getenv("NXENGINE_SIM_H");
    }
    if (ew && eh) {
        int w = std::atoi(ew);
        int h = std::atoi(eh);
        if (w >= 32 && h >= 32 && w <= screenW && h <= screenH) {
            return {w, h};
        }
    }

#if defined(__SWITCH__)
    const int maxDim = screenW > screenH ? screenW : screenH;
    // Handheld-first: 480x270 (fast) .. 960x540 (docked / stress).
    if (maxDim >= 1600) {
        return {960, 540};
    }
    if (maxDim >= 1200) {
        return {720, 405};
    }
    if (maxDim >= 900) {
        return {640, 360};
    }
    return {480, 270};
#else
    return {screenW, screenH};
#endif
}

} // namespace nx
