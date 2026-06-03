#pragma once
#include <cstdint>

namespace nx::tonePlatform {

bool init();
void shutdown();
void setOutputPaused(bool paused);
bool playPcm(const int16_t* interleavedStereo, int frameCount, int sampleRate);
bool deviceReady();

} // namespace nx::tonePlatform
