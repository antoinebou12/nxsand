#if defined(__SWITCH__)

#include "tone_audio_platform.hpp"
#include <SDL2/SDL.h>
#include <switch.h>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

namespace nx::tonePlatform {

namespace {

constexpr int kChannels = 2;
constexpr int kBytesPerSample = 2;
constexpr int kBufferCount = 3;
constexpr int kChunkFrames = 4096;
constexpr u32 kDataBytes =
    static_cast<u32>(kChunkFrames * kChannels * kBytesPerSample);
constexpr u32 kAlignedBufferBytes = (kDataBytes + 0xfffu) & ~0xfffu;

AudioOutBuffer audoutBufs[kBufferCount]{};
u8* buffers[kBufferCount] = {nullptr, nullptr, nullptr};
bool bufferQueued[kBufferCount] = {false, false, false};

const int16_t* loopPcm = nullptr;
int loopFrames = 0;
int loopPos = 0;
std::atomic<bool> loopActive{false};
std::atomic<float> loopVolume{0.4f};
uint32_t duckUntilMs = 0;
bool paused = false;
bool ready = false;

struct PendingOneShot {
    const int16_t* pcm = nullptr;
    int frames = 0;
    int pos = 0;
    float volume = 1.f;
};
PendingOneShot pending{};

void markReleased(AudioOutBuffer* released) {
    if (!released) return;
    for (int i = 0; i < kBufferCount; ++i) {
        if (released == &audoutBufs[i]) bufferQueued[i] = false;
    }
}

void fillChunk(u8* dst) {
    int16_t* out = reinterpret_cast<int16_t*>(dst);

    for (int i = 0; i < kChunkFrames; ++i) {
        float sample = 0.f;
        if (loopActive.load(std::memory_order_relaxed) && loopPcm && loopFrames > 0) {
            if (loopPos >= loopFrames) loopPos = 0;
            float vol = loopVolume.load(std::memory_order_relaxed);
            if (SDL_GetTicks() < duckUntilMs) vol *= 0.7f;
            sample += loopPcm[loopPos * 2] * vol;
            ++loopPos;
        }
        if (pending.pcm && pending.pos < pending.frames) {
            sample += pending.pcm[pending.pos * 2] * pending.volume;
            ++pending.pos;
            if (pending.pos >= pending.frames) pending = PendingOneShot{};
        }
        const int16_t s = static_cast<int16_t>(std::clamp(sample, -32767.f, 32767.f));
        out[i * 2] = s;
        out[i * 2 + 1] = s;
    }
}

bool tryQueueBuffer(int idx) {
    if (bufferQueued[idx] || !buffers[idx]) return false;
    fillChunk(buffers[idx]);

    audoutBufs[idx].next = nullptr;
    audoutBufs[idx].buffer = buffers[idx];
    audoutBufs[idx].buffer_size = kAlignedBufferBytes;
    audoutBufs[idx].data_size = kDataBytes;
    audoutBufs[idx].data_offset = 0;

    AudioOutBuffer* released = nullptr;
    const Result rc = audoutPlayBuffer(&audoutBufs[idx], &released);
    if (R_FAILED(rc)) return false;
    bufferQueued[idx] = true;
    markReleased(released);
    return true;
}

} // namespace

bool init() {
    if (ready) return true;

    for (int i = 0; i < kBufferCount; ++i) {
        buffers[i] = static_cast<u8*>(memalign(0x1000, kAlignedBufferBytes));
        if (!buffers[i]) return false;
        std::memset(buffers[i], 0, kAlignedBufferBytes);
    }

    Result rc = audoutInitialize();
    if (R_FAILED(rc)) {
        for (int i = 0; i < kBufferCount; ++i) {
            free(buffers[i]);
            buffers[i] = nullptr;
        }
        return false;
    }

    rc = audoutStartAudioOut();
    if (R_FAILED(rc)) {
        audoutExit();
        for (int i = 0; i < kBufferCount; ++i) {
            free(buffers[i]);
            buffers[i] = nullptr;
        }
        return false;
    }

    ready = true;
    return true;
}

void shutdown() {
    loopPcm = nullptr;
    loopFrames = 0;
    loopPos = 0;
    loopActive.store(false, std::memory_order_relaxed);
    pending = PendingOneShot{};
    if (!ready) return;
    audoutStopAudioOut();
    audoutExit();
    for (int i = 0; i < kBufferCount; ++i) {
        if (buffers[i]) free(buffers[i]);
        buffers[i] = nullptr;
        bufferQueued[i] = false;
    }
    ready = false;
}

void setOutputPaused(bool p) {
    paused = p;
    if (!ready) return;
    if (p) audoutStopAudioOut();
    else audoutStartAudioOut();
}

bool deviceReady() { return ready; }

bool queueOneShot(const int16_t* interleavedStereo, int frameCount, float volumeScale) {
    if (!ready || paused || !interleavedStereo || frameCount <= 0 || volumeScale <= 0.f) return false;
    pending.pcm = interleavedStereo;
    pending.frames = frameCount;
    pending.pos = 0;
    pending.volume = volumeScale;
    tickOutput();
    return true;
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
    if (!ready || paused) return;
    if (!loopActive.load(std::memory_order_relaxed) && !pending.pcm) return;
    for (int i = 0; i < kBufferCount; ++i) {
        if (!bufferQueued[i]) tryQueueBuffer(i);
    }
}

} // namespace nx::tonePlatform

#endif
