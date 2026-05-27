#include "brush_stroke.hpp"
#include "../gpu/sim_pipeline.hpp"

namespace nx {

void emitBrushStroke(SimPipeline& pipe, int x0, int y0, int x1, int y1, int radius, Material mat,
                     bool erase, int& outCommandCount, int& outDirtyW, int& outDirtyH) {
    const Material m = erase ? MAT_EMPTY : mat;
    const BrushStrokePlan plan = planBrushStroke(x0, y0, x1, y1, radius);
    for (const BrushDab& d : plan.dabs) {
        pipe.paintDisk(d.x, d.y, radius, m);
    }
    outCommandCount = static_cast<int>(plan.dabs.size());
    outDirtyW = plan.dirtyMaxX - plan.dirtyMinX + 1;
    outDirtyH = plan.dirtyMaxY - plan.dirtyMinY + 1;
}

} // namespace nx
