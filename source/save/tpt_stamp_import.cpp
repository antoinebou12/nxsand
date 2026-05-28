#include "tpt_stamp_import.hpp"
#include "tpt_material_map.hpp"
#include "../sim/materials.hpp"
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

namespace nx {

bool importTptStampJson(const std::string& jsonText, int targetW, int targetH,
                        std::vector<uint8_t>& outGrid) {
    if (targetW <= 0 || targetH <= 0) return false;
    outGrid.assign(static_cast<size_t>(targetW * targetH), static_cast<uint8_t>(MAT_EMPTY));

    nlohmann::json j = nlohmann::json::parse(jsonText, nullptr, false);
    if (j.is_discarded() || !j.contains("particles") || !j["particles"].is_array()) return false;

    const int srcW = std::max(1, j.value("width", targetW));
    const int srcH = std::max(1, j.value("height", targetH));
    const float scaleX = float(targetW) / float(srcW);
    const float scaleY = float(targetH) / float(srcH);

    for (const auto& part : j["particles"]) {
        if (!part.is_object()) continue;
        const int tptType = part.value("type", 0);
        const Material mat = tpt::mapTptType(tptType);
        if (mat == MAT_EMPTY) continue;

        const float px = part.value("x", 0.f);
        const float py = part.value("y", 0.f);
        const int gx = int(std::floor(px * scaleX));
        const int gy = int(std::floor(py * scaleY));
        if (gx < 0 || gy < 0 || gx >= targetW || gy >= targetH) continue;

        outGrid[static_cast<size_t>(gy * targetW + gx)] = static_cast<uint8_t>(mat);
    }
    return true;
}

} // namespace nx
