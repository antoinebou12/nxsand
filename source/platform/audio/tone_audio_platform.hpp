#pragma once
#include <cstdint>

namespace nx::tonePlatform {

constexpr int kAudioSampleRate = 48000;

bool init();
void shutdown();
void setOutputPaused(bool paused);
bool deviceReady();

bool queueOneShot(const int16_t* interleavedStereo, int frameCount, float volumeScale = 1.f);

void setLoopSource(const int16_t* interleavedStereo, int frameCount);
void setLoopActive(bool active);
void setLoopVolume(float scale);
void setDuckUntilMs(uint32_t untilMs);
void tickOutput();

} // namespace nx::tonePlatform
