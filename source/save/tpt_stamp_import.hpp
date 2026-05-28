#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nx {

// Rasterize a minimal TPT stamp JSON export into a top-down material grid (row 0 = top).
// JSON: { "width": W, "height": H, "particles": [ { "type": PT_*, "x": float, "y": float }, ... ] }
// Coordinates are TPT pixel space (origin top-left). Out-of-range particles are skipped.
bool importTptStampJson(const std::string& jsonText, int targetW, int targetH,
                        std::vector<uint8_t>& outGrid);

} // namespace nx
