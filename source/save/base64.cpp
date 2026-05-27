#include "base64.hpp"

namespace nx {

static const char* b64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<uint8_t>& raw) {
    std::string out;
    out.reserve(((raw.size() + 2) / 3) * 4);
    for (size_t i = 0; i < raw.size(); i += 3) {
        uint32_t n = uint32_t(raw[i]) << 16;
        if (i + 1 < raw.size()) n |= uint32_t(raw[i + 1]) << 8;
        if (i + 2 < raw.size()) n |= uint32_t(raw[i + 2]);
        out.push_back(b64[(n >> 18) & 63]);
        out.push_back(b64[(n >> 12) & 63]);
        out.push_back(i + 1 < raw.size() ? b64[(n >> 6) & 63] : '=');
        out.push_back(i + 2 < raw.size() ? b64[n & 63] : '=');
    }
    return out;
}

bool base64Decode(const std::string& in, std::vector<uint8_t>& out) {
    auto dec = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    out.clear();
    int buf = 0, nbits = 0;
    for (char ch : in) {
        if (ch == '=') break;
        int v = dec(static_cast<unsigned char>(ch));
        if (v < 0) continue;
        buf = (buf << 6) | v;
        nbits += 6;
        if (nbits >= 8) {
            nbits -= 8;
            out.push_back(static_cast<uint8_t>((buf >> nbits) & 0xFF));
        }
    }
    return true;
}

} // namespace nx
