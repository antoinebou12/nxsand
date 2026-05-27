#include "test_harness.hpp"
#include "sim/materials.hpp"
#include <cstring>

void run_materials_tests(TestContext& ctx) {
    CHECK(ctx, nx::pack_rgb(1, 2, 3) == (0xff000000u | (3u << 16) | (2u << 8) | 1u));

    auto pal = nx::build_palette();
    CHECK(ctx, pal[nx::MAT_EMPTY] == 0xff000000u);
    CHECK(ctx, pal[nx::MAT_SAND] == 0xff80b2d8u);
    CHECK(ctx, pal[nx::MAT_ACID] == nx::pack_rgb(80, 230, 80));

    CHECK(ctx, std::strcmp(nx::material_name(nx::MAT_SAND), "Sand") == 0);
    CHECK(ctx, !nx::material_is_solid(nx::MAT_EMPTY));
    CHECK(ctx, !nx::material_is_solid(nx::MAT_SMOKE));
    CHECK(ctx, !nx::material_is_solid(nx::MAT_FIRE));
    CHECK(ctx, nx::material_is_solid(nx::MAT_WALL));
    CHECK(ctx, nx::material_is_solid(nx::MAT_WATER));

    CHECK(ctx, nx::PLAYABLE_MATERIALS.size() == 11);
    CHECK(ctx, nx::PLAYABLE_MATERIALS[0] == nx::MAT_SAND);
    CHECK(ctx, nx::PLAYABLE_MATERIALS[10] == nx::MAT_ICE);
    CHECK(ctx, nx::PICKER_MATERIALS.size() == 11);
    CHECK(ctx, nx::HUD_PALETTE_ROW2.size() == 3);
    CHECK(ctx, nx::sanitizeBrushMaterial(12) == nx::MAT_SAND);
    CHECK(ctx, nx::sanitizeBrushMaterial(13) == nx::MAT_SAND);
    CHECK(ctx, nx::MATERIAL_COUNT == 13);
}
