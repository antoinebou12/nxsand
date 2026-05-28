#pragma once
#include "menu.hpp"

namespace nx::ui_copy {

void formatSubtitle(char* buf, size_t bufSize);
const char* menuNavHint(MenuScreen screen, bool hasEnteredPlay);
const char* playHudHint();
const char* materialRingHint();

} // namespace nx::ui_copy
