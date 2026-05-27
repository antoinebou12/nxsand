#include "haptics.hpp"
#include <SDL2/SDL.h>

namespace nx {

void pulsePaintRumble(InputState& in, const ControlSettings& controls) {
    if (!in.pad || controls.rumble == RumbleLevel::Off) return;
    Uint16 strength = 0;
    switch (controls.rumble) {
        case RumbleLevel::Low: strength = 8000; break;
        case RumbleLevel::Medium: strength = 16000; break;
        case RumbleLevel::High: strength = 28000; break;
        default: break;
    }
    SDL_GameControllerRumble(in.pad, strength, strength, 40);
}

} // namespace nx
