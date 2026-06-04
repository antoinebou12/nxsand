#if defined(__SWITCH__)

#include "launch_log.hpp"
#include "../save/save_paths.hpp"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kLaunchLogPath = "sdmc:/switch/nxsand/launch.log";
constexpr const char* kLaunchLogOldPath = "sdmc:/switch/nxsand/launch.log.old";
constexpr long kLaunchLogRotateBytes = 256L * 1024L;

std::chrono::steady_clock::time_point g_launchLogT0 = std::chrono::steady_clock::now();
FILE* g_launchLogFile = nullptr;
bool g_launchLogFileDisabled = false;

bool verboseLaunchLog() {
    const char* v = std::getenv("NXSAND_VERBOSE_LAUNCH_LOG");
    if (!v || !v[0]) v = std::getenv("NXENGINE_VERBOSE_LAUNCH_LOG");
    if (!v || !v[0]) return false;
    if (v[0] == '0' && v[1] == '\0') return false;
    if (std::strcmp(v, "false") == 0 || std::strcmp(v, "FALSE") == 0) return false;
    if (std::strcmp(v, "off") == 0 || std::strcmp(v, "OFF") == 0) return false;
    return true;
}

bool launchLogFileEnabled() {
    const char* v = std::getenv("NXSAND_LAUNCH_LOG");
    if (!v || !v[0]) v = std::getenv("NXENGINE_LAUNCH_LOG");
    if (!v || !v[0]) return true;
    if (v[0] == '0' && v[1] == '\0') return false;
    if (std::strcmp(v, "false") == 0 || std::strcmp(v, "FALSE") == 0) return false;
    if (std::strcmp(v, "off") == 0 || std::strcmp(v, "OFF") == 0) return false;
    return true;
}

void ensureLaunchLogDirs() {
    static bool tried = false;
    if (tried) return;
    tried = true;
    nx::ensureSaveStorageAtLaunch();
}

void rotateLaunchLogIfNeeded() {
    FILE* probe = std::fopen(kLaunchLogPath, "rb");
    if (!probe) return;
    if (std::fseek(probe, 0, SEEK_END) != 0) {
        std::fclose(probe);
        return;
    }
    const long sz = std::ftell(probe);
    std::fclose(probe);
    if (sz < kLaunchLogRotateBytes) return;

    std::remove(kLaunchLogOldPath);
    std::rename(kLaunchLogPath, kLaunchLogOldPath);
}

FILE* openLaunchLogSession() {
    if (g_launchLogFileDisabled) return nullptr;
    if (g_launchLogFile) return g_launchLogFile;
    if (!launchLogFileEnabled()) {
        g_launchLogFileDisabled = true;
        return nullptr;
    }
    ensureLaunchLogDirs();
    rotateLaunchLogIfNeeded();
    g_launchLogFile = std::fopen(kLaunchLogPath, "a");
    return g_launchLogFile;
}

} // namespace

void resetLaunchLogTimer() {
    g_launchLogT0 = std::chrono::steady_clock::now();
}

void closeLaunchLog() {
    if (!g_launchLogFile) return;
    std::fflush(g_launchLogFile);
    std::fclose(g_launchLogFile);
    g_launchLogFile = nullptr;
}

void appendLaunchLog(const char* msg) {
    if (verboseLaunchLog() && msg) {
        std::fprintf(stderr, "[launch] %s\n", msg);
    }
    FILE* f = openLaunchLogSession();
    if (!f) return;
    std::fprintf(f, "%s\n", msg ? msg : "(null)");
    std::fflush(f);
}

void appendLaunchLogTimed(const char* msg) {
    if (!msg) return;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - g_launchLogT0)
                          .count();
    char buf[768];
    std::snprintf(buf, sizeof(buf), "[%lldms] %s", static_cast<long long>(ms), msg);
    appendLaunchLog(buf);
}

void appendLaunchLogf(const char* fmt, ...) {
    char buf[768];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    appendLaunchLog(buf);
}

#endif
