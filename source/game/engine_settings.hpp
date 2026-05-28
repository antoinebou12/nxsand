#pragma once
#include <cstddef>
#include "game_settings.hpp"

namespace nx {

class App;

enum class EngineTab {
    Performance = 0,
    Visuals = 1,
    Controls = 2,
    Accessibility = 3,
    Display = 4,
    Debug = 5,
    Count = 6,
};

const char* engineTabLabel(EngineTab tab);
int engineTabRowCount(EngineTab tab);
const char* engineTabRowLabel(EngineTab tab, int row, const GameSettings& settings,
                              bool computeSimSupported, char* buf, size_t bufSize);
void adjustEngineTabRow(App& app, EngineTab tab, int row, int dir);

void applySettingsToRuntime(App& app);

} // namespace nx
