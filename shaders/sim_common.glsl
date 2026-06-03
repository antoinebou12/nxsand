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
    float plant_bloomRate;
    float lava_flowRate;
    float lava_spreadRate;
    float lava_igniteGas;
    float oil_igniteRate;
    float oil_floatRate;
    float ice_meltRate;
    float ice_freezeRate;
    float water_levelRate;
    float sand_wetSlideScale;
    float sand_lithifyRate;
    float gunpowder_wetIgniteScale;
    float gunpowder_packBoost;
    float metal_rustRate;
    float metal_sparkRate;
    float oil_coldScale;
    float wood_charRate;
    float salt_dissolveRate;
    float ember_spawnRate;
    float ember_fadeRate;
    float ember_igniteWood;
    float coal_burnRate;
    float tnt_detonateRate;
    float brick_slideScale;
    float brick_cohesionScale;
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
    return m == M_EMPTY || m == M_FIRE || m == M_SMOKE || m == M_STEAM || m == M_EMBER;
}

bool isLiquid(uint m) {
    return m == M_WATER || m == M_ACID || m == M_LAVA || m == M_OIL;
}

bool isPlant(uint m) {
    return m == M_PLANT || m == M_FLOWER;
}

bool isStatic(uint m) {
    return m == M_WALL || isPlant(m) || m == M_ICE || m == M_GLASS || m == M_WOOD ||
           m == M_METAL || m == M_TNT;
}

bool isMovable(uint m) {
    return m == M_SAND || m == M_WATER || m == M_FIRE || m == M_SMOKE ||
           m == M_ACID || m == M_LAVA || m == M_STONE || m == M_OIL || m == M_STEAM ||
           m == M_GUNPOWDER || m == M_SALT || m == M_EMBER || m == M_COAL || m == M_BRICK;
}

float flowChance(uint m) {
    if (m == M_WATER) return water_flowRate;
    if (m == M_ACID) return acid_flowRate;
    if (m == M_LAVA) return lava_spreadRate;
    if (m == M_OIL) return oil_floatRate;
    if (m == M_FIRE) return fire_spreadRate;
    if (m == M_EMBER) return fire_spreadRate * 1.2;
    if (m == M_SMOKE) return smoke_driftRate;
    if (m == M_STEAM) return smoke_driftRate * 1.1;
    return 0.0;
}

float fallChance(uint m) {
    if (m == M_WATER) return 1.0;
    if (m == M_ACID) return 0.88;
    if (m == M_LAVA) return clamp(lava_flowRate + 0.45, 0.25, 0.95);
    if (m == M_OIL) return 0.46;
    if (m == M_FIRE || m == M_SMOKE || m == M_EMBER) return fire_speed;
    if (m == M_STEAM) return fire_speed * 0.9;
    return 1.0;
}

int density(uint m) {
    if (m == M_FIRE || m == M_SMOKE || m == M_STEAM || m == M_EMBER) return 0;
    if (m == M_EMPTY) return 1;
    if (m == M_OIL) return 2;
    if (m == M_SALT) return 2;
    if (m == M_WATER || m == M_ACID || m == M_LAVA) return 3;
    if (m == M_SAND) return 4;
    if (m == M_GUNPOWDER || m == M_COAL) return 4;
    if (m == M_STONE) return 5;
    if (m == M_BRICK) return 6;
    return 9;
}

bool canDisplace(uint moving, uint target) {
    if (!isMovable(moving)) return false;
    if (target == M_WALL || isPlant(target) || target == M_ICE ||
        target == M_GLASS || target == M_WOOD || target == M_METAL || target == M_TNT)
        return false;
    return density(moving) > density(target);
}

bool isPowder(uint m) {
    return m == M_SAND || m == M_STONE || m == M_GUNPOWDER || m == M_SALT || m == M_COAL;
}

bool sandNearWater(ivec2 pos) {
    if (pos.x < 0 || pos.y < 0 || pos.x >= uGridSize.x || pos.y >= uGridSize.y) return false;
    return cell(pos + ivec2( 1,  0)) == M_WATER ||
           cell(pos + ivec2(-1,  0)) == M_WATER ||
           cell(pos + ivec2( 0,  1)) == M_WATER ||
           cell(pos + ivec2( 0, -1)) == M_WATER;
}

bool oilNearIce(ivec2 pos) {
    if (pos.x < 0 || pos.y < 0 || pos.x >= uGridSize.x || pos.y >= uGridSize.y) return false;
    return cell(pos + ivec2( 1,  0)) == M_ICE ||
           cell(pos + ivec2(-1,  0)) == M_ICE ||
           cell(pos + ivec2( 0,  1)) == M_ICE ||
           cell(pos + ivec2( 0, -1)) == M_ICE;
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

int countDiagonal(ivec2 c, uint m) {
    int n = 0;
    if (cell(c + ivec2( 1,  1)) == m) n++;
    if (cell(c + ivec2(-1,  1)) == m) n++;
    if (cell(c + ivec2( 1, -1)) == m) n++;
    if (cell(c + ivec2(-1, -1)) == m) n++;
    return n;
}

bool gunpowderNeighborHot(ivec2 n) {
    if (!inGrid(n) || cell(n) != M_GUNPOWDER) return false;
    return anyNeighbor(n, M_FIRE) || anyNeighbor(n, M_LAVA);
}

bool gunpowderChainHeat(ivec2 c) {
    return gunpowderNeighborHot(c + ivec2( 1,  0)) ||
           gunpowderNeighborHot(c + ivec2(-1,  0)) ||
           gunpowderNeighborHot(c + ivec2( 0,  1)) ||
           gunpowderNeighborHot(c + ivec2( 0, -1));
}

const int TNT_BLAST_R = 6;

bool tntContactNeighbor(uint m) {
    return m != M_EMPTY && m != M_TNT && m != M_EMBER && m != M_WALL;
}

bool tntContactFuse(ivec2 p) {
    return tntContactNeighbor(cell(p + ivec2( 1,  0))) ||
           tntContactNeighbor(cell(p + ivec2(-1,  0))) ||
           tntContactNeighbor(cell(p + ivec2( 0,  1))) ||
           tntContactNeighbor(cell(p + ivec2( 0, -1))) ||
           tntContactNeighbor(cell(p + ivec2( 1,  1))) ||
           tntContactNeighbor(cell(p + ivec2(-1,  1))) ||
           tntContactNeighbor(cell(p + ivec2( 1, -1))) ||
           tntContactNeighbor(cell(p + ivec2(-1, -1)));
}

bool tntHeatFuseOnly(ivec2 p) {
    return anyNeighbor(p, M_FIRE) || anyNeighbor(p, M_LAVA) || anyNeighbor(p, M_EMBER) ||
           neighborHas8Fire(p);
}

bool tntFuseCandidate(ivec2 p) {
    if (!inGrid(p) || cell(p) != M_TNT) return false;
    return tntContactFuse(p) || tntHeatFuseOnly(p);
}

bool tntNeighborHot(ivec2 n) {
    if (!inGrid(n) || cell(n) != M_TNT) return false;
    return tntHeatFuseOnly(n) || tntContactFuse(n);
}

bool tntChainHeat(ivec2 c) {
    return tntNeighborHot(c + ivec2( 1,  0)) ||
           tntNeighborHot(c + ivec2(-1,  0)) ||
           tntNeighborHot(c + ivec2( 0,  1)) ||
           tntNeighborHot(c + ivec2( 0, -1)) ||
           tntFuseCandidate(c + ivec2( 1,  0)) ||
           tntFuseCandidate(c + ivec2(-1,  0)) ||
           tntFuseCandidate(c + ivec2( 0,  1)) ||
           tntFuseCandidate(c + ivec2( 0, -1));
}

bool tntHasFuse(ivec2 p) {
    if (!inGrid(p) || cell(p) != M_TNT) return false;
    return tntFuseCandidate(p) || tntChainHeat(p);
}

float tntBlastStrengthNearby(ivec2 c, int nearTnt, bool nearFireIn, bool nearEmberIn) {
    if (nearTnt < 1 && !nearFireIn && !nearEmberIn) return 0.0;
    float strength = 0.0;
    for (int dy = -TNT_BLAST_R; dy <= TNT_BLAST_R; dy++) {
        for (int dx = -TNT_BLAST_R; dx <= TNT_BLAST_R; dx++) {
            int d = max(abs(dx), abs(dy));
            if (d > TNT_BLAST_R) continue;
            ivec2 p = c + ivec2(dx, dy);
            if (!inGrid(p)) continue;
            uint m = cell(p);
            float src = 0.0;
            if (m == M_TNT && tntFuseCandidate(p))
                src = 1.0;
            else if (m == M_FIRE || m == M_EMBER) {
                int tntN = countCardinal(
                               cell(p + ivec2( 1,  0)), cell(p + ivec2(-1,  0)),
                               cell(p + ivec2( 0,  1)), cell(p + ivec2( 0, -1)), M_TNT) +
                           countDiagonal(p, M_TNT);
                if (tntN >= 1) src = 0.9;
            }
            if (src <= 0.0) continue;
            strength = max(strength, src * (1.0 - float(d) / float(TNT_BLAST_R)));
            if (strength >= 0.95) return strength;
        }
    }
    return strength;
}

float flowChanceAt(uint m, ivec2 pos) {
    float ch = flowChance(m);
    if (m == M_OIL && oil_coldScale > 0.0 && oilNearIce(pos))
        return ch * oil_coldScale;
    return ch;
}

float slideChance(uint m) {
    if (m == M_BRICK && brick_slideScale > 0.0)
        return clamp(brick_slideScale, 0.08, 0.45);
    if (isPowder(m)) return clamp(fallChance(m) * 0.62, 0.55, 0.70);
    if (m == M_SMOKE || m == M_STEAM) return max(0.14, flowChance(m) * 0.90);
    if (m == M_WATER) return max(0.92, flowChance(m));
    return max(0.30, flowChance(m));
}

float slideChanceAt(uint m, ivec2 pos) {
    float ch = slideChance(m);
    if (m == M_SAND && sand_wetSlideScale > 0.0 && sandNearWater(pos))
        return clamp(ch * sand_wetSlideScale, 0.12, ch);
    if (m == M_BRICK && brick_cohesionScale > 0.0) {
        int brickN = countCardinal(
            cell(pos + ivec2(1, 0)), cell(pos + ivec2(-1, 0)),
            cell(pos + ivec2(0, 1)), cell(pos + ivec2(0, -1)), M_BRICK);
        if (brickN >= 2)
            ch = clamp(ch * brick_cohesionScale, 0.06, ch);
    }
    return ch;
}

float fallChanceAt(uint m, ivec2 pos) {
    if (m == M_SAND && sand_wetSlideScale > 0.0 && sandNearWater(pos))
        return clamp(fallChance(m) * sand_wetSlideScale, 0.15, 1.0);
    if (m == M_OIL && oil_coldScale > 0.0 && oilNearIce(pos))
        return clamp(fallChance(m) * oil_coldScale, 0.12, 1.0);
    return fallChance(m);
}

bool isSolidSurface(uint m) {
    return m == M_WALL || m == M_STONE || m == M_SAND || m == M_SALT || isPlant(m) || m == M_ICE ||
           m == M_GLASS || m == M_WOOD || m == M_METAL || m == M_TNT || m == M_BRICK;
}

bool nearPlantNeigh(uint right, uint left, uint up, uint down) {
    return neighborHas(right, left, up, down, M_PLANT) ||
           neighborHas(right, left, up, down, M_FLOWER);
}

// Horizontal gas drift: less skimming along floors; smoke clings beside wall.
float gasHorizFlow(uint gas, ivec2 pos, ivec2 dir) {
    float ch = flowChance(gas);
    if (gas != M_SMOKE && gas != M_FIRE && gas != M_STEAM && gas != M_EMBER) return ch;
    if (isSolidSurface(cell(pos + ivec2(0, -1)))) ch *= 0.55;
    if (gas == M_SMOKE || gas == M_STEAM) {
        uint block = cell(pos + ivec2(-dir.x, -dir.y));
        if (block == M_WALL) ch *= 0.42;
    }
    return ch;
}

uint react(uint self, ivec2 c) {
    if (self > 22u) return M_EMPTY;

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
    bool nearPlant = nearPlantNeigh(nRight, nLeft, nUp, nDown);
    bool nearSmoke = neighborHas(nRight, nLeft, nUp, nDown, M_SMOKE);
    bool nearWall = neighborHas(nRight, nLeft, nUp, nDown, M_WALL);
    bool nearStone = neighborHas(nRight, nLeft, nUp, nDown, M_STONE);
    bool nearBrick = neighborHas(nRight, nLeft, nUp, nDown, M_BRICK);
    bool nearSand = neighborHas(nRight, nLeft, nUp, nDown, M_SAND);
    bool nearEmber = neighborHas(nRight, nLeft, nUp, nDown, M_EMBER);
    int nearTnt = countCardinal(nRight, nLeft, nUp, nDown, M_TNT) +
                  countDiagonal(c, M_TNT);

    if (self != M_WALL && self != M_STONE && self != M_BRICK && self != M_METAL && self != M_TNT &&
        (nearTnt >= 1 || nearFire || nearEmber) &&
        (!isStatic(self) || self == M_WOOD || self == M_GLASS)) {
        float blast = tntBlastStrengthNearby(c, nearTnt, nearFire, nearEmber);
        if (blast >= 0.22) {
            if (rand01(c, 82u) < min(1.0, 0.70 + blast * 0.30)) return M_FIRE;
            if (rand01(c, 83u) < min(1.0, 0.55 + blast * 0.45)) return M_EMBER;
            if (isPowder(self) || isLiquid(self)) {
                if (rand01(c, 84u) < min(1.0, blast * 2.8)) return M_EMPTY;
            }
        } else if (blast >= 0.06) {
            if (isPowder(self) || isLiquid(self)) {
                if (rand01(c, 84u) < min(1.0, blast * 3.5)) return M_EMPTY;
                if (rand01(c, 85u) < min(1.0, blast * 2.8)) return M_FIRE;
            }
            if (self == M_WOOD || self == M_GLASS) {
                if (rand01(c, 86u) < min(1.0, blast * 2.0)) return M_FIRE;
            }
            if (isGas(self) && self != M_EMPTY) {
                if (rand01(c, 87u) < min(1.0, blast * 2.5)) return M_EMPTY;
            }
        }
    }

    if (self == M_EMPTY) {
        if (nearFire && ember_spawnRate > 0.0 && rand01(c, 59u) < ember_spawnRate)
            return M_EMBER;
        if ((nearFire || nearLava) && nearWater && rand01(c, 41u) < 0.20)
            return M_STEAM;
        int nearGnp = countCardinal(nRight, nLeft, nUp, nDown, M_GUNPOWDER) +
                      countDiagonal(c, M_GUNPOWDER);
        if (nearGnp >= 2 && (nearFire || nearLava)) {
            if (rand01(c, 55u) < 0.22) return M_SMOKE;
            if (rand01(c, 60u) < 0.12) return M_EMBER;
            if (nearGnp >= 3 && rand01(c, 56u) < 0.10) return M_FIRE;
        }
        if (nearTnt >= 1 || nearFire || nearEmber) {
            float blast = tntBlastStrengthNearby(c, nearTnt, nearFire, nearEmber);
            if (blast > 0.0) {
                if (rand01(c, 73u) < min(1.0, 0.35 + 0.55 * blast)) return M_SMOKE;
                if (rand01(c, 74u) < min(1.0, 0.20 + 0.45 * blast)) return M_EMBER;
                if (rand01(c, 75u) < min(1.0, 0.30 + 0.55 * blast)) return M_FIRE;
            }
        }
        if (nearPlant) {
            bool wallSupport = plant_wallSupport >= 0.5 && nearWallInGrid(c);
            uint below = cell(c + ivec2(0, -1));
            bool waterBelow = below == M_WATER;
            bool wallClimb = wallSupport && (below == M_WALL || isPlant(below));
            float chance = 0.0;
            if (nearWater && waterBelow)
                chance = min(1.0, plant_growthRate * 9.0);
            else if (nearWater)
                chance = min(1.0, plant_growthRate * 6.0);
            if (wallSupport)
                chance = max(chance, min(1.0, plant_growthRate * 3.0));
            if (wallClimb)
                chance = max(chance, min(1.0, plant_growthRate * 5.0));
            if (chance > 0.0 && rand01(c, 10u) < chance)
                return M_PLANT;
        }
        return M_EMPTY;
    }
    if (self == M_WALL) {
        if (nearAcid && rand01(c, 25u) < acid_wallCorrode) return M_EMPTY;
        return M_WALL;
    }
    if (self == M_PLANT || self == M_FLOWER) {
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
        if (self == M_PLANT && plant_bloomRate > 0.0) {
            int waterCard = countCardinal(nRight, nLeft, nUp, nDown, M_WATER);
            int waterDiag = countDiagonal(c, M_WATER);
            int wet = waterCard + waterDiag;
            if (wet >= 3) {
                bool lush = waterCard >= 2 &&
                            (nDown == M_WATER || nUp == M_WATER || waterDiag >= 2);
                bool tip = nUp == M_EMPTY;
                if (lush || tip) {
                    float bloom = min(1.0, plant_bloomRate * (0.40 + 0.11 * float(wet)));
                    if (tip) bloom = min(1.0, bloom * 1.4);
                    if (rand01(c, 72u) < bloom) return M_FLOWER;
                }
            }
        }
        return self;
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
        if (nearSand) return M_GLASS;
        if (nearOil && rand01(c, 28u) < lava_igniteGas) return M_FIRE;
        return M_LAVA;
    }
    if (self == M_SAND) {
        if (nearLava && rand01(c, 16u) < 0.55) return M_GLASS;
        if (nearAcid && rand01(c, 17u) < 0.12) return M_EMPTY;
        if (nearWater && sand_lithifyRate > 0.0 && rand01(c, 51u) < sand_lithifyRate)
            return M_STONE;
        return M_SAND;
    }
    if (self == M_STONE) {
        if (nearAcid && rand01(c, 18u) < acid_stoneCorrode) return M_EMPTY;
        if (nearLava && nearWater && rand01(c, 29u) < 0.04) return M_SMOKE;
        return M_STONE;
    }
    if (self == M_BRICK) {
        if (nearAcid && rand01(c, 82u) < acid_stoneCorrode * 0.5) return M_EMPTY;
        if (nearLava && nearWater && rand01(c, 83u) < 0.04) return M_SMOKE;
        return M_BRICK;
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
        if ((nearWall || nearStone || nearBrick) && rand01(c, 35u) < 0.06)
            return M_SMOKE;
        return M_ACID;
    }
    if (self == M_FIRE) {
        if (nearWater || nearIce || nearAcid) {
            float r = rand01(c, 24u);
            if (nearWater && r < 0.15) return M_STEAM;
            return r < 0.35 ? M_SMOKE : M_EMPTY;
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
        else if (nearStone || nearSand || nearBrick)
            fade = min(1.0, fade + 0.05);
        if (rand01(c, 23u) < fade) return M_EMPTY;
        return M_SMOKE;
    }
    if (self == M_STEAM) {
        if (nearIce && rand01(c, 39u) < 0.35) return M_WATER;
        float fade = smoke_fadeRate * 1.8;
        if (nearWall)
            fade = smoke_fadeRate * 0.55;
        else if (nearStone || nearSand || nearBrick)
            fade = min(1.0, fade + 0.04);
        if (rand01(c, 40u) < fade) return M_EMPTY;
        return M_STEAM;
    }
    if (self == M_EMBER) {
        if (rand01(c, 62u) < ember_fadeRate) return M_EMPTY;
        return M_EMBER;
    }
    if (self == M_GLASS) {
        if (nearAcid && rand01(c, 26u) < acid_wallCorrode * 0.4) return M_EMPTY;
        return M_GLASS;
    }
    if (self == M_WOOD) {
        if (nearAcid && rand01(c, 36u) < 0.25) return M_EMPTY;
        if (nearSmoke && !nearFire && !nearLava && wood_charRate > 0.0 &&
            rand01(c, 52u) < wood_charRate)
            return M_COAL;
        if (nearLava) return M_FIRE;
        if (nearEmber && ember_igniteWood > 0.0 && rand01(c, 61u) < ember_igniteWood)
            return M_FIRE;
        if (nearFire && fire_ignitePlant > 0.0) {
            float ignite = min(1.0, fire_ignitePlant * 2.0);
            if (countCardinal(nRight, nLeft, nUp, nDown, M_FIRE) >= 2)
                ignite = 1.0;
            if (rand01(c, 13u) < ignite) return M_FIRE;
            if (wood_charRate > 0.0 && rand01(c, 89u) < wood_charRate * 0.65)
                return M_COAL;
        }
        return M_WOOD;
    }
    if (self == M_METAL) {
        if (nearAcid && rand01(c, 48u) < acid_wallCorrode * 0.25) return M_EMPTY;
        if (nearWater && metal_rustRate > 0.0 && rand01(c, 53u) < metal_rustRate)
            return M_STONE;
        int heatNeighbors = countCardinal(nRight, nLeft, nUp, nDown, M_FIRE) +
                            countCardinal(nRight, nLeft, nUp, nDown, M_LAVA);
        if (heatNeighbors >= 2 && metal_sparkRate > 0.0 && rand01(c, 49u) < metal_sparkRate)
            return M_FIRE;
        return M_METAL;
    }
    if (self == M_GUNPOWDER) {
        bool heat = nearFire || nearLava || gunpowderChainHeat(c);
        if (heat) {
            int packC = countCardinal(nRight, nLeft, nUp, nDown, M_GUNPOWDER);
            int pack8 = packC + countDiagonal(c, M_GUNPOWDER);
            float ignite = nearLava ? 0.92 : 0.78;
            if (countCardinal(nRight, nLeft, nUp, nDown, M_FIRE) >= 2 || packC >= 2)
                ignite = min(1.0, ignite + gunpowder_packBoost);
            if (pack8 >= 5)
                ignite = 1.0;
            else if (pack8 >= 3)
                ignite = min(1.0, ignite + 0.28);
            if (nearWater && gunpowder_wetIgniteScale >= 0.0)
                ignite *= gunpowder_wetIgniteScale;
            if (rand01(c, 46u) < ignite) return M_FIRE;
            float blast = packC >= 2 ? 0.58 : 0.28;
            if (rand01(c, 47u) < blast) return M_SMOKE;
        }
        if (nearAcid && rand01(c, 50u) < 0.18) return M_EMPTY;
        return M_GUNPOWDER;
    }
    if (self == M_COAL) {
        bool heat = nearFire || nearLava || nearEmber;
        if (heat && coal_burnRate > 0.0) {
            float burn = min(1.0, coal_burnRate * (nearLava ? 3.5 : 1.0));
            if (countCardinal(nRight, nLeft, nUp, nDown, M_FIRE) >= 2)
                burn = min(1.0, burn * 2.0);
            if (nearWater)
                burn *= 0.15;
            if (rand01(c, 76u) < burn) return M_FIRE;
            if (rand01(c, 77u) < burn * 0.45) return M_SMOKE;
        }
        if (nearAcid && rand01(c, 78u) < 0.12) return M_EMPTY;
        return M_COAL;
    }
    if (self == M_TNT) {
        if (tntHasFuse(c) && tnt_detonateRate > 0.0) {
            bool contact = tntContactFuse(c);
            int packC = countCardinal(nRight, nLeft, nUp, nDown, M_TNT);
            int pack8 = packC + countDiagonal(c, M_TNT);
            float det = contact ? 1.0 : tnt_detonateRate;
            if (!contact && nearLava)
                det = min(1.0, det * 1.35);
            if (!contact && (nearFire || nearEmber))
                det = min(1.0, det + 0.28);
            if (!contact && countCardinal(nRight, nLeft, nUp, nDown, M_FIRE) >= 1)
                det = min(1.0, det + 0.22);
            if (!contact && countCardinal(nRight, nLeft, nUp, nDown, M_FIRE) >= 2)
                det = min(1.0, det + 0.35);
            if (packC >= 2)
                det = 1.0;
            if (pack8 >= 3)
                det = 1.0;
            if (tntChainHeat(c) && packC >= 1)
                det = 1.0;
            if (rand01(c, 79u) < det)
                return M_FIRE;
            float blast = packC >= 2 ? 0.72 : 0.48;
            if (rand01(c, 80u) < blast) return M_SMOKE;
            if (rand01(c, 88u) < 0.35) return M_EMBER;
        }
        if (nearAcid && rand01(c, 81u) < 0.08) return M_EMPTY;
        return M_TNT;
    }
    if (self == M_SALT) {
        if (nearWater && salt_dissolveRate > 0.0) {
            int waterN = countCardinal(nRight, nLeft, nUp, nDown, M_WATER);
            float dissolve = salt_dissolveRate;
            if (waterN >= 3) dissolve = min(1.0, dissolve * 2.5);
            else if (waterN >= 2) dissolve = min(1.0, dissolve * 1.6);
            if (rand01(c, 57u) < dissolve) return M_EMPTY;
        }
        if (nearAcid && rand01(c, 58u) < 0.15) return M_EMPTY;
        return M_SALT;
    }
    return self;
}

void diagonalSwap(inout uint bottomA, inout uint topA, inout uint bottomB, inout uint topB,
                  ivec2 c, uint salt) {
    ivec2 posTopA = c + ivec2(0, 1);
    ivec2 posTopB = c + ivec2(1, 1);
    ivec2 posBottomA = c;
    ivec2 posBottomB = c + ivec2(1, 0);
    bool flip = (rng(c, salt) & 1u) != 0u;
    if (flip) {
        if (canDisplace(topA, bottomB) && rand01(c, salt + 50u) < slideChanceAt(topA, posTopA)) {
            uint t = bottomB;
            bottomB = topA;
            topA = t;
        } else if (canDisplace(topB, bottomA) && rand01(c, salt + 51u) < slideChanceAt(topB, posTopB)) {
            uint t = bottomA;
            bottomA = topB;
            topB = t;
        }
    } else {
        if (canDisplace(topB, bottomA) && rand01(c, salt + 52u) < slideChanceAt(topB, posTopB)) {
            uint t = bottomA;
            bottomA = topB;
            topB = t;
        } else if (canDisplace(topA, bottomB) && rand01(c, salt + 53u) < slideChanceAt(topA, posTopA)) {
            uint t = bottomB;
            bottomB = topA;
            topA = t;
        }
    }
}

void verticalSwap(inout uint bottom, inout uint top, ivec2 bottomPos, uint salt) {
    ivec2 topPos = bottomPos + ivec2(0, 1);
    if (isMovable(top) && density(top) > density(bottom) &&
        rand01(bottomPos, salt) < fallChanceAt(top, topPos)) {
        uint t = bottom;
        bottom = top;
        top = t;
        return;
    }
    if (isMovable(bottom) && (bottom == M_FIRE || bottom == M_SMOKE || bottom == M_STEAM ||
        bottom == M_EMBER) &&
        top == M_EMPTY &&
        rand01(bottomPos, salt + 1u) < fallChanceAt(bottom, bottomPos)) {
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
    if (m == M_OIL) return water_levelRate * 0.10;
    return 0.0;
}

float boostedFlow(uint m, ivec2 pos, ivec2 dir) {
    float base = flowChanceAt(m, pos);
    float boost = levelBoost(m);
    if (boost <= 0.0) return base;
    if (cellEmpty(pos + dir) && cellEmpty(pos + dir * 2) &&
        rand01(pos, 99u) < min(1.0, boost * 3.0))
        return min(1.0, base + 0.95);
    if (m == M_WATER && cellEmpty(pos + dir)) {
        ivec2 ahead = pos + dir;
        if (isSolidSurface(cell(ahead + ivec2(0, -1))) &&
            rand01(pos, 100u) < min(1.0, boost * 6.0))
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
    } else if (left == M_EMPTY && (right == M_FIRE || right == M_SMOKE || right == M_STEAM ||
               right == M_EMBER) && !flip &&
               rand01(c, salt + 42u) < gasHorizFlow(right, rightPos, ivec2(-1, 0))) {
        left = right;
        right = M_EMPTY;
    } else if (right == M_EMPTY && (left == M_FIRE || left == M_SMOKE || left == M_STEAM ||
               left == M_EMBER) && flip &&
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
