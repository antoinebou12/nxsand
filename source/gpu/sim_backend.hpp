#pragma once

namespace nx {

enum class SimBackend : int {
    Fragment = 0,
    Compute = 1,
};

const char* simBackendLabel(SimBackend b);

} // namespace nx
