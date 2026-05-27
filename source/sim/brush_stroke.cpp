#include "brush_stroke.hpp"
#include <algorithm>
#include <cmath>

namespace nx {

BrushStrokePlan planBrushStroke(int x0, int y0, int x1, int y1, int radius) {
    BrushStrokePlan plan;

    const float dx = float(x1 - x0);
    const float dy = float(y1 - y0);
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float spacing = std::max(1.f, float(radius) * 0.35f);
    const int steps = std::max(1, int(std::ceil(dist / spacing)));

    plan.dabs.reserve(static_cast<size_t>(steps + 1));

    int minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool haveBox = false;
    for (int i = 0; i <= steps; ++i) {
        const float t = float(i) / float(steps);
        const int px = int(std::lround(float(x0) + dx * t));
        const int py = int(std::lround(float(y0) + dy * t));
        plan.dabs.push_back({px, py});
        if (!haveBox) {
            minX = px - radius;
            maxX = px + radius;
            minY = py - radius;
            maxY = py + radius;
            haveBox = true;
        } else {
            minX = std::min(minX, px - radius);
            maxX = std::max(maxX, px + radius);
            minY = std::min(minY, py - radius);
            maxY = std::max(maxY, py + radius);
        }
    }
    plan.dirtyMinX = minX;
    plan.dirtyMinY = minY;
    plan.dirtyMaxX = maxX;
    plan.dirtyMaxY = maxY;
    return plan;
}

} // namespace nx
