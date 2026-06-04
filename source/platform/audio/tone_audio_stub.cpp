#if !defined(NX_DESKTOP) && !defined(__SWITCH__)

#include "tone_audio_platform.hpp"

namespace nx::tonePlatform {

bool init() { return false; }
void shutdown() {}
void setOutputPaused(bool) {}
bool deviceReady() { return false; }
bool queueOneShot(const int16_t*, int, float) { return false; }
void setLoopSource(const int16_t*, int) {}
void setLoopActive(bool) {}
void setLoopVolume(float) {}
void setDuckUntilMs(uint32_t) {}
void tickOutput() {}

} // namespace nx::tonePlatform

#endif
