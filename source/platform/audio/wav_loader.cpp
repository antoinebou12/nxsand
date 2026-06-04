#include "wav_loader.hpp"
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>

namespace nx {

std::string romfsAudioPath(const char* filename) {
#if defined(__SWITCH__)
    return std::string("audio/") + filename;
#else
    return std::string("romfs/audio/") + filename;
#endif
}

bool loadWavFile(const std::string& path, WavPcm& out, std::string* err) {
    out = WavPcm{};
    SDL_AudioSpec spec{};
    Uint8* raw = nullptr;
    Uint32 rawLen = 0;
    if (!SDL_LoadWAV(path.c_str(), &spec, &raw, &rawLen)) {
        if (err) *err = std::string("SDL_LoadWAV failed: ") + path + " (" + SDL_GetError() + ")";
        return false;
    }
    if (spec.format != AUDIO_S16LSB || spec.channels != 2) {
        SDL_FreeWAV(raw);
        if (err) *err = "WAV must be 16-bit stereo: " + path;
        return false;
    }
    if (spec.freq != 48000) {
        SDL_FreeWAV(raw);
        if (err) *err = "WAV must be 48000 Hz: " + path;
        return false;
    }
    const size_t samples = rawLen / sizeof(int16_t);
    out.sampleRate = spec.freq;
    out.channels = spec.channels;
    out.samples.resize(samples);
    if (samples > 0) {
        std::memcpy(out.samples.data(), raw, rawLen);
    }
    SDL_FreeWAV(raw);
    return true;
}

bool loadRomfsWav(const char* filename, WavPcm& out, std::string* err) {
    if (loadWavFile(romfsAudioPath(filename), out, err)) return true;
#if defined(__SWITCH__)
    const std::string alt = std::string("romfs:/audio/") + filename;
    return loadWavFile(alt, out, err);
#else
    (void)filename;
    return false;
#endif
}

} // namespace nx
