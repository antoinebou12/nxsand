#if defined(NX_DESKTOP)

#include "tone_audio_platform.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <vector>

namespace nx::tonePlatform {

namespace {

SDL_AudioDeviceID deviceId = 0;
bool ready = false;
bool paused = false;

const int16_t* loopPcm = nullptr;
int loopFrames = 0;
int loopPos = 0;
std::atomic<bool> loopActive{false};
std::atomic<float> loopVolume{0.4f};
uint32_t duckUntilMs = 0;

constexpr int kChunkFrames = 2048;
std::vector<int16_t> mixChunk;

float effectiveLoopVolume() {
    float v = loopVolume.load(std::memory_order_relaxed);
    if (SDL_GetTicks() < duckUntilMs) v *= 0.7f;
    return v;
}

} // namespace

bool init() {
    if (ready) return true;
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    }

    SDL_AudioSpec want{};
    want.freq = kAudioSampleRate;
    want.format = AUDIO_S16LSB;
    want.channels = 2;
    want.samples = 1024;
    want.silence = 0;

    SDL_AudioSpec have{};
    deviceId = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (deviceId == 0) return false;

    mixChunk.resize(static_cast<size_t>(kChunkFrames * 2));
    SDL_PauseAudioDevice(deviceId, 0);
    ready = true;
    return true;
}

void shutdown() {
    loopPcm = nullptr;
    loopFrames = 0;
    loopPos = 0;
    loopActive.store(false, std::memory_order_relaxed);
    if (!ready) return;
    SDL_CloseAudioDevice(deviceId);
    deviceId = 0;
    ready = false;
}

void setOutputPaused(bool p) {
    paused = p;
    if (!ready) return;
    SDL_PauseAudioDevice(deviceId, paused ? 1 : 0);
}

bool deviceReady() { return ready; }

bool queueOneShot(const int16_t* interleavedStereo, int frameCount, float volumeScale) {
    if (!ready || paused || !interleavedStereo || frameCount <= 0 || volumeScale <= 0.f) return false;

    const int bytes = frameCount * 2 * static_cast<int>(sizeof(int16_t));
    const Uint32 queued = SDL_GetQueuedAudioSize(deviceId);
    const Uint32 maxQueued = static_cast<Uint32>(kAudioSampleRate) * 2u * sizeof(int16_t) / 2u;
    if (queued > maxQueued) return false;

    if (volumeScale >= 0.99f) {
        return SDL_QueueAudio(deviceId, interleavedStereo, static_cast<Uint32>(bytes)) == 0;
    }

    std::vector<int16_t> scaled(static_cast<size_t>(frameCount * 2));
    for (int i = 0; i < frameCount * 2; ++i) {
        scaled[static_cast<size_t>(i)] =
            static_cast<int16_t>(std::clamp(interleavedStereo[i] * volumeScale, -32767.f, 32767.f));
    }
    return SDL_QueueAudio(deviceId, scaled.data(), static_cast<Uint32>(bytes)) == 0;
}

void setLoopSource(const int16_t* interleavedStereo, int frameCount) {
    loopPcm = interleavedStereo;
    loopFrames = frameCount;
    loopPos = 0;
}

void setLoopActive(bool active) { loopActive.store(active, std::memory_order_relaxed); }

void setLoopVolume(float scale) { loopVolume.store(scale, std::memory_order_relaxed); }

void setDuckUntilMs(uint32_t untilMs) { duckUntilMs = untilMs; }

void tickOutput() {
    if (!ready || paused || !loopActive.load(std::memory_order_relaxed) || !loopPcm ||
        loopFrames <= 0) {
        return;
    }

    const Uint32 queued = SDL_GetQueuedAudioSize(deviceId);
    const Uint32 targetQueued = static_cast<Uint32>(kAudioSampleRate) * 2u * sizeof(int16_t) / 8u;
    if (queued > targetQueued) return;

    const float vol = effectiveLoopVolume();
    int written = 0;
    while (written < kChunkFrames) {
        if (loopPos >= loopFrames) loopPos = 0;
        const int16_t s = static_cast<int16_t>(loopPcm[loopPos * 2] * vol);
        mixChunk[static_cast<size_t>(written * 2)] = s;
        mixChunk[static_cast<size_t>(written * 2 + 1)] = s;
        ++loopPos;
        ++written;
    }

    const int bytes = written * 2 * static_cast<int>(sizeof(int16_t));
    SDL_QueueAudio(deviceId, mixChunk.data(), static_cast<Uint32>(bytes));
}

} // namespace nx::tonePlatform

#endif
