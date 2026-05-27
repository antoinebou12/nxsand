// Settings-menu metadata for PhysicsParams (labels, min/max, defaults).
// Hard-coded reactions (e.g. lava+water) are documented in docs/PHYSICS.md.
#pragma once
#include "materials.hpp"
#include "physics_params.hpp"
#include <cstddef>
#include <vector>

namespace nx {

struct ParamSpec {
    const char* id;
    const char* label;
    float minV, maxV, step, def;
};

int settingsMaterialCount();
Material settingsMaterialAt(int i);
int paramCountFor(Material m);
const ParamSpec* paramSpecAt(Material m, int i);

float getParam(const PhysicsParams& p, Material m, const char* id);
void adjustParam(PhysicsParams& p, Material m, const char* id, int direction);
void formatParamValue(char* buf, size_t bufSize, const ParamSpec* spec, float v);

} // namespace nx
