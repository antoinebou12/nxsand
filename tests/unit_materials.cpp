#include "test_harness.hpp"
#include "sim/materials.hpp"
#include <cstring>

void run_materials_tests(TestContext& ctx) {
    CHECK(ctx, nx::pack_rgb(1, 2, 3) == (0xff000000u | (3u << 16) | (2u << 8) | 1u));

    auto pal = nx::build_palette();
    CHECK(ctx, pal[nx::MAT_EMPTY] == 0xff000000u);
    CHECK(ctx, pal[nx::MAT_SAND] == 0xff80b2d8u);
    CHECK(ctx, pal[nx::MAT_ACID] == nx::pack_rgb(80, 230, 80));
    CHECK(ctx, pal[nx::MAT_STEAM] == nx::pack_rgb(200, 220, 240));
    CHECK(ctx, pal[nx::MAT_GLASS] == nx::pack_rgb(180, 220, 230));
    CHECK(ctx, pal[nx::MAT_WOOD] == nx::pack_rgb(120, 72, 38));
    CHECK(ctx, pal[nx::MAT_METAL] == nx::pack_rgb(160, 165, 175));
    CHECK(ctx, pal[nx::MAT_GUNPOWDER] == nx::pack_rgb(45, 42, 38));
    CHECK(ctx, pal[nx::MAT_SALT] == nx::pack_rgb(235, 240, 248));
    CHECK(ctx, pal[nx::MAT_EMBER] == nx::pack_rgb(200, 120, 20));

    CHECK(ctx, std::strcmp(nx::material_name(nx::MAT_SAND), "Sand") == 0);
    CHECK(ctx, std::strcmp(nx::material_name(nx::MAT_STEAM), "Steam") == 0);
    CHECK(ctx, std::strcmp(nx::material_name(nx::MAT_METAL), "Metal") == 0);
    CHECK(ctx, std::strcmp(nx::material_name(nx::MAT_GUNPOWDER), "Gunpowder") == 0);
    CHECK(ctx, std::strcmp(nx::material_name(nx::MAT_SALT), "Salt") == 0);
    CHECK(ctx, std::strcmp(nx::material_name(nx::MAT_EMBER), "Ember") == 0);
    CHECK(ctx, !nx::material_is_solid(nx::MAT_EMPTY));
    CHECK(ctx, !nx::material_is_solid(nx::MAT_SMOKE));
    CHECK(ctx, !nx::material_is_solid(nx::MAT_FIRE));
    CHECK(ctx, !nx::material_is_solid(nx::MAT_STEAM));
    CHECK(ctx, !nx::material_is_solid(nx::MAT_EMBER));
    CHECK(ctx, !nx::material_is_solid(nx::MAT_GUNPOWDER));
    CHECK(ctx, nx::material_is_solid(nx::MAT_WALL));
    CHECK(ctx, nx::material_is_solid(nx::MAT_WATER));
    CHECK(ctx, nx::material_is_solid(nx::MAT_WOOD));
    CHECK(ctx, nx::material_is_solid(nx::MAT_METAL));
    CHECK(ctx, nx::material_is_solid(nx::MAT_SALT));

    CHECK(ctx, nx::PLAYABLE_MATERIALS.size() == 17);
    CHECK(ctx, nx::PLAYABLE_MATERIALS[0] == nx::MAT_SAND);
    CHECK(ctx, nx::PLAYABLE_MATERIALS[15] == nx::MAT_GUNPOWDER);
    CHECK(ctx, nx::PLAYABLE_MATERIALS[16] == nx::MAT_SALT);
    CHECK(ctx, nx::PICKER_MATERIALS.size() == 17);
    CHECK(ctx, nx::HUD_PALETTE_ROW2.size() == 6);
    CHECK(ctx, nx::HUD_PALETTE_ROW3.size() == 3);
    CHECK(ctx, nx::sanitizeBrushMaterial(12) == nx::MAT_STEAM);
    CHECK(ctx, nx::sanitizeBrushMaterial(15) == nx::MAT_METAL);
    CHECK(ctx, nx::sanitizeBrushMaterial(16) == nx::MAT_GUNPOWDER);
    CHECK(ctx, nx::sanitizeBrushMaterial(17) == nx::MAT_SALT);
    CHECK(ctx, nx::sanitizeBrushMaterial(18) == nx::MAT_SAND);
    CHECK(ctx, nx::MATERIAL_COUNT == 18);
}
