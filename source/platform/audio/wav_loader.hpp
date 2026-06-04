#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nx {

struct WavPcm {
    int sampleRate = 48000;
    int channels = 2;
    std::vector<int16_t> samples;
};

bool loadWavFile(const std::string& path, WavPcm& out, std::string* err = nullptr);

/// Resolve `audio/<filename>` on Switch (cwd is romfs:/) or `romfs/audio/<filename>` on desktop.
std::string romfsAudioPath(const char* filename);

bool loadRomfsWav(const char* filename, WavPcm& out, std::string* err = nullptr);

} // namespace nx
