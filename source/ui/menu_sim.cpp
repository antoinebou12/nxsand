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
    uploadTexture(0, true);
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
    for (int x = 0; x < kMenuSimW; ++x) {
        grid_[idx(x, kMenuSimH - 1)] = MAT_WALL;
    }
    for (int x = 10; x < kMenuSimW - 10; ++x) {
        grid_[idx(x, kMenuSimH - 6)] = MAT_LAVA;
        if (x % 5 == 0) grid_[idx(x, kMenuSimH - 7)] = MAT_LAVA;
    }
    for (int x = 22; x < 42; x += 3) {
        grid_[idx(x, 3 + (x & 1))] = MAT_SAND;
    }
    grid_[idx(10, kMenuSimH - 13)] = MAT_PLANT;
    grid_[idx(kMenuSimW - 12, kMenuSimH - 11)] = MAT_FIRE;
    seeded_ = true;
}

void MenuSim::step() {
    for (int y = kMenuSimH - 2; y >= 0; --y) {
        for (int x = 1; x < kMenuSimW - 1; ++x) {
            const int i = idx(x, y);
            const unsigned char m = grid_[i];
            if (m != MAT_SAND && m != MAT_WATER && m != MAT_SMOKE && m != MAT_FIRE) continue;

            if ((m == MAT_SAND || m == MAT_WATER) && grid_[idx(x, y + 1)] == MAT_EMPTY) {
                std::swap(grid_[i], grid_[idx(x, y + 1)]);
                continue;
            }
            if (m == MAT_SAND) {
                const int dir = ((frameAccum_ + x + y) & 1) ? 1 : -1;
                if (grid_[idx(x + dir, y + 1)] == MAT_EMPTY) {
                    std::swap(grid_[i], grid_[idx(x + dir, y + 1)]);
                }
                continue;
            }
            if (m == MAT_WATER) {
                const int dir = ((frameAccum_ + x) & 1) ? 1 : -1;
                if (grid_[idx(x + dir, y)] == MAT_EMPTY) {
                    std::swap(grid_[i], grid_[idx(x + dir, y)]);
                }
                continue;
            }
            if ((m == MAT_SMOKE || m == MAT_FIRE) && y > 1 && grid_[idx(x, y - 1)] == MAT_EMPTY) {
                std::swap(grid_[i], grid_[idx(x, y - 1)]);
            }
        }
    }

    const int dropX = 12 + ((frameAccum_ * 7) % (kMenuSimW - 24));
    if (grid_[idx(dropX, 2)] == MAT_EMPTY) grid_[idx(dropX, 2)] = MAT_SAND;
    if ((frameAccum_ & 7) == 0 && grid_[idx(kMenuSimW - 14, kMenuSimH - 13)] == MAT_EMPTY) {
        grid_[idx(kMenuSimW - 14, kMenuSimH - 13)] = MAT_SMOKE;
    }
}

void MenuSim::uploadTexture(int animTick, bool flickerEnabled) {
    if (tex_ == 0) return;
    const auto pal = build_palette();
    for (int y = 0; y < kMenuSimH; ++y) {
        for (int x = 0; x < kMenuSimW; ++x) {
            const int i = idx(x, y);
            const Material m = static_cast<Material>(grid_[i]);
            uint32_t c = pal[grid_[i]];
            if (m == MAT_LAVA && flickerEnabled) {
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

void MenuSim::tick(int animTick, bool flickerEnabled) {
    if (!seeded_) seed();
    ++frameAccum_;
    if ((frameAccum_ % 2) == 0) step();
    if ((frameAccum_ % 3) != 0) return;
    uploadTexture(animTick, flickerEnabled);
}

void MenuSim::draw(RenderPipeline& r, int screenW, int screenH, float /*uiScale*/) {
    if (tex_ == 0) return;
    const float aspect = float(kMenuSimW) / float(kMenuSimH);
    float drawW = float(screenW);
    float drawH = drawW / aspect;
    if (drawH < float(screenH)) {
        drawH = float(screenH);
        drawW = drawH * aspect;
    }
    const float drawX = (float(screenW) - drawW) * 0.5f;
    const float drawY = (float(screenH) - drawH) * 0.5f;
#if defined(__SWITCH__)
    const float alpha = 0.20f;
#else
    const float alpha = 0.24f;
#endif
    r.drawTexturedRect(drawX, drawY, drawW, drawH, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f, 1.f, alpha, screenW,
                       screenH, tex_);
}

} // namespace nx
