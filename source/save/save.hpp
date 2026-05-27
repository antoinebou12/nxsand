#pragma once
#include <cstdint>
#include <string>
#include "save_paths.hpp"

namespace nx {
class App;

struct SlotMeta {
    bool empty = true;
    int64_t timestampMs = 0;
};

bool saveGame(App& app, int slot);
bool loadGame(App& app, int slot);
SlotMeta getSlotMeta(int slot);
} // namespace nx
