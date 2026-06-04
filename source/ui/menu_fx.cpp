#include "menu_fx.hpp"
#include "../gpu/render_pipeline.hpp"
#include <cmath>
#include <cstdlib>

namespace nx {

#if defined(__SWITCH__)
static constexpr int kStarCount = 48;
static constexpr int kParticleCap = 1;
#else
static constexpr int kStarCount = 130;
static constexpr int kParticleCap = 80;
#endif

static struct {
    bool ready = false;
    float x[kStarCount];
    float y[kStarCount];
    float r[kStarCount];
    float b[kStarCount];
} stars;

static struct {
    int count = 0;
    float x[kParticleCap];
    float y[kParticleCap];
    float vx[kParticleCap];
    float vy[kParticleCap];
    float rad[kParticleCap];
    float life[kParticleCap];
    float maxLife[kParticleCap];
    int color[kParticleCap];
} particles;

static const float kParticleColors[6][3] = {
    {0.37f, 0.92f, 0.83f},
    {0.78f, 0.63f, 0.31f},
    {0.23f, 0.60f, 0.91f},
    {1.00f, 0.31f, 0.19f},
    {0.63f, 0.31f, 1.00f},
    {0.31f, 0.91f, 0.47f},
};

static void ensureStars(int screenW, int screenH) {
    if (stars.ready) return;
    for (int i = 0; i < kStarCount; ++i) {
        stars.x[i] = float(std::rand() % std::max(1, screenW));
        stars.y[i] = float(std::rand() % std::max(1, screenH));
        stars.r[i] = 0.2f + float(std::rand() % 100) / 100.f * 1.0f;
        stars.b[i] = float(std::rand() % 1000) / 1000.f;
    }
    stars.ready = true;
}

static void spawnParticle(int screenW, int screenH) {
    if (particles.count >= kParticleCap) return;
    const int i = particles.count++;
    const float maxLife = 120.f + float(std::rand() % 200);
    particles.x[i] = float(std::rand() % std::max(1, screenW));
    particles.y[i] = float(screenH) + 10.f;
    particles.vx[i] = (float(std::rand() % 100) / 100.f - 0.5f) * 0.8f;
    particles.vy[i] = -(0.4f + float(std::rand() % 100) / 100.f * 0.8f);
    particles.rad[i] = 1.5f + float(std::rand() % 100) / 100.f * 2.5f;
    particles.life[i] = maxLife;
    particles.maxLife[i] = maxLife;
    particles.color[i] = std::rand() % 6;
}

static void removeParticle(int i) {
    const int last = --particles.count;
    if (i == last) return;
    particles.x[i] = particles.x[last];
    particles.y[i] = particles.y[last];
    particles.vx[i] = particles.vx[last];
    particles.vy[i] = particles.vy[last];
    particles.rad[i] = particles.rad[last];
    particles.life[i] = particles.life[last];
    particles.maxLife[i] = particles.maxLife[last];
    particles.color[i] = particles.color[last];
}

void tickMenuBackgroundFx(int tick, int screenW, int screenH) {
    ensureStars(screenW, screenH);
#if defined(__SWITCH__)
    (void)tick;
    return;
#endif
    if (particles.count < kParticleCap && tick % 3 == 0) spawnParticle(screenW, screenH);
    for (int i = particles.count - 1; i >= 0; --i) {
        particles.x[i] += particles.vx[i] + std::sin(tick * 0.02f + float(i)) * 0.15f;
        particles.y[i] += particles.vy[i];
        particles.life[i] -= 1.f;
        if (particles.life[i] <= 0.f || particles.y[i] < -10.f) removeParticle(i);
    }
}

static void drawDot(RenderPipeline& r, float cx, float cy, float radius, float cr, float cg, float cb,
                    float a, int W, int H) {
    const float d = radius * 2.f;
    r.drawSolidRect(cx - radius, cy - radius, d, d, cr, cg, cb, a, W, H);
}

void drawMenuBackgroundFx(RenderPipeline& r, int screenW, int screenH, int tick) {
    ensureStars(screenW, screenH);
    for (int i = 0; i < kStarCount; ++i) {
#if defined(__SWITCH__)
        const float tw = 0.18f + stars.b[i] * 0.20f;
#else
        const float tw = 0.15f + stars.b[i] * 0.5f * (0.6f + 0.4f * std::sin(tick * 0.018f + stars.b[i] * 9.f));
#endif
        const float sz = stars.r[i] * 2.f;
        drawDot(r, stars.x[i], stars.y[i], sz, 0.78f, 0.82f, 1.f, tw, screenW, screenH);
    }
    for (int i = 0; i < particles.count; ++i) {
        const float life = particles.life[i];
        const float maxLife = particles.maxLife[i];
        const float alpha =
            std::min(1.f, life / 40.f) * std::min(1.f, (maxLife - life) / 20.f + 0.3f) * 0.75f;
        const int ci = particles.color[i];
        drawDot(r, particles.x[i], particles.y[i], particles.rad[i], kParticleColors[ci][0],
                kParticleColors[ci][1], kParticleColors[ci][2], alpha, screenW, screenH);
    }
}

} // namespace nx
