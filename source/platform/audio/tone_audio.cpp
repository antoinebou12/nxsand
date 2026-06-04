#include "tone_audio.hpp"
#include "menu_music.hpp"
#include "tone_audio_platform.hpp"
#include "wav_loader.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace nx {

namespace {

constexpr int kChannels = 2;

static std::atomic<SoundLevel> g_level{SoundLevel::Medium};
static uint32_t g_busyUntilMs = 0;
static uint32_t g_explosionCooldownUntilMs = 0;
static bool g_wavsLoaded = false;

struct ToneWav {
    WavPcm pcm;
    bool ok = false;
};

static std::array<ToneWav, 5> g_toneWavs{};

static bool audioDisabledByEnv() {
    const char* v = SDL_getenv("NXSAND_DISABLE_AUDIO");
    if (!v || !v[0]) v = SDL_getenv("NXENGINE_DISABLE_AUDIO");
    if (!v || !v[0]) return false;
    return v[0] == '1' || v[0] == 'y' || v[0] == 'Y' ||
           (std::strcmp(v, "true") == 0) || (std::strcmp(v, "TRUE") == 0);
}

static float volumeScale(SoundLevel level) {
    switch (level) {
        case SoundLevel::Off: return 0.f;
        case SoundLevel::Low: return 0.18f;
        case SoundLevel::Medium: return 0.30f;
        case SoundLevel::High: return 0.42f;
    }
    return 0.f;
}

static void applyLoopVolume(SoundLevel level) {
    const float master = volumeScale(level);
    constexpr float kThemeAtMedium = 0.42f;
    constexpr float kMediumScale = 0.30f;
    const float theme = (master <= 0.f) ? 0.f : master * (kThemeAtMedium / kMediumScale);
    tonePlatform::setLoopVolume(theme);
}

static bool isBusy() { return SDL_GetTicks() < g_busyUntilMs; }

static void markBusy(int durationMs) {
    const uint32_t until = SDL_GetTicks() + static_cast<uint32_t>(durationMs);
    if (until > g_busyUntilMs) g_busyUntilMs = until;
}

static int toneIndex(ToneId id) { return static_cast<int>(id); }

static bool loadToneWavs() {
    if (g_wavsLoaded) return true;
    const char* files[5] = {
        "ui_confirm.wav",
        "ui_back.wav",
        "ui_nav.wav",
        "ui_material.wav",
        "explosion_light.wav",
    };
    bool any = false;
    for (int i = 0; i < 5; ++i) {
        std::string err;
        g_toneWavs[static_cast<size_t>(i)].ok =
            loadRomfsWav(files[i], g_toneWavs[static_cast<size_t>(i)].pcm, &err);
        if (g_toneWavs[static_cast<size_t>(i)].ok) any = true;
    }
    g_wavsLoaded = any;
    return any;
}

static const WavPcm* wavFor(ToneId id, bool heavy) {
    if (id == ToneId::Explosion && heavy) {
        static WavPcm heavyPcm;
        static bool heavyLoaded = false;
        if (!heavyLoaded) {
            std::string err;
            heavyLoaded = loadRomfsWav("explosion_heavy.wav", heavyPcm, &err);
        }
        return heavyLoaded ? &heavyPcm : nullptr;
    }
    const int idx = toneIndex(id);
    if (idx < 0 || idx >= 5) return nullptr;
    const ToneWav& tw = g_toneWavs[static_cast<size_t>(idx)];
    return tw.ok ? &tw.pcm : nullptr;
}

static int durationMsFor(const WavPcm& pcm) {
    const int frames = static_cast<int>(pcm.samples.size() / kChannels);
    if (frames <= 0 || pcm.sampleRate <= 0) return 0;
    return (frames * 1000) / pcm.sampleRate;
}

} // namespace

bool toneAudioEnsureReady() {
    if (audioDisabledByEnv()) return false;
    if (!tonePlatform::init()) return false;
    loadToneWavs();
    applyLoopVolume(g_level.load(std::memory_order_relaxed));
    return tonePlatform::deviceReady();
}

bool toneAudioInit() { return toneAudioEnsureReady(); }

void toneAudioShutdown() {
    toneAudioReleaseCachedWavs();
    tonePlatform::shutdown();
}

void toneAudioReleaseCachedWavs() {
    g_wavsLoaded = false;
    for (auto& tw : g_toneWavs) {
        tw = ToneWav{};
    }
}

void toneAudioSetLevel(SoundLevel level) {
    g_level.store(level, std::memory_order_relaxed);
    applyLoopVolume(level);
}

void toneAudioSetOutputPaused(bool paused) { tonePlatform::setOutputPaused(paused); }

void playTone(ToneId id, bool heavy) {
    const SoundLevel level = g_level.load(std::memory_order_relaxed);
    const float masterVol = volumeScale(level);
    if (masterVol <= 0.f || audioDisabledByEnv()) return;
    if (!toneAudioEnsureReady() || !tonePlatform::deviceReady()) return;

    if (id == ToneId::Explosion) {
        const uint32_t now = SDL_GetTicks();
        if (now < g_explosionCooldownUntilMs) return;
        if (isBusy()) return;
        g_explosionCooldownUntilMs = now + 120;
    } else if (isBusy()) {
        // UI tones may cut in over a short tail.
    }

    const WavPcm* pcm = wavFor(id, heavy);
    if (!pcm || pcm->samples.empty()) return;

    const int frames = static_cast<int>(pcm->samples.size() / kChannels);
    if (frames <= 0) return;

    if (id != ToneId::Explosion) {
        menuMusicNotifyUiTone();
    }

    if (!tonePlatform::queueOneShot(pcm->samples.data(), frames, masterVol)) return;
    markBusy(durationMsFor(*pcm));
}

} // namespace nx
