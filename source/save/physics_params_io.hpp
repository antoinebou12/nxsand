#pragma once
#include "../sim/physics_params.hpp"

namespace nx {

bool loadPhysicsParams(PhysicsParams& out);
bool savePhysicsParams(const PhysicsParams& params);
void markPhysicsParamsDirty();
bool physicsParamsDirty();
bool flushPhysicsParamsIfDirty(const PhysicsParams& params);

} // namespace nx
