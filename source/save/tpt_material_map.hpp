#pragma once
#include "../sim/materials.hpp"
#include <cstdint>

namespace nx::tpt {

// Common The Powder Toy element type IDs (PT_*), for stamp import only.
constexpr int PT_NONE = 0;
constexpr int PT_DUST = 1;
constexpr int PT_WATR = 2;
constexpr int PT_OIL = 3;
constexpr int PT_SAND = 4;
constexpr int PT_SMKE = 5;
constexpr int PT_FIRE = 6;
constexpr int PT_ICEI = 17;
constexpr int PT_BRMT = 22;
constexpr int PT_STNE = 23;
constexpr int PT_BMTL = 28;
constexpr int PT_PLNT = 34;
constexpr int PT_LAVA = 37;
constexpr int PT_DSTW = 64;
constexpr int PT_SLTW = 65;
constexpr int PT_ACID = 146;
constexpr int PT_GLAS = 52;
constexpr int PT_WOOD = 191;
constexpr int PT_STEM = 63;
constexpr int PT_GUNP = 62;
constexpr int PT_TNT  = 8;
constexpr int PT_COAL = 27;
constexpr int PT_SALT = 95;

inline Material mapTptType(int tptType) {
    switch (tptType) {
        case PT_WATR:
        case PT_DSTW:
        case PT_SLTW:
            return MAT_WATER;
        case PT_SAND:
        case PT_DUST:
            return MAT_SAND;
        case PT_STNE:
        case PT_BRMT:
            return MAT_STONE;
        case PT_LAVA:
            return MAT_LAVA;
        case PT_OIL:
            return MAT_OIL;
        case PT_FIRE:
            return MAT_FIRE;
        case PT_SMKE:
            return MAT_SMOKE;
        case PT_ACID:
            return MAT_ACID;
        case PT_BMTL:
            return MAT_METAL;
        case PT_GUNP:
            return MAT_GUNPOWDER;
        case PT_TNT:
            return MAT_TNT;
        case PT_COAL:
            return MAT_COAL;
        case PT_SALT:
            return MAT_SALT;
        case PT_ICEI:
            return MAT_ICE;
        case PT_PLNT:
            return MAT_PLANT;
        case PT_GLAS:
            return MAT_GLASS;
        case PT_WOOD:
            return MAT_WOOD;
        case PT_STEM:
            return MAT_STEAM;
        default:
            return MAT_EMPTY;
    }
}

} // namespace nx::tpt
