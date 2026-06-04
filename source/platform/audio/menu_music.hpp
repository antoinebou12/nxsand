#pragma once

namespace nx {

bool menuMusicInit();
void menuMusicShutdown();
void menuMusicSetActive(bool active);
void menuMusicTick();
void menuMusicNotifyUiTone();
void menuMusicReleaseTheme();

} // namespace nx
