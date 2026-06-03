#if !defined(__SWITCH__) && !defined(NX_DESKTOP)

#include "tone_audio_platform.hpp"

namespace nx::tonePlatform {

bool init() { return false; }
void shutdown() {}
void setOutputPaused(bool) {}
bool deviceReady() { return false; }
bool playPcm(const int16_t*, int, int) { return false; }

} // namespace nx::tonePlatform

#endif
