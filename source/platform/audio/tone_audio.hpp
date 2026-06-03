#pragma once
#include "../../game/game_settings.hpp"

namespace nx {

enum class ToneId : int {
    UiConfirm = 0,
    UiBack,
    UiNav,
    MaterialPick,
    Explosion,
};

bool toneAudioInit();
bool toneAudioEnsureReady();
void toneAudioShutdown();
void toneAudioSetLevel(SoundLevel level);
void toneAudioSetOutputPaused(bool paused);
void playTone(ToneId id, bool heavy = false);

} // namespace nx
