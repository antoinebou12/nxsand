#pragma once

#if defined(__SWITCH__)

void appendLaunchLog(const char* msg);
void appendLaunchLogf(const char* fmt, ...);
void resetLaunchLogTimer();
void appendLaunchLogTimed(const char* msg);
void closeLaunchLog();

#else

inline void appendLaunchLog(const char*) {}
template <typename... Args>
inline void appendLaunchLogf(const char*, Args...) {}
inline void resetLaunchLogTimer() {}
inline void appendLaunchLogTimed(const char* msg) { (void)msg; }
inline void closeLaunchLog() {}

#endif
