#include "test_harness.hpp"
#include "save/tpt_material_map.hpp"
#include "save/tpt_stamp_import.hpp"
#include "sim/materials.hpp"

void run_tpt_import_tests(TestContext& ctx) {
    CHECK(ctx, nx::tpt::mapTptType(nx::tpt::PT_WATR) == nx::MAT_WATER);
    CHECK(ctx, nx::tpt::mapTptType(nx::tpt::PT_SAND) == nx::MAT_SAND);
    CHECK(ctx, nx::tpt::mapTptType(nx::tpt::PT_LAVA) == nx::MAT_LAVA);
    CHECK(ctx, nx::tpt::mapTptType(9999) == nx::MAT_EMPTY);

    static const char* kMini = R"({
        "width": 10,
        "height": 10,
        "particles": [
            {"type": 2, "x": 5.0, "y": 5.0},
            {"type": 4, "x": 6.0, "y": 5.0}
        ]
    })";

    std::vector<uint8_t> grid;
    CHECK(ctx, nx::importTptStampJson(kMini, 10, 10, grid));
    CHECK(ctx, grid.size() == 100u);
    CHECK(ctx, grid[55] == static_cast<uint8_t>(nx::MAT_WATER));
    CHECK(ctx, grid[56] == static_cast<uint8_t>(nx::MAT_SAND));

    CHECK(ctx, !nx::importTptStampJson("{", 8, 8, grid));
}
