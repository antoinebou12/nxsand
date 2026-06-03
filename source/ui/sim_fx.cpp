#include "sim_fx.hpp"
#include "../platform/audio/tone_audio.hpp"
#include "../sim/materials.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace nx {

static constexpr int kParticleCap = 160;
static constexpr int kMaxWatchCells = 80 * 80;

static struct {
    int count = 0;
    float x[kParticleCap];
    float y[kParticleCap];
    float vx[kParticleCap];
    float vy[kParticleCap];
    float rad[kParticleCap];
    float life[kParticleCap];
    float maxLife[kParticleCap];
    int kind[kParticleCap];
} particles;

static struct {
    bool valid = false;
    int x0 = 0;
    int y0 = 0;
    int x1 = -1;
    int y1 = -1;
} watch;

static std::vector<uint8_t> watchPrev;

static const float kSparkColors[3][3] = {
    {1.00f, 0.55f, 0.12f},
    {1.00f, 0.82f, 0.28f},
    {0.95f, 0.35f, 0.10f},
};
static const float kSmokeColors[2][3] = {
    {0.42f, 0.40f, 0.38f},
    {0.28f, 0.26f, 0.24f},
};

static void mergeWatch(int x0, int y0, int x1, int y1) {
    if (!watch.valid) {
        watch.x0 = x0;
        watch.y0 = y0;
        watch.x1 = x1;
        watch.y1 = y1;
        watch.valid = true;
        return;
    }
    watch.x0 = std::min(watch.x0, x0);
    watch.y0 = std::min(watch.y0, y0);
    watch.x1 = std::max(watch.x1, x1);
    watch.y1 = std::max(watch.y1, y1);
}

static void gridToScreen(int gx, int gy, const PlayRegion& pr, int gridW, int gridH, float& sx,
                         float& sy) {
    const float cellW = float(pr.w) / float(std::max(1, gridW));
    const float cellH = float(pr.h) / float(std::max(1, gridH));
    sx = float(pr.x) + (float(gx) + 0.5f) * cellW;
    sy = float(pr.y) + (float(gy) + 0.5f) * cellH;
}

static void spawnBurst(float sx, float sy, float cellSize, int intensity, bool heavy) {
    const int n = std::clamp(intensity, heavy ? 14 : 6, heavy ? 24 : 14);
    for (int p = 0; p < n && particles.count < kParticleCap; ++p) {
        const int i = particles.count++;
        const float angle = float(std::rand() % 6283) / 1000.f;
        const float speed = (2.5f + float(std::rand() % 100) / 100.f * 5.5f) * cellSize;
        particles.x[i] = sx + (float(std::rand() % 100) / 100.f - 0.5f) * cellSize * 0.6f;
        particles.y[i] = sy + (float(std::rand() % 100) / 100.f - 0.5f) * cellSize * 0.6f;
        particles.vx[i] = std::cos(angle) * speed;
        particles.vy[i] = std::sin(angle) * speed - cellSize * (1.2f + float(std::rand() % 50) / 100.f);
        particles.rad[i] = cellSize * (0.35f + float(std::rand() % 100) / 100.f * 0.85f);
        const float maxLife = heavy ? 22.f + float(std::rand() % 24) : 14.f + float(std::rand() % 20);
        particles.life[i] = maxLife;
        particles.maxLife[i] = maxLife;
        particles.kind[i] = std::rand() % 5;
    }
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
    particles.kind[i] = particles.kind[last];
}

void resetSimExplosionFx() {
    particles.count = 0;
    watch.valid = false;
    watchPrev.clear();
}

void notifySimExplosionWatch(int x0, int y0, int x1, int y1) {
    mergeWatch(x0, y0, x1, y1);
}

void expandSimExplosionWatch(int gridW, int gridH, int marginCells) {
    if (!watch.valid || gridW <= 0 || gridH <= 0) return;
    watch.x0 = std::max(0, watch.x0 - marginCells);
    watch.y0 = std::max(0, watch.y0 - marginCells);
    watch.x1 = std::min(gridW - 1, watch.x1 + marginCells);
    watch.y1 = std::min(gridH - 1, watch.y1 + marginCells);
}

void tickSimExplosionFx(SimPipeline& pipe, const PlayRegion& pr, int gridW, int gridH,
                        uint32_t simTick) {
    for (int i = particles.count - 1; i >= 0; --i) {
        particles.x[i] += particles.vx[i];
        particles.y[i] += particles.vy[i];
        particles.vy[i] += 0.18f;
        particles.vx[i] *= 0.96f;
        particles.life[i] -= 1.f;
        if (particles.life[i] <= 0.f) removeParticle(i);
    }

    if (!watch.valid || gridW <= 0 || gridH <= 0 || pr.w <= 0 || pr.h <= 0) return;
    if ((simTick & 1u) != 0u) return;

    const int w = watch.x1 - watch.x0 + 1;
    const int h = watch.y1 - watch.y0 + 1;
    if (w <= 0 || h <= 0 || w * h > kMaxWatchCells) return;

    std::vector<uint8_t> cur;
    if (!pipe.readRegionTopDown(watch.x0, watch.y0, w, h, cur)) return;

    const size_t n = cur.size();
    if (watchPrev.size() != n) watchPrev.assign(n, MAT_EMPTY);

    const float cellW = float(pr.w) / float(gridW);
    const float cellH = float(pr.h) / float(gridH);
    const float cellSize = std::max(cellW, cellH);

    bool detonated = false;
    bool heavyDetonation = false;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y * w + x);
            const uint8_t prev = watchPrev[i];
            const uint8_t now = cur[i];
            const bool fromGunpowder = prev == MAT_GUNPOWDER;
            const bool fromTnt = prev == MAT_TNT;
            if (!fromGunpowder && !fromTnt) continue;
            const bool gunpowderBurst =
                fromGunpowder && (now == MAT_FIRE || now == MAT_SMOKE);
            const bool tntBurst =
                fromTnt && (now == MAT_FIRE || now == MAT_EMPTY || now == MAT_SMOKE);
            if (!gunpowderBurst && !tntBurst) continue;
            detonated = true;
            if (now == MAT_FIRE) heavyDetonation = true;
            float sx = 0.f;
            float sy = 0.f;
            gridToScreen(watch.x0 + x, watch.y0 + y, pr, gridW, gridH, sx, sy);
            spawnBurst(sx, sy, cellSize, now == MAT_FIRE ? 18 : 10, now == MAT_FIRE);
        }
    }
    if (detonated) playTone(ToneId::Explosion, heavyDetonation);

    watchPrev = std::move(cur);
}

void drawSimExplosionFx(RenderPipeline& r, const PlayRegion& pr, int gridW, int gridH, int screenW,
                        int screenH) {
    if (particles.count <= 0 || pr.w <= 0 || pr.h <= 0) return;
    const float cellW = float(pr.w) / float(std::max(1, gridW));
    const float cellH = float(pr.h) / float(std::max(1, gridH));
    const float cellSize = std::max(cellW, cellH);

    for (int i = 0; i < particles.count; ++i) {
        const float life = particles.life[i];
        const float maxLife = particles.maxLife[i];
        const float t = std::min(1.f, life / std::max(1.f, maxLife * 0.35f));
        const float alpha = t * std::min(1.f, (maxLife - life) / 8.f + 0.25f);
        const int k = particles.kind[i];
        float cr = 0.f;
        float cg = 0.f;
        float cb = 0.f;
        if (k < 3) {
            cr = kSparkColors[k][0];
            cg = kSparkColors[k][1];
            cb = kSparkColors[k][2];
        } else {
            const int si = k - 3;
            cr = kSmokeColors[si % 2][0];
            cg = kSmokeColors[si % 2][1];
            cb = kSmokeColors[si % 2][2];
        }
        const float sx = particles.x[i];
        const float sy = particles.y[i];
        const float rad = particles.rad[i] * (0.6f + t * 0.5f);
        (void)cellSize;
        const float d = rad * 2.f;
        r.drawSolidRect(sx - rad, sy - rad, d, d, cr, cg, cb, alpha * 0.9f, screenW, screenH);
    }
}

} // namespace nx
