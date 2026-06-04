#include "menu_music.hpp"
#include "tone_audio_platform.hpp"
#include "wav_loader.hpp"
#include <SDL2/SDL.h>

namespace nx {

namespace {

WavPcm theme{};
bool loaded = false;
bool active = false;

float musicVolumeScale() { return 0.42f; }

} // namespace

bool menuMusicInit() {
    if (loaded) return true;
    std::string err;
    if (!loadRomfsWav("menu_theme.wav", theme, &err)) {
        return false;
    }
    if (theme.samples.empty()) return false;
    const int frames = static_cast<int>(theme.samples.size() / 2);
    tonePlatform::setLoopSource(theme.samples.data(), frames);
    tonePlatform::setLoopVolume(musicVolumeScale());
    loaded = true;
    return true;
}

void menuMusicShutdown() {
    tonePlatform::setLoopActive(false);
    theme = WavPcm{};
    loaded = false;
    active = false;
}

void menuMusicSetActive(bool on) {
    active = on && loaded;
    tonePlatform::setLoopActive(active);
}

void menuMusicTick() {
    if (!loaded || !active) return;
    tonePlatform::tickOutput();
}

void menuMusicNotifyUiTone() {
    tonePlatform::setDuckUntilMs(SDL_GetTicks() + 100u);
}

void menuMusicReleaseTheme() {
    tonePlatform::setLoopActive(false);
    tonePlatform::setLoopSource(nullptr, 0);
    theme = WavPcm{};
    loaded = false;
    active = false;
}

} // namespace nx
