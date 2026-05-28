// Material IDs and palette (GPU sim, saves, HUD).
//
// IDs 0–13 match nxsand `Material` enum; do not renumber — slot JSON stores raw bytes.
// GPU rules live in shaders/sim.frag (`M_*` constants must stay in sync).
//
// Notable reaction (NXSand vs nxsand CPU): lava on water produces STONE + SMOKE here
// (nxsand writes WALL under the lava). Stone is a separate playable solid (palette slot 9).
#pragma once
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
    MAT_ATTRACTOR = 12,  // legacy nxsand ID; not playable (sim clears to empty)
    MAT_PUSHER    = 13,  // legacy nxsand ID; not playable (sim clears to empty)
};

// Highest material ID in saves.
constexpr int MATERIAL_COUNT = 13;
constexpr int PALETTE_SIZE   = 256;

// Same packing as packRgb(r,g,b) in materials.ts: 0xff000000 | b<<16 | g<<8 | r.
// In memory on a little-endian host the bytes land as R,G,B,A, matching GL_RGBA8.
constexpr uint32_t pack_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xff000000u | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
}

// Palette numbers copied verbatim from initPalette() in materials.ts.
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
    p[MAT_OIL]       = pack_rgb(25, 45, 95);
    p[MAT_ICE] = pack_rgb(190, 230, 255);
    return p;
}

// Selector / HUD row 1+2 order (matches PLAYABLE_MATERIALS in materials.ts).
constexpr std::array<Material, 11> PLAYABLE_MATERIALS{
    MAT_SAND, MAT_WATER, MAT_WALL, MAT_PLANT, MAT_FIRE, MAT_LAVA,
    MAT_ACID, MAT_SMOKE, MAT_STONE, MAT_OIL, MAT_ICE,
};

// Picture material picker (playable materials only).
constexpr std::array<Material, 11> PICKER_MATERIALS{
    MAT_SAND, MAT_WATER, MAT_WALL, MAT_PLANT, MAT_FIRE, MAT_LAVA,
    MAT_ACID, MAT_SMOKE, MAT_STONE, MAT_OIL, MAT_ICE,
};

struct HudSlot {
    Material    mat;
    const char* keyHint;
};

// Bottom palette row 1 (matches nxsand HUD_ROW1).
constexpr std::array<HudSlot, 8> HUD_PALETTE_ROW1{{
    {MAT_SAND,  ""}, {MAT_WATER, ""}, {MAT_WALL, ""},  {MAT_PLANT, ""},
    {MAT_FIRE,  ""}, {MAT_LAVA,  ""}, {MAT_ACID,  ""}, {MAT_SMOKE, ""},
}};
constexpr std::array<HudSlot, 3> HUD_PALETTE_ROW2{{
    {MAT_STONE, ""}, {MAT_OIL, ""}, {MAT_ICE, ""},
}};

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
        case MAT_ATTRACTOR:
        case MAT_PUSHER:    return "?";
    }
    return "?";
}

inline bool material_is_solid(Material m) {
    return m != MAT_EMPTY && m != MAT_SMOKE && m != MAT_FIRE;
}

inline Material sanitizeBrushMaterial(int id) {
    const auto m = static_cast<Material>(id);
    for (Material p : PICKER_MATERIALS) {
        if (p == m) return m;
    }
    return MAT_SAND;
}

} // namespace nx
