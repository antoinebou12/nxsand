#include "benchmark_scene.hpp"
#include "app.hpp"
#include "../sim/materials.hpp"

namespace nx {

void seedBenchmarkScene(App& app, int sceneId) {
    if (!app.simPipeline) return;
    app.simPipeline->clearAll(MAT_EMPTY);
    app.sim.gridHasMatter = false;
    const int w = app.sim.grid_w;
    const int h = app.sim.grid_h;
    switch (sceneId) {
        case 0:
            for (int x = 0; x < w; x += 2) {
                app.simPipeline->paintDisk(x, h - 2, 1, MAT_SAND);
            }
            break;
        case 1:
            app.simPipeline->paintDisk(w / 2, h / 2, w / 4, MAT_WATER);
            break;
        case 2:
            app.simPipeline->paintDisk(w / 2, h / 3, 8, MAT_LAVA);
            break;
        default:
            break;
    }
    app.sim.gridHasMatter = sceneId >= 0 && sceneId <= 2;
    app.sim.sleeping = false;
    app.toast.show("Benchmark scene loaded", 1.2f);
}

void seedStarterScene(App& app) {
    if (!app.simPipeline) return;
    app.simPipeline->clearAll(MAT_EMPTY);
    const int w = app.sim.grid_w;
    const int h = app.sim.grid_h;
    const int cx = w / 2;
    const int floorY = std::max(8, h / 10);
    const int bowlR = std::max(5, w / 72);

    for (int x = w / 6; x <= (w * 5) / 6; x += bowlR * 2) {
        app.simPipeline->paintDisk(x, floorY, bowlR, MAT_WALL);
    }
    for (int i = 0; i < 6; ++i) {
        app.simPipeline->paintDisk(w / 6 + i * bowlR, floorY + i * (bowlR / 2), bowlR, MAT_WALL);
        app.simPipeline->paintDisk((w * 5) / 6 - i * bowlR, floorY + i * (bowlR / 2), bowlR, MAT_WALL);
    }

    app.simPipeline->paintDisk(cx - w / 5, h / 2, std::max(6, w / 48), MAT_SAND);
    app.simPipeline->paintDisk(cx - w / 14, h / 3, std::max(7, w / 44), MAT_WATER);
    app.simPipeline->paintDisk(cx + w / 10, h / 3, std::max(6, w / 52), MAT_OIL);
    app.simPipeline->paintDisk(cx + w / 4, h / 2, std::max(5, w / 58), MAT_ACID);
    app.simPipeline->paintDisk(cx, (h * 3) / 4, std::max(4, w / 72), MAT_LAVA);
    app.simPipeline->paintDisk(cx + w / 3, (h * 2) / 3, std::max(4, w / 72), MAT_ICE);
    app.simPipeline->paintDisk(cx - w / 3, floorY + h / 6, std::max(5, w / 70), MAT_PLANT);
    app.simPipeline->syncSimForSampling();
    app.sim.gridHasMatter = true;
    app.sim.sleeping = false;
    app.toast.show("Starter sandbox", 1.2f);
}

} // namespace nx
