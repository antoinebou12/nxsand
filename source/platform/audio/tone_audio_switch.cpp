#if defined(__SWITCH__)

#include "tone_audio_platform.hpp"
#include <switch.h>
#include <cstdio>
#include <cstring>

namespace nx::tonePlatform {

namespace {

constexpr int kChannels = 2;
constexpr int kBytesPerSample = 2;
constexpr int kMaxFrames = (48000 * 150) / 1000;

AudioOutBuffer audoutBuf{};
u8* buffer = nullptr;
u32 bufferSize = 0;
bool ready = false;

void logAudio(const char* msg) {
    FILE* f = std::fopen("sdmc:/switch/nxsand/launch.log", "a");
    if (!f) return;
    std::fprintf(f, "%s\n", msg);
    std::fclose(f);
}

void logAudiof(const char* fmt, u32 rc) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), fmt, rc);
    logAudio(buf);
}

} // namespace

bool init() {
    if (ready) return true;

    const u32 dataSize =
        static_cast<u32>(kMaxFrames * kChannels * kBytesPerSample);
    bufferSize = (dataSize + 0xfff) & ~0xfffu;
    buffer = static_cast<u8*>(memalign(0x1000, bufferSize));
    if (!buffer) {
        logAudio("tone audio: memalign failed");
        return false;
    }
    std::memset(buffer, 0, bufferSize);

    Result rc = audoutInitialize();
    if (R_FAILED(rc)) {
        logAudiof("tone audio: audoutInitialize 0x%x", rc);
        free(buffer);
        buffer = nullptr;
        return false;
    }

    rc = audoutStartAudioOut();
    if (R_FAILED(rc)) {
        logAudiof("tone audio: audoutStartAudioOut 0x%x", rc);
        audoutExit();
        free(buffer);
        buffer = nullptr;
        return false;
    }

    ready = true;
    return true;
}

void setOutputPaused(bool) {}

void shutdown() {
    if (!ready) return;
    audoutStopAudioOut();
    audoutExit();
    if (buffer) {
        free(buffer);
        buffer = nullptr;
    }
    bufferSize = 0;
    ready = false;
}

bool deviceReady() { return ready; }

bool playPcm(const int16_t* interleavedStereo, int frameCount, int sampleRate) {
    (void)sampleRate;
    if (!ready || !buffer || !interleavedStereo || frameCount <= 0) return false;
    const u32 dataSize =
        static_cast<u32>(frameCount * kChannels * kBytesPerSample);
    if (dataSize > bufferSize) return false;

    std::memcpy(buffer, interleavedStereo, dataSize);

    audoutBuf.next = nullptr;
    audoutBuf.buffer = buffer;
    audoutBuf.buffer_size = bufferSize;
    audoutBuf.data_size = dataSize;
    audoutBuf.data_offset = 0;

    AudioOutBuffer* released = nullptr;
    const Result rc = audoutPlayBuffer(&audoutBuf, &released);
    if (R_FAILED(rc)) {
        logAudiof("tone audio: audoutPlayBuffer 0x%x", rc);
        return false;
    }
    return true;
}

} // namespace nx::tonePlatform

#endif
