#include "active_tiles_overlay.hpp"
#include "../game/app.hpp"
#include "../gpu/active_tiles.hpp"
#include "../gpu/render_pipeline.hpp"
#include "../gpu/sim_pipeline.hpp"

namespace nx {

void drawActiveTilesOverlay(RenderPipeline& r, const App& app, const PlayRegion& pr) {
    if (!app.settings.debug.showActiveTiles) return;
    if (app.settings.performance.activeTiles == ActiveTileMode::Off) return;
    if (!app.simPipeline || !app.simPipeline->ready()) return;

    const auto& map = app.simPipeline->activeTiles;
    const int gw = map.gridW();
    const int gh = map.gridH();
    const int tw = map.tileW();
    const int th = map.tileH();
    if (gw <= 0 || gh <= 0 || tw <= 0 || th <= 0 || pr.w <= 0 || pr.h <= 0) return;

    const float tilePxW = float(pr.w) / float(map.gridW()) * float(ActiveTileMap::kTileSize);
    const float tilePxH = float(pr.h) / float(map.gridH()) * float(ActiveTileMap::kTileSize);

    for (int ty = 0; ty < th; ++ty) {
        for (int tx = 0; tx < tw; ++tx) {
            if (!map.isActive(tx, ty)) continue;
            const float x = float(pr.x) + float(tx * ActiveTileMap::kTileSize) /
                                             float(map.gridW()) * float(pr.w);
            const float y = float(pr.y) + float(ty * ActiveTileMap::kTileSize) /
                                             float(map.gridH()) * float(pr.h);
            r.drawSolidRect(x, y, tilePxW, tilePxH, 0.2f, 0.9f, 0.4f, 0.18f, app.screenW,
                            app.screenH);
        }
    }
}

} // namespace nx
