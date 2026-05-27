#pragma once
#include <string>

namespace nx {

std::string saveDirectory();
std::string legacySaveDirectory();
bool ensureDirectoryExists(const std::string& path);

/// Write `data` to `finalPath` atomically: write to `finalPath.tmp`, flush, then rename.
/// Prevents truncated/corrupt save files when the Switch loses power or is sleep-yanked
/// mid-write (a real risk on handheld with unbuffered SD writes).
bool atomicWriteFile(const std::string& finalPath, const std::string& data);

#if defined(__SWITCH__)
// Prepare for JSON saves. SD is usually pre-mounted by libnx.
// Returns false only if the card truly is not reachable (game can still run).
bool ensureSwitchStorageReady();
#endif

/// Best-effort one-time migration from legacy `nxengine` save folder to `nxsand`.
void migrateLegacySaveData();

} // namespace nx
