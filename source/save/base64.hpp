#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace nx {

std::string base64Encode(const std::vector<uint8_t>& raw);
bool base64Decode(const std::string& in, std::vector<uint8_t>& out);

} // namespace nx
