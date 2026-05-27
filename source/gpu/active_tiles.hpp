#pragma once
// Active tile tracking: marks brush-woken regions for conservative fragment-pass scissoring.
#include <algorithm>
#include <cstdint>
#include <vector>
#include "../game/game_settings.hpp"

namespace nx {

class ActiveTileMap {
public:
    static constexpr int kTileSize = 32;

    void reset(int gridW, int gridH) {
        gw_ = std::max(1, gridW);
        gh_ = std::max(1, gridH);
        tw_ = (gw_ + kTileSize - 1) / kTileSize;
        th_ = (gh_ + kTileSize - 1) / kTileSize;
        tiles_.assign(static_cast<size_t>(tw_ * th_), 1u);
        unchangedFrames_.assign(tiles_.size(), 0u);
    }

    void wakeAll() {
        std::fill(tiles_.begin(), tiles_.end(), 1u);
        std::fill(unchangedFrames_.begin(), unchangedFrames_.end(), 0u);
    }

    void sleepAll() {
        std::fill(tiles_.begin(), tiles_.end(), 0u);
        std::fill(unchangedFrames_.begin(), unchangedFrames_.end(), 0u);
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
        const uint8_t maxFrames = mode == ActiveTileMode::Aggressive ? 80u : 160u;
        const int spread = mode == ActiveTileMode::Aggressive ? 1 : 2;
        std::vector<uint8_t> next(tiles_.size(), 0u);
        std::vector<uint8_t> nextFrames(unchangedFrames_.size(), 0u);
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
                        next[ni] = 1u;
                        nextFrames[ni] = std::max<uint8_t>(nextFrames[ni], age);
                    }
                }
            }
        }
        tiles_.swap(next);
        unchangedFrames_.swap(nextFrames);
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
    int gw_ = 0;
    int gh_ = 0;
    int tw_ = 0;
    int th_ = 0;
    std::vector<uint8_t> tiles_;
    std::vector<uint8_t> unchangedFrames_;
};

} // namespace nx
