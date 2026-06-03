#include "tone_audio.hpp"
#include "tone_audio_platform.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

namespace nx {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr int kSampleRate = 48000;
constexpr int kChannels = 2;
struct ToneSpec {
    float freqHz;
    int durationMs;
    float ampScale;
};

static std::atomic<SoundLevel> g_level{SoundLevel::Medium};
static uint32_t g_busyUntilMs = 0;
static uint32_t g_explosionCooldownUntilMs = 0;

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

static ToneSpec specFor(ToneId id, bool heavy) {
    switch (id) {
        case ToneId::UiConfirm:
            return {660.f, 40, 1.f};
        case ToneId::UiBack:
            return {440.f, 35, 1.f};
        case ToneId::UiNav:
            return {520.f, 25, 0.85f};
        case ToneId::MaterialPick:
            return {880.f, 35, 1.f};
        case ToneId::Explosion:
            return {heavy ? 180.f : 210.f, heavy ? 110 : 85, heavy ? 1.f : 0.85f};
    }
    return {440.f, 30, 1.f};
}

static bool isBusy() {
    return SDL_GetTicks() < g_busyUntilMs;
}

static void markBusy(int durationMs) {
    const uint32_t until = SDL_GetTicks() + static_cast<uint32_t>(durationMs);
    if (until > g_busyUntilMs) g_busyUntilMs = until;
}

static int fillToneBuffer(int16_t* out, int maxFrames, const ToneSpec& spec, float masterVol) {
    const int frames =
        std::min(maxFrames, (spec.durationMs * kSampleRate) / 1000);
    if (frames <= 0 || masterVol <= 0.f) return 0;

    const float amp = 0.3f * 32767.f * spec.ampScale * masterVol;
    const int attackFrames = std::max(1, frames / 16);
    const int releaseFrames = std::max(1, frames / 10);

    for (int i = 0; i < frames; ++i) {
        float env = 1.f;
        if (i < attackFrames) env = float(i) / float(attackFrames);
        else if (i >= frames - releaseFrames)
            env = float(frames - i) / float(releaseFrames);

        const float t = float(i) / float(kSampleRate);
        const float sample = amp * env * std::sin(2.f * kPi * spec.freqHz * t);
        const int16_t s = static_cast<int16_t>(std::clamp(sample, -32767.f, 32767.f));
        out[i * kChannels + 0] = s;
        out[i * kChannels + 1] = s;
    }
    return frames;
}

} // namespace

bool toneAudioEnsureReady() {
    if (audioDisabledByEnv()) return false;
    return tonePlatform::init();
}

bool toneAudioInit() { return toneAudioEnsureReady(); }

void toneAudioShutdown() { tonePlatform::shutdown(); }

void toneAudioSetLevel(SoundLevel level) { g_level.store(level, std::memory_order_relaxed); }

void toneAudioSetOutputPaused(bool paused) { tonePlatform::setOutputPaused(paused); }

void playTone(ToneId id, bool heavy) {
    const SoundLevel level = g_level.load(std::memory_order_relaxed);
    const float masterVol = volumeScale(level);
    if (masterVol <= 0.f || audioDisabledByEnv()) return;
    if (!toneAudioEnsureReady() || !tonePlatform::deviceReady()) return;

    const ToneSpec spec = specFor(id, heavy);

    if (id == ToneId::Explosion) {
        const uint32_t now = SDL_GetTicks();
        if (now < g_explosionCooldownUntilMs) return;
        if (isBusy()) return;
        g_explosionCooldownUntilMs = now + 120;
    } else if (isBusy()) {
        // UI tones may cut in over a short tail.
    }

    const int maxFrames = (kSampleRate * 150) / 1000;
    std::vector<int16_t> pcm(static_cast<size_t>(maxFrames * kChannels), 0);
    const int frames = fillToneBuffer(pcm.data(), maxFrames, spec, masterVol);
    if (frames <= 0) return;

    if (!tonePlatform::playPcm(pcm.data(), frames, kSampleRate)) return;
    markBusy(spec.durationMs);
}

} // namespace nx
