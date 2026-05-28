#include "sim_backend.hpp"

namespace nx {

const char* simBackendLabel(SimBackend b) {
    switch (b) {
        case SimBackend::Fragment: return "Fragment";
        case SimBackend::Compute: return "Compute";
    }
    return "Fragment";
}

} // namespace nx
