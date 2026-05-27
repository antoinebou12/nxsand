#include "menu_sim.hpp"
#include "../gpu/render_pipeline.hpp"
#include "../sim/materials.hpp"
#include <algorithm>

namespace nx {

static constexpr int kMenuSimW = 64;
static constexpr int kMenuSimH = 36;

static int idx(int x, int y) { return y * kMenuSimW + x; }

bool MenuSim::init() {
    if (tex_ != 0) return true;
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    seed();
    uploadTexture(0);
    return tex_ != 0;
}

void MenuSim::shutdown() {
    if (tex_ != 0) {
        glDeleteTextures(1, &tex_);
        tex_ = 0;
    }
    seeded_ = false;
    texAllocated_ = false;
}

void MenuSim::seed() {
    std::fill(std::begin(grid_), std::end(grid_), MAT_EMPTY);
    for (int x = 8; x < kMenuSimW - 8; ++x) {
        grid_[idx(x, 2)] = MAT_WATER;
        if (x % 9 == 0) grid_[idx(x, 1)] = MAT_SAND;
    }
    for (int x = 20; x < 44; ++x) grid_[idx(x, kMenuSimH - 6)] = MAT_LAVA;
    grid_[idx(10, 8)] = MAT_PLANT;
    grid_[idx(kMenuSimW - 12, 10)] = MAT_FIRE;
    seeded_ = true;
}

void MenuSim::uploadTexture(int animTick) {
    if (tex_ == 0) return;
    const auto pal = build_palette();
    for (int y = 0; y < kMenuSimH; ++y) {
        for (int x = 0; x < kMenuSimW; ++x) {
            const int i = idx(x, y);
            const Material m = static_cast<Material>(grid_[i]);
            uint32_t c = pal[grid_[i]];
            if (m == MAT_LAVA) {
                int r = int(c & 0xff);
                int g = int((c >> 8) & 0xff);
                int b = int((c >> 16) & 0xff);
                const int pulse = 12 + ((animTick + x) & 7) * 2;
                r = std::min(255, r + pulse);
                g = std::min(255, g + (pulse >> 1));
                c = 0xff000000u | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
            } else if (m == MAT_EMPTY) {
                c = 0xc8000000u;
            }
            pixels_[i] = c;
        }
    }
    glBindTexture(GL_TEXTURE_2D, tex_);
    if (!texAllocated_) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kMenuSimW, kMenuSimH, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixels_);
        texAllocated_ = true;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kMenuSimW, kMenuSimH, GL_RGBA, GL_UNSIGNED_BYTE,
                        pixels_);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MenuSim::tick(int animTick) {
    if (!seeded_) seed();
    // Static decorative grid: only refresh RGBA for lava shimmer (no CPU sand stepping).
    if (++frameAccum_ < 3) return;
    frameAccum_ = 0;
    uploadTexture(animTick);
}

void MenuSim::draw(RenderPipeline& r, int screenW, int screenH, float /*uiScale*/) {
    if (tex_ == 0) return;
    const float aspect = float(kMenuSimW) / float(kMenuSimH);
    // Keep decorative sim in the top band so it does not bleed through the menu panel.
    const float maxH = float(screenH) * 0.32f;
    const float maxW = float(screenW) * 0.68f;
    float drawH = maxH;
    float drawW = drawH * aspect;
    if (drawW > maxW) {
        drawW = maxW;
        drawH = drawW / aspect;
    }
    const float drawX = (float(screenW) - drawW) * 0.5f;
    const float drawY = float(screenH) * 0.05f;
    // Backdrop sim is decorative — bump alpha so users can actually see what
    // the engine looks like in motion behind the menu chrome.
#if defined(__SWITCH__)
    const float alpha = 0.22f;
#else
    const float alpha = 0.26f;
#endif
    r.drawTexturedRect(drawX, drawY, drawW, drawH, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f, 1.f, alpha, screenW,
                       screenH, tex_);
}

} // namespace nx
