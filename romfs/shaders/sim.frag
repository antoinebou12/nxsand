#version 300 es
// Stable GLES 3.0 fragment CA: 2x2 Margolus block, GL_R8UI ping-pong.

precision highp float;
precision highp int;
precision highp usampler2D;

in vec2 v_uv;
layout(location = 0) out uint outMat;

uniform highp usampler2D uSim;
uniform ivec2 uGridSize;
uniform ivec2 uPhase;
uniform uint uFrame;

layout(std140) uniform PhysicsBlock {
    float fire_speed;
    float fire_smokeRate;
    float fire_ignitePlant;
    float fire_igniteOil;
    float fire_spreadRate;
    float smoke_fadeRate;
    float smoke_driftRate;
    float water_flowRate;
    float _physPad1;
    float acid_wallCorrode;
    float acid_stoneCorrode;
    float plant_growthRate;
    float plant_wallSupport;
    float lava_flowRate;
    float lava_spreadRate;
    float lava_igniteGas;
    float oil_igniteRate;
    float oil_floatRate;
    float ice_meltRate;
    float ice_freezeRate;
    float _physPad2;
    float _physPad3;
    float _physPad4;
    float _physPad5;
};

const uint M_EMPTY = 0u;
const uint M_SAND  = 1u;
const uint M_WATER = 2u;
const uint M_FIRE  = 3u;
const uint M_SMOKE = 4u;
const uint M_WALL  = 5u;
const uint M_ACID  = 6u;
const uint M_PLANT = 7u;
const uint M_LAVA  = 8u;
const uint M_STONE = 9u;
const uint M_OIL   = 10u;
const uint M_ICE   = 11u;

uint hash1(uint v) {
    v ^= v >> 16u;
    v *= 2246822519u;
    v ^= v >> 13u;
    v *= 3266489917u;
    return v ^ (v >> 16u);
}

uint rng(ivec2 c, uint salt) {
    return hash1(uint(c.x) * 374761393u ^ uint(c.y) * 668265263u ^ (uFrame + salt) * 2246822519u);
}

float rand01(ivec2 c, uint salt) {
    return float(rng(c, salt) & 0x00ffffffu) * (1.0 / 16777216.0);
}

uint cell(ivec2 c) {
    if (c.x < 0 || c.y < 0 || c.x >= uGridSize.x || c.y >= uGridSize.y) return M_WALL;
    return texelFetch(uSim, c, 0).r;
}

bool isGas(uint m) {
    return m == M_EMPTY || m == M_FIRE || m == M_SMOKE;
}

bool isLiquid(uint m) {
    return m == M_WATER || m == M_ACID || m == M_LAVA || m == M_OIL;
}

bool isStatic(uint m) {
    return m == M_WALL || m == M_PLANT || m == M_ICE;
}

bool isMovable(uint m) {
    return m == M_SAND || m == M_WATER || m == M_FIRE || m == M_SMOKE ||
           m == M_ACID || m == M_LAVA || m == M_STONE || m == M_OIL;
}

float flowChance(uint m) {
    if (m == M_WATER) return water_flowRate;
    if (m == M_ACID) return 0.24;
    if (m == M_LAVA) return lava_spreadRate;
    if (m == M_OIL) return oil_floatRate;
    if (m == M_FIRE) return fire_spreadRate;
    if (m == M_SMOKE) return smoke_driftRate;
    return 0.0;
}

int density(uint m) {
    if (m == M_FIRE || m == M_SMOKE) return 0;
    if (m == M_EMPTY) return 1;
    if (m == M_OIL) return 2;
    if (m == M_WATER || m == M_ACID || m == M_LAVA) return 3;
    if (m == M_SAND) return 4;
    if (m == M_STONE) return 5;
    return 9;
}

bool anyNeighbor(ivec2 c, uint m) {
    return cell(c + ivec2( 1,  0)) == m ||
           cell(c + ivec2(-1,  0)) == m ||
           cell(c + ivec2( 0,  1)) == m ||
           cell(c + ivec2( 0, -1)) == m;
}

bool hasPlantNeighbor(ivec2 c) {
    return anyNeighbor(c, M_PLANT);
}

bool hasSupport(ivec2 c) {
    return anyNeighbor(c, M_WATER) ||
           (plant_wallSupport >= 0.5 && anyNeighbor(c, M_WALL));
}

uint react(uint self, ivec2 c) {
    if (self >= 12u) return M_EMPTY;
    if (self == M_WALL) return M_WALL;

    bool nearFire = anyNeighbor(c, M_FIRE);
    bool nearLava = anyNeighbor(c, M_LAVA);
    bool nearWater = anyNeighbor(c, M_WATER);
    bool nearAcid = anyNeighbor(c, M_ACID);
    bool nearIce = anyNeighbor(c, M_ICE);

    if (self == M_EMPTY) {
        if (hasPlantNeighbor(c) && hasSupport(c) && rand01(c, 10u) < plant_growthRate) return M_PLANT;
        return M_EMPTY;
    }
    if (self == M_PLANT) {
        if ((nearFire && rand01(c, 11u) < fire_ignitePlant) || nearLava) return M_FIRE;
        return M_PLANT;
    }
    if (self == M_OIL) {
        if ((nearFire || nearLava) && rand01(c, 12u) < oil_igniteRate) return M_FIRE;
        return M_OIL;
    }
    if (self == M_WATER) {
        if (nearLava) return M_STONE;
        if (nearFire) return M_WATER;
        if (nearAcid && rand01(c, 14u) < 0.35) return M_SMOKE;
        if (nearIce && rand01(c, 15u) < ice_freezeRate) return M_ICE;
        return M_WATER;
    }
    if (self == M_LAVA) {
        if (nearWater) return M_SMOKE;
        if (anyNeighbor(c, M_SAND)) return M_STONE;
        return M_LAVA;
    }
    if (self == M_SAND) {
        if (nearLava && rand01(c, 16u) < 0.55) return M_STONE;
        if (nearAcid && rand01(c, 17u) < 0.12) return M_EMPTY;
        return M_SAND;
    }
    if (self == M_STONE) {
        if (nearAcid && rand01(c, 18u) < acid_stoneCorrode) return M_EMPTY;
        return M_STONE;
    }
    if (self == M_ICE) {
        if (nearFire || nearLava) return M_WATER;
        if (nearAcid && rand01(c, 19u) < 0.20) return M_WATER;
        return M_ICE;
    }
    if (self == M_ACID) {
        if (nearWater && rand01(c, 20u) < 0.35) return M_SMOKE;
        return M_ACID;
    }
    if (self == M_FIRE) {
        if (nearWater || nearIce || nearAcid) {
            return rand01(c, 24u) < 0.35 ? M_SMOKE : M_EMPTY;
        }
        if (rand01(c, 21u) < fire_smokeRate) return M_SMOKE;
        return M_FIRE;
    }
    if (self == M_SMOKE) {
        if (nearIce && rand01(c, 22u) < 0.40) return M_WATER;
        if (rand01(c, 23u) < smoke_fadeRate) return M_EMPTY;
        return M_SMOKE;
    }
    return self;
}

void verticalSwap(inout uint bottom, inout uint top) {
    if (isMovable(top) && density(top) > density(bottom)) {
        uint t = bottom;
        bottom = top;
        top = t;
        return;
    }
    if (isMovable(bottom) && (bottom == M_FIRE || bottom == M_SMOKE) && density(bottom) < density(top)) {
        uint t = bottom;
        bottom = top;
        top = t;
    }
}

void horizontalSwap(inout uint left, inout uint right, ivec2 c, uint salt) {
    bool flip = (rng(c, salt) & 1u) != 0u;
    if (left == M_EMPTY && isLiquid(right) && flip && rand01(c, salt + 40u) < flowChance(right)) {
        left = right;
        right = M_EMPTY;
    } else if (right == M_EMPTY && isLiquid(left) && !flip && rand01(c, salt + 41u) < flowChance(left)) {
        right = left;
        left = M_EMPTY;
    } else if (left == M_EMPTY && (right == M_FIRE || right == M_SMOKE) && !flip &&
               rand01(c, salt + 42u) < flowChance(right)) {
        left = right;
        right = M_EMPTY;
    } else if (right == M_EMPTY && (left == M_FIRE || left == M_SMOKE) && flip &&
               rand01(c, salt + 43u) < flowChance(left)) {
        right = left;
        left = M_EMPTY;
    }
}

uint simulateBlockCell(ivec2 c) {
    ivec2 rel = c - uPhase;
    ivec2 block = ivec2(
        rel.x >= 0 ? (rel.x / 2) : ((rel.x - 1) / 2),
        rel.y >= 0 ? (rel.y / 2) : ((rel.y - 1) / 2)
    );
    ivec2 origin = block * 2 + uPhase;
    ivec2 local = c - origin;

    if (local.x < 0 || local.x > 1 || local.y < 0 || local.y > 1 ||
        origin.x < 0 || origin.y < 0 ||
        origin.x + 1 >= uGridSize.x || origin.y + 1 >= uGridSize.y) {
        return react(cell(c), c);
    }

    uint v00 = react(cell(origin + ivec2(0, 0)), origin + ivec2(0, 0));
    uint v10 = react(cell(origin + ivec2(1, 0)), origin + ivec2(1, 0));
    uint v01 = react(cell(origin + ivec2(0, 1)), origin + ivec2(0, 1));
    uint v11 = react(cell(origin + ivec2(1, 1)), origin + ivec2(1, 1));

    verticalSwap(v00, v01);
    verticalSwap(v10, v11);
    horizontalSwap(v00, v10, origin, 30u);
    horizontalSwap(v01, v11, origin + ivec2(0, 1), 31u);
    verticalSwap(v00, v01);
    verticalSwap(v10, v11);

    if (local.x == 0 && local.y == 0) return v00;
    if (local.x == 1 && local.y == 0) return v10;
    if (local.x == 0 && local.y == 1) return v01;
    return v11;
}

void main() {
    ivec2 c = ivec2(gl_FragCoord.xy);
    if (c.x < 0 || c.y < 0 || c.x >= uGridSize.x || c.y >= uGridSize.y) {
        outMat = M_EMPTY;
        return;
    }
    outMat = simulateBlockCell(c);
}
