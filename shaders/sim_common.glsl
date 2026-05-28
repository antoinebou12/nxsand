// Shared Margolus CA rules for sim.frag and sim.comp.
// Requires: sim_ids.glsl (included by wrapper before cell()), uGridSize, uPhase, uFrame,
// PhysicsBlock, and uint cell(ivec2 c).

layout(std140) uniform PhysicsBlock {
    float fire_speed;
    float fire_smokeRate;
    float fire_ignitePlant;
    float fire_igniteOil;
    float fire_spreadRate;
    float smoke_fadeRate;
    float smoke_driftRate;
    float water_flowRate;
    float acid_flowRate;
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
    float water_levelRate;
    float _physPad3;
    float _physPad4;
    float _physPad5;
};

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
    if (m == M_ACID) return acid_flowRate;
    if (m == M_LAVA) return lava_spreadRate;
    if (m == M_OIL) return oil_floatRate;
    if (m == M_FIRE) return fire_spreadRate;
    if (m == M_SMOKE) return smoke_driftRate;
    return 0.0;
}

float fallChance(uint m) {
    if (m == M_WATER) return 1.0;
    if (m == M_ACID) return 0.88;
    if (m == M_LAVA) return clamp(lava_flowRate + 0.45, 0.25, 0.95);
    if (m == M_OIL) return 0.55;
    if (m == M_FIRE || m == M_SMOKE) return fire_speed;
    return 1.0;
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

bool canDisplace(uint moving, uint target) {
    if (!isMovable(moving)) return false;
    if (target == M_WALL || target == M_PLANT || target == M_ICE) return false;
    return density(moving) > density(target);
}

bool isPowder(uint m) {
    return m == M_SAND || m == M_STONE;
}

float slideChance(uint m) {
    if (isPowder(m)) return clamp(fallChance(m) * 0.62, 0.55, 0.70);
    if (m == M_SMOKE) return max(0.14, flowChance(m) * 0.90);
    if (m == M_WATER) return max(0.70, flowChance(m));
    return max(0.30, flowChance(m));
}

bool isSolidSurface(uint m) {
    return m == M_WALL || m == M_STONE || m == M_SAND || m == M_PLANT || m == M_ICE;
}

// Horizontal gas drift: less skimming along floors; smoke clings beside wall.
float gasHorizFlow(uint gas, ivec2 pos, ivec2 dir) {
    float ch = flowChance(gas);
    if (gas != M_SMOKE && gas != M_FIRE) return ch;
    if (isSolidSurface(cell(pos + ivec2(0, -1)))) ch *= 0.55;
    if (gas == M_SMOKE) {
        uint block = cell(pos + ivec2(-dir.x, -dir.y));
        if (block == M_WALL) ch *= 0.42;
    }
    return ch;
}

bool anyNeighbor(ivec2 c, uint m) {
    return cell(c + ivec2( 1,  0)) == m ||
           cell(c + ivec2(-1,  0)) == m ||
           cell(c + ivec2( 0,  1)) == m ||
           cell(c + ivec2( 0, -1)) == m;
}

bool neighborHas(uint right, uint left, uint up, uint down, uint m) {
    return right == m || left == m || up == m || down == m;
}

bool inGrid(ivec2 c) {
    return c.x >= 0 && c.y >= 0 && c.x < uGridSize.x && c.y < uGridSize.y;
}

// Out-of-bounds reads are M_WALL for movement; plant wall support must ignore map edges.
bool nearWallInGrid(ivec2 c) {
    return (inGrid(c + ivec2( 1,  0)) && cell(c + ivec2( 1,  0)) == M_WALL) ||
           (inGrid(c + ivec2(-1,  0)) && cell(c + ivec2(-1,  0)) == M_WALL) ||
           (inGrid(c + ivec2( 0,  1)) && cell(c + ivec2( 0,  1)) == M_WALL) ||
           (inGrid(c + ivec2( 0, -1)) && cell(c + ivec2( 0, -1)) == M_WALL);
}

bool neighborHas8Fire(ivec2 c) {
    return anyNeighbor(c, M_FIRE) ||
           cell(c + ivec2( 1,  1)) == M_FIRE ||
           cell(c + ivec2(-1,  1)) == M_FIRE ||
           cell(c + ivec2( 1, -1)) == M_FIRE ||
           cell(c + ivec2(-1, -1)) == M_FIRE;
}

int countCardinal(uint right, uint left, uint up, uint down, uint m) {
    int n = 0;
    if (right == m) n++;
    if (left == m) n++;
    if (up == m) n++;
    if (down == m) n++;
    return n;
}

uint react(uint self, ivec2 c) {
    if (self >= 12u) return M_EMPTY;

    uint nRight = cell(c + ivec2( 1,  0));
    uint nLeft  = cell(c + ivec2(-1,  0));
    uint nUp    = cell(c + ivec2( 0,  1));
    uint nDown  = cell(c + ivec2( 0, -1));
    bool nearFire = neighborHas(nRight, nLeft, nUp, nDown, M_FIRE);
    bool nearLava = neighborHas(nRight, nLeft, nUp, nDown, M_LAVA);
    bool nearWater = neighborHas(nRight, nLeft, nUp, nDown, M_WATER);
    bool nearAcid = neighborHas(nRight, nLeft, nUp, nDown, M_ACID);
    bool nearIce = neighborHas(nRight, nLeft, nUp, nDown, M_ICE);
    bool nearOil = neighborHas(nRight, nLeft, nUp, nDown, M_OIL);
    bool nearPlant = neighborHas(nRight, nLeft, nUp, nDown, M_PLANT);
    bool nearSmoke = neighborHas(nRight, nLeft, nUp, nDown, M_SMOKE);
    bool nearWall = neighborHas(nRight, nLeft, nUp, nDown, M_WALL);
    bool nearStone = neighborHas(nRight, nLeft, nUp, nDown, M_STONE);
    bool nearSand = neighborHas(nRight, nLeft, nUp, nDown, M_SAND);

    if (self == M_EMPTY) {
        if (nearPlant) {
            bool wallSupport = plant_wallSupport >= 0.5 && nearWallInGrid(c);
            uint below = cell(c + ivec2(0, -1));
            bool waterBelow = below == M_WATER;
            bool wallClimb = wallSupport && (below == M_WALL || below == M_PLANT);
            float chance = 0.0;
            if (nearWater && waterBelow)
                chance = min(1.0, plant_growthRate * 14.0);
            else if (nearWater)
                chance = min(1.0, plant_growthRate * 9.0);
            if (wallSupport)
                chance = max(chance, min(1.0, plant_growthRate * 4.0));
            if (wallClimb)
                chance = max(chance, min(1.0, plant_growthRate * 7.0));
            if (chance > 0.0 && rand01(c, 10u) < chance)
                return M_PLANT;
        }
        return M_EMPTY;
    }
    if (self == M_WALL) {
        if (nearAcid && rand01(c, 25u) < acid_wallCorrode) return M_EMPTY;
        return M_WALL;
    }
    if (self == M_PLANT) {
        if (nearAcid && rand01(c, 27u) < 0.35) return M_EMPTY;
        if (nearLava) return M_FIRE;
        if (nearFire && fire_ignitePlant > 0.0) {
            float ignite = min(1.0, fire_ignitePlant * 2.5);
            if (countCardinal(nRight, nLeft, nUp, nDown, M_FIRE) >= 2)
                ignite = 1.0;
            if (rand01(c, 11u) < ignite) return M_FIRE;
        }
        if (neighborHas8Fire(c)) {
            float ignite = fire_ignitePlant * 0.85;
            if (countCardinal(nRight, nLeft, nUp, nDown, M_FIRE) >= 2)
                ignite = 1.0;
            if (rand01(c, 11u) < ignite) return M_FIRE;
        }
        if (nearSmoke && fire_ignitePlant > 0.0 &&
            rand01(c, 37u) < fire_ignitePlant * 0.40) return M_FIRE;
        return M_PLANT;
    }
    if (self == M_OIL) {
        if (nearFire) {
            float baseIgnite = max(oil_igniteRate, fire_igniteOil);
            float ignite = baseIgnite > 0.0 ? min(1.0, max(baseIgnite * 3.0, 0.55)) : 0.0;
            if (countCardinal(nRight, nLeft, nUp, nDown, M_FIRE) >= 2)
                ignite = min(1.0, ignite * 2.5);
            if (rand01(c, 12u) < ignite) return M_FIRE;
        }
        if (nearLava && rand01(c, 31u) < oil_igniteRate) return M_FIRE;
        return M_OIL;
    }
    if (self == M_WATER) {
        if (nearLava) {
            if (rand01(c, 32u) < 0.12) return M_SMOKE;
            return M_STONE;
        }
        if (nearFire) return M_WATER;
        if (nearAcid && rand01(c, 14u) < 0.35) return M_SMOKE;
        if (nearIce && rand01(c, 15u) < ice_freezeRate) return M_ICE;
        return M_WATER;
    }
    if (self == M_LAVA) {
        if (nearWater || nearIce) return M_SMOKE;
        if (nearSand) return M_STONE;
        if (nearOil && rand01(c, 28u) < lava_igniteGas) return M_FIRE;
        return M_LAVA;
    }
    if (self == M_SAND) {
        if (nearLava && rand01(c, 16u) < 0.55) return M_STONE;
        if (nearAcid && rand01(c, 17u) < 0.12) return M_EMPTY;
        return M_SAND;
    }
    if (self == M_STONE) {
        if (nearAcid && rand01(c, 18u) < acid_stoneCorrode) return M_EMPTY;
        if (nearLava && nearWater && rand01(c, 29u) < 0.04) return M_SMOKE;
        return M_STONE;
    }
    if (self == M_ICE) {
        if (nearWater && rand01(c, 38u) < 0.002) return M_WATER;
        if (nearFire && rand01(c, 33u) < ice_meltRate) return M_WATER;
        if (nearLava && rand01(c, 34u) < max(ice_meltRate, 0.08)) return M_WATER;
        if (nearAcid && rand01(c, 19u) < 0.20) return M_WATER;
        return M_ICE;
    }
    if (self == M_ACID) {
        if (nearWater && rand01(c, 20u) < 0.25) return M_SMOKE;
        if ((nearFire || nearLava) && rand01(c, 30u) < 0.18) return M_SMOKE;
        if ((nearWall || nearStone) && rand01(c, 35u) < 0.06)
            return M_SMOKE;
        return M_ACID;
    }
    if (self == M_FIRE) {
        if (nearWater || nearIce || nearAcid) {
            return rand01(c, 24u) < 0.35 ? M_SMOKE : M_EMPTY;
        }
        float burnout = (nearPlant || nearOil) ? fire_smokeRate * 0.35 : fire_smokeRate;
        if (rand01(c, 21u) < burnout) return M_SMOKE;
        return M_FIRE;
    }
    if (self == M_SMOKE) {
        if (nearIce && rand01(c, 22u) < 0.42) return M_WATER;
        float fade = smoke_fadeRate;
        if (nearWall)
            fade = smoke_fadeRate * 0.35;
        else if (nearStone || nearSand)
            fade = min(1.0, fade + 0.05);
        if (rand01(c, 23u) < fade) return M_EMPTY;
        return M_SMOKE;
    }
    return self;
}

void diagonalSwap(inout uint bottomA, inout uint topA, inout uint bottomB, inout uint topB,
                  ivec2 c, uint salt) {
    bool flip = (rng(c, salt) & 1u) != 0u;
    if (flip) {
        if (canDisplace(topA, bottomB) && rand01(c, salt + 50u) < slideChance(topA)) {
            uint t = bottomB;
            bottomB = topA;
            topA = t;
        } else if (canDisplace(topB, bottomA) && rand01(c, salt + 51u) < slideChance(topB)) {
            uint t = bottomA;
            bottomA = topB;
            topB = t;
        }
    } else {
        if (canDisplace(topB, bottomA) && rand01(c, salt + 52u) < slideChance(topB)) {
            uint t = bottomA;
            bottomA = topB;
            topB = t;
        } else if (canDisplace(topA, bottomB) && rand01(c, salt + 53u) < slideChance(topA)) {
            uint t = bottomB;
            bottomB = topA;
            topA = t;
        }
    }
}

void verticalSwap(inout uint bottom, inout uint top, ivec2 c, uint salt) {
    if (isMovable(top) && density(top) > density(bottom) && rand01(c, salt) < fallChance(top)) {
        uint t = bottom;
        bottom = top;
        top = t;
        return;
    }
    if (isMovable(bottom) && (bottom == M_FIRE || bottom == M_SMOKE) && top == M_EMPTY &&
        rand01(c, salt + 1u) < fallChance(bottom)) {
        uint t = bottom;
        bottom = top;
        top = t;
    }
}

bool liquidCanSpread(uint from, uint into) {
    if (into == M_EMPTY) return true;
    return isLiquid(into) && into == from;
}

bool cellEmpty(ivec2 p) {
    if (p.x < 0 || p.y < 0 || p.x >= uGridSize.x || p.y >= uGridSize.y) return false;
    return cell(p) == M_EMPTY;
}

float levelBoost(uint m) {
    if (m == M_WATER) return water_levelRate;
    if (m == M_ACID) return water_levelRate * 0.35;
    if (m == M_OIL) return water_levelRate * 0.15;
    return 0.0;
}

float boostedFlow(uint m, ivec2 pos, ivec2 dir) {
    float base = flowChance(m);
    float boost = levelBoost(m);
    if (boost <= 0.0) return base;
    if (cellEmpty(pos + dir) && cellEmpty(pos + dir * 2) && rand01(pos, 99u) < boost)
        return min(1.0, base + 0.95);
    if (m == M_WATER && cellEmpty(pos + dir)) {
        ivec2 ahead = pos + dir;
        if (isSolidSurface(cell(ahead + ivec2(0, -1))) && rand01(pos, 100u) < boost * 3.0)
            return min(1.0, base + 0.70);
    }
    return base;
}

void horizontalSwap(inout uint left, inout uint right, ivec2 c, uint salt) {
    bool flip = (rng(c, salt) & 1u) != 0u;
    ivec2 leftPos = c;
    ivec2 rightPos = c + ivec2(1, 0);
    if (isLiquid(left) && liquidCanSpread(left, right) && flip &&
        rand01(c, salt + 40u) < boostedFlow(left, leftPos, ivec2(1, 0))) {
        uint t = right;
        right = left;
        left = t;
    } else if (isLiquid(right) && liquidCanSpread(right, left) && !flip &&
               rand01(c, salt + 41u) < boostedFlow(right, rightPos, ivec2(-1, 0))) {
        uint t = left;
        left = right;
        right = t;
    } else if (left == M_EMPTY && (right == M_FIRE || right == M_SMOKE) && !flip &&
               rand01(c, salt + 42u) < gasHorizFlow(right, rightPos, ivec2(-1, 0))) {
        left = right;
        right = M_EMPTY;
    } else if (right == M_EMPTY && (left == M_FIRE || left == M_SMOKE) && flip &&
               rand01(c, salt + 43u) < gasHorizFlow(left, leftPos, ivec2(1, 0))) {
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

    verticalSwap(v00, v01, origin, 60u);
    verticalSwap(v10, v11, origin + ivec2(1, 0), 62u);
    diagonalSwap(v00, v01, v10, v11, origin, 32u);
    horizontalSwap(v00, v10, origin, 30u);
    horizontalSwap(v01, v11, origin + ivec2(0, 1), 31u);
    verticalSwap(v00, v01, origin + ivec2(0, 1), 64u);
    verticalSwap(v10, v11, origin + ivec2(1, 1), 66u);

    if (local.x == 0 && local.y == 0) return v00;
    if (local.x == 1 && local.y == 0) return v10;
    if (local.x == 0 && local.y == 1) return v01;
    return v11;
}
