// Material IDs and palette (GPU sim, saves, HUD).
//
// IDs 0–22; slot JSON stores raw bytes — do not renumber existing IDs. ID 21 (legacy TNT) loads as empty.
// IDs 18–19 (Ember, Flower) are spawn-only — not in PICKER_MATERIALS.
// GPU rules live in shaders/sim.frag (`M_*` constants must stay in sync).
//
// Notable reaction (NXSand vs nxsand CPU): lava on water produces STONE + SMOKE here
// (nxsand writes WALL under the lava). Stone is a separate playable solid (palette slot 9).
#ifndef NX_MATERIALS_HPP
#define NX_MATERIALS_HPP
#include <cstddef>
#include <cstdint>
#include <array>

namespace nx {

enum Material : uint8_t {
    MAT_EMPTY     = 0,
    MAT_SAND      = 1,
    MAT_WATER     = 2,
    MAT_FIRE      = 3,
    MAT_SMOKE     = 4,
    MAT_WALL      = 5,
    MAT_ACID      = 6,
    MAT_PLANT     = 7,
    MAT_LAVA      = 8,
    MAT_STONE     = 9,
    MAT_OIL       = 10,
    MAT_ICE       = 11,
    MAT_STEAM     = 12,
    MAT_GLASS     = 13,
    MAT_WOOD      = 14,
    MAT_METAL     = 15,
    MAT_GUNPOWDER = 16,
    MAT_SALT      = 17,
    MAT_EMBER     = 18,
    MAT_FLOWER    = 19,  // spawn-only bloom from wet plant; not in material ring
    MAT_COAL      = 20,
    MAT_BRICK     = 22,
};

constexpr uint8_t LEGACY_MAT_TNT_ID = 21;

// Highest material ID in saves (includes spawn-only Ember and Flower).
constexpr int MATERIAL_COUNT = 22;
constexpr int PALETTE_SIZE   = 256;

// Same packing as packRgb(r,g,b) in materials.ts: 0xff000000 | b<<16 | g<<8 | r.
// In memory on a little-endian host the bytes land as R,G,B,A, matching GL_RGBA8.
constexpr uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xff000000u | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
}

inline std::array<uint32_t, PALETTE_SIZE> build_palette() {
    std::array<uint32_t, PALETTE_SIZE> p{};
    p[MAT_EMPTY]     = 0xff000000u;
    p[MAT_SAND]      = 0xff80b2d8u;  // exact value from JS (ABGR-in-uint32)
    p[MAT_WATER]     = 0xffff8844u;
    p[MAT_FIRE]      = 0xff2266ffu;
    p[MAT_SMOKE]     = 0xff555555u;
    p[MAT_WALL]      = 0xff888888u;
    p[MAT_ACID]      = pack_rgb(80, 230, 80);
    p[MAT_PLANT]     = pack_rgb(38, 210, 48);
    p[MAT_LAVA]      = pack_rgb(255, 70, 15);
    p[MAT_STONE]     = pack_rgb(90, 105, 125);
    p[MAT_OIL]       = pack_rgb(18, 62, 128);
    p[MAT_ICE]       = pack_rgb(190, 230, 255);
    p[MAT_STEAM]     = pack_rgb(200, 220, 240);
    p[MAT_GLASS]     = pack_rgb(180, 220, 230);
    p[MAT_WOOD]      = pack_rgb(120, 72, 38);
    p[MAT_METAL]     = pack_rgb(160, 165, 175);
    p[MAT_GUNPOWDER] = pack_rgb(118, 108, 78);
    p[MAT_SALT]      = pack_rgb(235, 240, 248);
    p[MAT_EMBER]     = pack_rgb(200, 120, 20);
    p[MAT_FLOWER]    = pack_rgb(255, 130, 200);
    p[MAT_COAL]      = pack_rgb(35, 32, 28);
    p[MAT_BRICK]     = pack_rgb(140, 75, 55);
    return p;
}

constexpr std::array<Material, 19> PLAYABLE_MATERIALS{
    MAT_SAND, MAT_WATER, MAT_WALL, MAT_PLANT, MAT_FIRE, MAT_LAVA,
    MAT_ACID, MAT_SMOKE, MAT_STONE, MAT_OIL, MAT_ICE, MAT_WOOD, MAT_GLASS, MAT_STEAM,
    MAT_METAL, MAT_GUNPOWDER, MAT_COAL, MAT_SALT, MAT_BRICK,
};

constexpr std::array<Material, 19> PICKER_MATERIALS{
    MAT_SAND, MAT_WATER, MAT_WALL, MAT_PLANT, MAT_FIRE, MAT_LAVA,
    MAT_ACID, MAT_SMOKE, MAT_STONE, MAT_OIL, MAT_ICE, MAT_WOOD, MAT_GLASS, MAT_STEAM,
    MAT_METAL, MAT_GUNPOWDER, MAT_COAL, MAT_SALT, MAT_BRICK,
};

inline int selectorMaterialCount() {
    return static_cast<int>(PICKER_MATERIALS.size());
}

inline int materialSelectorIndex(Material m) {
    for (std::size_t i = 0; i < PICKER_MATERIALS.size(); ++i) {
        if (PICKER_MATERIALS[i] == m) return static_cast<int>(i);
    }
    return 0;
}

inline const char* material_short_name(Material m) {
    switch (m) {
        case MAT_SAND:      return "SND";
        case MAT_WATER:     return "H2O";
        case MAT_FIRE:      return "FIR";
        case MAT_SMOKE:     return "SMK";
        case MAT_WALL:      return "WAL";
        case MAT_ACID:      return "ACD";
        case MAT_PLANT:     return "PLT";
        case MAT_LAVA:      return "LAV";
        case MAT_STONE:     return "STN";
        case MAT_OIL:       return "OIL";
        case MAT_ICE:       return "ICE";
        case MAT_STEAM:     return "STM";
        case MAT_GLASS:     return "GLS";
        case MAT_WOOD:      return "WOD";
        case MAT_METAL:     return "MTL";
        case MAT_GUNPOWDER: return "GUN";
        case MAT_SALT:      return "SLT";
        case MAT_EMBER:     return "EMB";
        case MAT_FLOWER:    return "FLW";
        case MAT_COAL:      return "COL";
        case MAT_BRICK:     return "BRK";
        default:            return "---";
    }
}

inline const char* material_name(Material m) {
    switch (m) {
        case MAT_EMPTY:     return "Empty";
        case MAT_SAND:      return "Sand";
        case MAT_WATER:     return "Water";
        case MAT_FIRE:      return "Fire";
        case MAT_SMOKE:     return "Smoke";
        case MAT_WALL:      return "Wall";
        case MAT_ACID:      return "Acid";
        case MAT_PLANT:     return "Plant";
        case MAT_LAVA:      return "Lava";
        case MAT_STONE:     return "Stone";
        case MAT_OIL:       return "Oil";
        case MAT_ICE:       return "Ice";
        case MAT_STEAM:     return "Steam";
        case MAT_GLASS:     return "Glass";
        case MAT_WOOD:      return "Wood";
        case MAT_METAL:     return "Metal";
        case MAT_GUNPOWDER: return "GUN";
        case MAT_SALT:      return "Salt";
        case MAT_EMBER:     return "Ember";
        case MAT_FLOWER:    return "Flower";
        case MAT_COAL:      return "Coal";
        case MAT_BRICK:     return "Brick";
    }
    return "?";
}

inline bool material_is_solid(Material m) {
    return m != MAT_EMPTY && m != MAT_SMOKE && m != MAT_FIRE && m != MAT_STEAM &&
           m != MAT_EMBER && m != MAT_GUNPOWDER && m != MAT_COAL;
}

inline uint8_t sanitizeGridMaterial(uint8_t id) {
    if (id == LEGACY_MAT_TNT_ID || id > static_cast<uint8_t>(MATERIAL_COUNT)) return MAT_EMPTY;
    return id;
}

inline Material sanitizeBrushMaterial(int id) {
    if (id == static_cast<int>(LEGACY_MAT_TNT_ID)) return MAT_SAND;
    const auto m = static_cast<Material>(id);
    for (Material p : PICKER_MATERIALS) {
        if (p == m) return m;
    }
    return MAT_SAND;
}

} // namespace nx

#endif // NX_MATERIALS_HPP
