#pragma once
#include <cstddef>
#include "game_settings.hpp"

namespace nx {

class App;

enum class EngineTab {
    Performance = 0,
    Visuals = 1,
    Controls = 2,
    Audio = 3,
    Accessibility = 4,
    Display = 5,
    Debug = 6,
    Count = 7,
};

const char* engineTabLabel(EngineTab tab);
int engineTabRowCount(EngineTab tab);
const char* engineTabRowLabel(EngineTab tab, int row, const GameSettings& settings,
                              char* buf, size_t bufSize);
void adjustEngineTabRow(App& app, EngineTab tab, int row, int dir);
bool engineTabRowIsToggle(EngineTab tab, int row);

void applySettingsToRuntime(App& app);

} // namespace nx
