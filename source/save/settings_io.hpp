#pragma once
#include "../game/game_settings.hpp"

namespace nx {

bool loadGameSettings(GameSettings& out);
bool saveGameSettings(const GameSettings& settings);
void markGameSettingsDirty();
void flushGameSettingsIfDirty(const GameSettings& settings);

} // namespace nx
