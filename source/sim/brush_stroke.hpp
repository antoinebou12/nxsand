#pragma once
#include "../sim/materials.hpp"
#include "../sim/brush_command.hpp"
#include <vector>

namespace nx {

class SimPipeline;

struct BrushDab {
    int x;
    int y;
};

// Output of planBrushStroke. dabs[] is in stroke order; the bbox is inclusive
// and reflects the union of all dab disks (each dab covers cx-r..cx+r, cy-r..cy+r).
struct BrushStrokePlan {
    std::vector<BrushDab> dabs;
    int dirtyMinX = 0;
    int dirtyMinY = 0;
    int dirtyMaxX = 0;
    int dirtyMaxY = 0;
};

// Pure geometry: turn a click-drag from (x0,y0) to (x1,y1) into discrete dab
// centers. No GPU, no Material, no SimPipeline — unit-testable.
BrushStrokePlan planBrushStroke(int x0, int y0, int x1, int y1, int radius);

void emitBrushStroke(SimPipeline& pipe, int x0, int y0, int x1, int y1, int radius, Material mat,
                     bool erase, int& outCommandCount, int& outDirtyW, int& outDirtyH);

} // namespace nx
