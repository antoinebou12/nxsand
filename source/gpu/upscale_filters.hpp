#pragma once
#include "../game/game_settings.hpp"

namespace nx {

// GLSL upscale.frag uFilter values (UpscaleFilter::Nearest skips the upscale pass).
inline int upscaleFilterShaderId(UpscaleFilter f) {
    switch (f) {
        case UpscaleFilter::Tent: return 1;
        case UpscaleFilter::Mitchell: return 2;
        case UpscaleFilter::CatmullRom: return 3;
        case UpscaleFilter::Lanczos3: return 4;
        default: return 0;
    }
}

} // namespace nx
