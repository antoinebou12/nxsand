#pragma once
#include "../sim/physics_params.hpp"

namespace nx {

bool loadPhysicsParams(PhysicsParams& out);
bool savePhysicsParams(const PhysicsParams& params);
void markPhysicsParamsDirty();
void flushPhysicsParamsIfDirty(const PhysicsParams& params);

} // namespace nx
