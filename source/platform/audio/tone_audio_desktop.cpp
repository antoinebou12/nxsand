#if defined(NX_DESKTOP)

#include "tone_audio_platform.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstring>
#include <vector>

namespace nx::tonePlatform {

namespace {

SDL_AudioDeviceID deviceId = 0;
bool ready = false;

} // namespace

bool init() {
    if (ready) return true;
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    }

    SDL_AudioSpec want{};
    want.freq = 48000;
    want.format = AUDIO_S16LSB;
    want.channels = 2;
    want.samples = 1024;
    want.silence = 0;

    SDL_AudioSpec have{};
    deviceId = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (deviceId == 0) return false;

    SDL_PauseAudioDevice(deviceId, 0);
    ready = true;
    return true;
}

void shutdown() {
    if (!ready) return;
    SDL_CloseAudioDevice(deviceId);
    deviceId = 0;
    ready = false;
}

void setOutputPaused(bool paused) {
    if (!ready) return;
    SDL_PauseAudioDevice(deviceId, paused ? 1 : 0);
}

bool deviceReady() { return ready; }

bool playPcm(const int16_t* interleavedStereo, int frameCount, int sampleRate) {
    if (!ready || !interleavedStereo || frameCount <= 0) return false;

    const int bytes =
        frameCount * 2 * sizeof(int16_t);
    const Uint32 queued = SDL_GetQueuedAudioSize(deviceId);
    const Uint32 maxQueued =
        static_cast<Uint32>(sampleRate) * static_cast<Uint32>(sizeof(int16_t)) * 2u / 4u;
    if (queued > maxQueued) return false;

    return SDL_QueueAudio(deviceId, interleavedStereo,
                          static_cast<Uint32>(bytes)) == 0;
}

} // namespace nx::tonePlatform

#endif
