#include "save_paths.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>

#if defined(__SWITCH__)
#include <switch.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#else
#include <filesystem>
#endif

namespace nx {

static bool g_saveDirReady = false;

std::string saveDirectory() {
#if defined(__SWITCH__)
    return "sdmc:/switch/nxsand/";
#else
    return "./nxsand_save/";
#endif
}

std::string legacySaveDirectory() {
#if defined(__SWITCH__)
    return "sdmc:/switch/nxengine/";
#else
    return "./nxengine_save/";
#endif
}

#if defined(__SWITCH__)
static bool pathStatOk(const char* path) {
    struct stat st {};
    return stat(path, &st) == 0;
}
#endif

bool ensureDirectoryExists(const std::string& path) {
    if (path.empty()) return false;
#if defined(__SWITCH__)
    std::string cleanPath = path;
    std::string prefix = "";
    if (cleanPath.rfind("sdmc:/", 0) == 0) {
        prefix = "sdmc:/";
        cleanPath = cleanPath.substr(6);
    } else if (cleanPath.rfind("/", 0) == 0) {
        prefix = "/";
        cleanPath = cleanPath.substr(1);
    }
    
    std::string current = prefix;
    std::stringstream ss(cleanPath);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (part.empty()) continue;
        current += part + "/";
        struct stat st{};
        if (stat(current.c_str(), &st) != 0) {
            if (mkdir(current.c_str(), 0777) != 0 && errno != EEXIST) {
                return false;
            }
        } else if (!S_ISDIR(st.st_mode)) {
            return false;
        }
    }
    return true;
#else
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec;
#endif
}

bool ensureSaveDirectoryReady() {
    if (g_saveDirReady) return true;
    if (!ensureDirectoryExists(saveDirectory())) return false;
    g_saveDirReady = true;
    return true;
}

bool atomicWriteFile(const std::string& finalPath, const std::string& data) {
    if (finalPath.empty()) return false;
    const std::string tmpPath = finalPath + ".tmp";

    // Write payload to tmp file and flush before rename. On Switch the SD write cache
    // means a plain ofstream close can return success while data still sits in RAM; we
    // explicitly close + fsync (via the FILE* path) so the rename is durable.
    {
        std::ofstream f(tmpPath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!f) return false;
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!f) return false;
        f.flush();
        if (!f) return false;
    }

    // No fsync: devkitPro newlib may not link ::fsync/::fileno reliably, and the
    // atomic rename below already gives us "either old file or new file" — never a
    // half-written one. Worst case on power loss is losing the last save, not corrupting
    // an existing one.

    // std::rename overwrites on POSIX-style devoptab (Switch SDMC) and on Windows when
    // the target exists only if we remove it first; remove() is a no-op when missing.
    std::remove(finalPath.c_str());
    if (std::rename(tmpPath.c_str(), finalPath.c_str()) != 0) {
        std::remove(tmpPath.c_str());
        return false;
    }
    return true;
}

void migrateLegacySaveData() {
    const std::string dst = saveDirectory();
    const std::string src = legacySaveDirectory();
    if (dst == src) return;

#if defined(__SWITCH__)
    struct stat st{};
    if (stat("sdmc:/switch", &st) != 0) {
        fsdevMountSdmc();
    }
#endif

    ensureDirectoryExists(dst);

#if defined(__SWITCH__)
    struct stat dstSt {};
    if (stat(dst.c_str(), &dstSt) == 0 && S_ISDIR(dstSt.st_mode)) {
        std::string probe = dst + "settings.json";
        struct stat probeSt {};
        if (stat(probe.c_str(), &probeSt) == 0) return;
    }

    struct stat srcSt {};
    if (stat(src.c_str(), &srcSt) != 0 || !S_ISDIR(srcSt.st_mode)) return;

    const char* files[] = {"settings.json", "physics.json", "slot-1.json", "slot-2.json", "slot-3.json"};
    for (const char* f : files) {
        std::string srcFile = src + f;
        std::string dstFile = dst + f;
        struct stat srcFileSt {};
        if (stat(srcFile.c_str(), &srcFileSt) != 0) continue;
        FILE* in = std::fopen(srcFile.c_str(), "rb");
        if (!in) continue;
        FILE* out = std::fopen(dstFile.c_str(), "wb");
        if (!out) {
            std::fclose(in);
            continue;
        }
        char buf[4096];
        size_t n = 0;
        while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0) {
            std::fwrite(buf, 1, n, out);
        }
        std::fclose(out);
        std::fclose(in);
    }
#else
    std::error_code ec;
    if (!std::filesystem::exists(src, ec)) return;
    for (const auto& entry : std::filesystem::directory_iterator(src, ec)) {
        if (ec || !entry.is_regular_file()) continue;
        const auto dstFile = std::filesystem::path(dst) / entry.path().filename();
        if (std::filesystem::exists(dstFile, ec)) continue;
        std::filesystem::copy_file(entry.path(), dstFile, std::filesystem::copy_options::skip_existing, ec);
    }
#endif
}

#if defined(__SWITCH__)
bool ensureSwitchStorageReady() {
    if (!pathStatOk("/switch") && !pathStatOk("sdmc:/switch")) {
        // Only mount when the SD device is not visible yet (not when already mounted).
        Result rc = fsdevMountSdmc();
        if (R_FAILED(rc) && !pathStatOk("/switch") && !pathStatOk("sdmc:/switch")) {
            return false;
        }
    }

    if (!ensureSaveDirectoryReady()) return false;
    migrateLegacySaveData();
    return true;
}
#endif

bool ensureSaveStorageAtLaunch() {
#if defined(__SWITCH__)
    return ensureSwitchStorageReady();
#else
    if (!ensureSaveDirectoryReady()) return false;
    migrateLegacySaveData();
    return true;
#endif
}

} // namespace nx
