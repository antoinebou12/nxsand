#pragma once
// Active tile tracking: marks brush-woken regions for conservative fragment-pass scissoring.
#include <algorithm>
#include <cstdint>
#include <vector>
#include "../game/game_settings.hpp"
#include "../sim/materials.hpp"

namespace nx {

struct ActiveTileRun {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

class ActiveTileMap {
public:
    static constexpr int kTileSize = 32;

    void reset(int gridW, int gridH) {
        gw_ = std::max(1, gridW);
        gh_ = std::max(1, gridH);
        tw_ = (gw_ + kTileSize - 1) / kTileSize;
        th_ = (gh_ + kTileSize - 1) / kTileSize;
        tiles_.assign(static_cast<size_t>(tw_ * th_), 0u);
        unchangedFrames_.assign(tiles_.size(), 0u);
        next_.assign(tiles_.size(), 0u);
        nextFrames_.assign(tiles_.size(), 0u);
        hasRememberedBounds_ = false;
    }

    void wakeAll() {
        std::fill(tiles_.begin(), tiles_.end(), 1u);
        std::fill(unchangedFrames_.begin(), unchangedFrames_.end(), 0u);
    }

    void sleepAll() {
        std::fill(tiles_.begin(), tiles_.end(), 0u);
        std::fill(unchangedFrames_.begin(), unchangedFrames_.end(), 0u);
        hasRememberedBounds_ = false;
    }

    void wakeFromGridTopDown(const uint8_t* cells, int w, int h) {
        if (!cells || w <= 0 || h <= 0 || tiles_.empty()) return;
        sleepAll();
        for (int y = 0; y < gh_; ++y) {
            int sy = (y * h) / gh_;
            if (sy >= h) sy = h - 1;
            for (int x = 0; x < gw_; ++x) {
                int sx = (x * w) / gw_;
                if (sx >= w) sx = w - 1;
                if (cells[static_cast<size_t>(sy * w + sx)] == MAT_EMPTY) continue;
                const int tx = x / kTileSize;
                const int ty = y / kTileSize;
                wakeTile(tx, ty);
            }
        }
    }

    void wakeBoundsPx(int x0, int y0, int x1, int y1, int expandTiles = 0) {
        if (tiles_.empty()) return;
        const int tx0 = std::max(0, x0 / kTileSize - expandTiles);
        const int ty0 = std::max(0, y0 / kTileSize - expandTiles);
        const int tx1 = std::min(tw_ - 1, x1 / kTileSize + expandTiles);
        const int ty1 = std::min(th_ - 1, y1 / kTileSize + expandTiles);
        for (int ty = ty0; ty <= ty1; ++ty) {
            for (int tx = tx0; tx <= tx1; ++tx) {
                wakeTile(tx, ty);
            }
        }
    }

    void rememberBounds(int x0, int y0, int x1, int y1) {
        rememberedX0_ = x0;
        rememberedY0_ = y0;
        rememberedX1_ = x1;
        rememberedY1_ = y1;
        hasRememberedBounds_ = true;
    }

    bool hasRememberedBounds() const { return hasRememberedBounds_; }

    void rewakeRememberedBounds(int expandTiles = 2) {
        if (hasRememberedBounds_) {
            wakeBoundsPx(rememberedX0_, rememberedY0_, rememberedX1_, rememberedY1_, expandTiles);
            return;
        }
        wakeBottomBandFallback();
    }

    void markDisk(int cx, int cy, int radius) {
        if (tiles_.empty()) return;
        const int x0 = std::max(0, cx - radius - 1);
        const int x1 = std::min(gw_ - 1, cx + radius + 1);
        const int y0 = std::max(0, cy - radius - 1);
        const int y1 = std::min(gh_ - 1, cy + radius + 1);
        const int tx0 = x0 / kTileSize;
        const int tx1 = x1 / kTileSize;
        const int ty0 = y0 / kTileSize;
        const int ty1 = y1 / kTileSize;
        for (int ty = ty0; ty <= ty1; ++ty) {
            for (int tx = tx0; tx <= tx1; ++tx) {
                wakeTile(tx, ty);
            }
        }
    }

    void wakeTile(int tx, int ty) {
        if (tx < 0 || ty < 0 || tx >= tw_ || ty >= th_) return;
        const size_t i = static_cast<size_t>(ty * tw_ + tx);
        tiles_[i] = 1u;
        unchangedFrames_[i] = 0u;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                const int nx = tx + dx;
                const int ny = ty + dy;
                if (nx < 0 || ny < 0 || nx >= tw_ || ny >= th_) continue;
                tiles_[static_cast<size_t>(ny * tw_ + nx)] = 1u;
            }
        }
    }

    void tickOptimizer(ActiveTileMode mode) {
        if (mode == ActiveTileMode::Off || tiles_.empty()) return;
        const uint8_t maxFrames = mode == ActiveTileMode::Aggressive ? 120u : 160u;
        const int spread = mode == ActiveTileMode::Aggressive ? 1 : 2;
        std::fill(next_.begin(), next_.end(), 0u);
        std::fill(nextFrames_.begin(), nextFrames_.end(), 0u);
        for (int ty = 0; ty < th_; ++ty) {
            for (int tx = 0; tx < tw_; ++tx) {
                const size_t i = static_cast<size_t>(ty * tw_ + tx);
                if (!tiles_[i]) continue;
                const uint8_t age = static_cast<uint8_t>(
                    std::min<int>(255, int(unchangedFrames_[i]) + 1));
                if (age >= maxFrames) continue;
                for (int dy = -spread; dy <= spread; ++dy) {
                    for (int dx = -spread; dx <= spread; ++dx) {
                        if (mode == ActiveTileMode::Aggressive && dx != 0 && dy != 0) continue;
                        const int nx = tx + dx;
                        const int ny = ty + dy;
                        if (nx < 0 || ny < 0 || nx >= tw_ || ny >= th_) continue;
                        const size_t ni = static_cast<size_t>(ny * tw_ + nx);
                        next_[ni] = 1u;
                        nextFrames_[ni] = std::max<uint8_t>(nextFrames_[ni], age);
                    }
                }
            }
        }
        if (mode == ActiveTileMode::Aggressive) {
            wakeAggressiveVerticalHalo(next_);
        }
        int nextActive = 0;
        for (uint8_t v : next_) {
            if (v) ++nextActive;
        }
        if (nextActive == 0) {
            tiles_.swap(next_);
            unchangedFrames_.swap(nextFrames_);
            return;
        }
        tiles_.swap(next_);
        unchangedFrames_.swap(nextFrames_);
    }

    void tickConservative() {
        tickOptimizer(ActiveTileMode::Conservative);
    }

    bool activeBounds(int& x0, int& y0, int& x1, int& y1, int expandTiles = 0) const {
        if (tiles_.empty()) return false;
        int minTx = tw_, minTy = th_, maxTx = -1, maxTy = -1;
        for (int ty = 0; ty < th_; ++ty) {
            for (int tx = 0; tx < tw_; ++tx) {
                if (!isActive(tx, ty)) continue;
                minTx = std::min(minTx, tx);
                minTy = std::min(minTy, ty);
                maxTx = std::max(maxTx, tx);
                maxTy = std::max(maxTy, ty);
            }
        }
        if (maxTx < minTx || maxTy < minTy) return false;
        minTx = std::max(0, minTx - expandTiles);
        minTy = std::max(0, minTy - expandTiles);
        maxTx = std::min(tw_ - 1, maxTx + expandTiles);
        maxTy = std::min(th_ - 1, maxTy + expandTiles);
        x0 = minTx * kTileSize;
        y0 = minTy * kTileSize;
        x1 = std::min(gw_ - 1, (maxTx + 1) * kTileSize - 1);
        y1 = std::min(gh_ - 1, (maxTy + 1) * kTileSize - 1);
        return true;
    }

    bool activeRuns(std::vector<ActiveTileRun>& out, int expandTiles = 0) const {
        out.clear();
        if (tiles_.empty()) return false;
        for (int ty = 0; ty < th_; ++ty) {
            int tx = 0;
            while (tx < tw_) {
                while (tx < tw_ && !isExpandedActive(tx, ty, expandTiles)) ++tx;
                if (tx >= tw_) break;
                const int startTx = tx;
                while (tx < tw_ && isExpandedActive(tx, ty, expandTiles)) ++tx;
                const int endTx = tx - 1;

                ActiveTileRun run;
                run.x0 = startTx * kTileSize;
                run.x1 = std::min(gw_ - 1, (endTx + 1) * kTileSize - 1);
                run.y0 = ty * kTileSize;
                run.y1 = std::min(gh_ - 1, (ty + 1) * kTileSize - 1);
                if (!out.empty()) {
                    ActiveTileRun& prev = out.back();
                    if (prev.x0 == run.x0 && prev.x1 == run.x1 && prev.y1 + 1 == run.y0) {
                        prev.y1 = run.y1;
                        continue;
                    }
                }
                out.push_back(run);
            }
        }
        return !out.empty();
    }

    int tileW() const { return tw_; }
    int tileH() const { return th_; }
    int gridW() const { return gw_; }
    int gridH() const { return gh_; }

    bool isActive(int tx, int ty) const {
        if (tx < 0 || ty < 0 || tx >= tw_ || ty >= th_) return false;
        return tiles_[static_cast<size_t>(ty * tw_ + tx)] != 0;
    }

    int activeCount() const {
        int n = 0;
        for (uint8_t v : tiles_) {
            if (v) ++n;
        }
        return n;
    }

private:
    void wakeBottomBandFallback() {
        const int ty0 = (th_ * 3) / 5;
        wakeBoundsPx(0, ty0 * kTileSize, gw_ - 1, gh_ - 1, 1);
    }

    void wakeAggressiveVerticalHalo(std::vector<uint8_t>& next) {
        std::vector<uint8_t> halo(next.size(), 0u);
        for (int ty = 0; ty < th_; ++ty) {
            for (int tx = 0; tx < tw_; ++tx) {
                if (!next[static_cast<size_t>(ty * tw_ + tx)]) continue;
                const int haloTy = std::max(0, ty - 1);
                for (int dx = -1; dx <= 1; ++dx) {
                    const int hx = tx + dx;
                    if (hx < 0 || hx >= tw_) continue;
                    halo[static_cast<size_t>(haloTy * tw_ + hx)] = 1u;
                }
            }
        }
        for (size_t i = 0; i < next.size(); ++i) {
            if (halo[i]) next[i] = 1u;
        }
    }

    bool isExpandedActive(int tx, int ty, int expandTiles) const {
        const int expand = std::max(0, expandTiles);
        for (int dy = -expand; dy <= expand; ++dy) {
            for (int dx = -expand; dx <= expand; ++dx) {
                if (isActive(tx + dx, ty + dy)) return true;
            }
        }
        return false;
    }

    int gw_ = 0;
    int gh_ = 0;
    int tw_ = 0;
    int th_ = 0;
    bool hasRememberedBounds_ = false;
    int rememberedX0_ = 0;
    int rememberedY0_ = 0;
    int rememberedX1_ = 0;
    int rememberedY1_ = 0;
    std::vector<uint8_t> tiles_;
    std::vector<uint8_t> unchangedFrames_;
    std::vector<uint8_t> next_;
    std::vector<uint8_t> nextFrames_;
};

} // namespace nx
