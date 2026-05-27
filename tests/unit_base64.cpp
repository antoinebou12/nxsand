#include "test_harness.hpp"
#include "save/base64.hpp"
#include <string>
#include <vector>

void run_base64_tests(TestContext& ctx) {
    {
        std::vector<uint8_t> raw;
        CHECK(ctx, nx::base64Encode(raw).empty());
        std::vector<uint8_t> out;
        CHECK(ctx, nx::base64Decode("", out));
        CHECK(ctx, out.empty());
    }

    {
        std::vector<uint8_t> raw{0x00, 0x01, 0x02};
        const std::string enc = nx::base64Encode(raw);
        CHECK(ctx, enc == "AAEC");
        std::vector<uint8_t> dec;
        CHECK(ctx, nx::base64Decode(enc, dec));
        CHECK(ctx, dec == raw);
    }

    {
        const std::string enc = "Zm9v";
        std::vector<uint8_t> dec;
        CHECK(ctx, nx::base64Decode(enc, dec));
        CHECK(ctx, dec.size() == 3);
        CHECK(ctx, dec[0] == 'f' && dec[1] == 'o' && dec[2] == 'o');
    }

    {
        std::vector<uint8_t> raw(256);
        for (size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<uint8_t>(i);
        std::vector<uint8_t> dec;
        CHECK(ctx, nx::base64Decode(nx::base64Encode(raw), dec));
        CHECK(ctx, dec == raw);
    }
}
