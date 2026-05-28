#version 300 es
precision highp float;
precision highp int;
precision highp usampler2D;

in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

uniform usampler2D uSim;
uniform sampler2D  uPalette;
uniform ivec2      uGridSize;
uniform uint       uFrame;
uniform int        uPaletteMode;
uniform int        uFlicker;
uniform int        uGrain;
uniform int        uBlob;
uniform float      uAoStrength;

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

uint cellAt(ivec2 c) {
    if (c.x < 0 || c.y < 0 || c.x >= uGridSize.x || c.y >= uGridSize.y) return M_EMPTY;
    return texelFetch(uSim, c, 0).r;
}

float grain(ivec2 c) {
    uint x = uint(c.x) * 1103515245u + uint(c.y) * 12345u + uFrame * 2654435761u;
    x ^= x >> 16u;
    x *= 2246822519u;
    x ^= x >> 13u;
    return float(x & 255u) / 255.0;
}

bool isEmissive(uint m) {
    return m == M_FIRE || m == M_LAVA || m == M_SMOKE;
}

vec3 emissiveTint(uint m) {
    if (m == M_FIRE) return vec3(1.0, 0.45, 0.10);
    if (m == M_LAVA) return vec3(1.0, 0.35, 0.06);
    return vec3(0.35, 0.32, 0.38);
}

vec3 materialColor(uint m, float n) {
    if (m == M_SAND)  return mix(vec3(0.50, 0.36, 0.16), vec3(0.94, 0.74, 0.33), n);
    if (m == M_WATER) return mix(vec3(0.02, 0.13, 0.36), vec3(0.10, 0.46, 0.86), n * 0.55);
    if (m == M_FIRE)  return mix(vec3(0.90, 0.08, 0.02), vec3(1.00, 0.56, 0.08), n);
    if (m == M_SMOKE) return mix(vec3(0.13, 0.13, 0.15), vec3(0.50, 0.52, 0.56), n);
    if (m == M_WALL)  return mix(vec3(0.30, 0.32, 0.35), vec3(0.58, 0.60, 0.64), n);
    if (m == M_ACID)  return mix(vec3(0.06, 0.55, 0.08), vec3(0.58, 0.96, 0.20), n);
    if (m == M_PLANT) return mix(vec3(0.05, 0.27, 0.08), vec3(0.30, 0.70, 0.22), n);
    if (m == M_LAVA)  return mix(vec3(0.52, 0.03, 0.00), vec3(1.00, 0.52, 0.04), n);
    if (m == M_STONE) return mix(vec3(0.25, 0.29, 0.33), vec3(0.52, 0.58, 0.64), n);
    if (m == M_OIL)   return mix(vec3(0.01, 0.01, 0.03), vec3(0.06, 0.08, 0.18), n);
    if (m == M_ICE)   return mix(vec3(0.46, 0.74, 0.94), vec3(0.88, 0.98, 1.00), n);
    return texture(uPalette, vec2((float(m) + 0.5) / 256.0, 0.5)).rgb;
}

vec3 emptyFringeHalo(ivec2 c) {
    vec3 halo = vec3(0.0);
    const float wCard = 0.088;
    const float wDiag = 0.044;
    halo += emissiveTint(cellAt(c + ivec2(1, 0))) * wCard * float(isEmissive(cellAt(c + ivec2(1, 0))));
    halo += emissiveTint(cellAt(c + ivec2(-1, 0))) * wCard * float(isEmissive(cellAt(c + ivec2(-1, 0))));
    halo += emissiveTint(cellAt(c + ivec2(0, 1))) * wCard * float(isEmissive(cellAt(c + ivec2(0, 1))));
    halo += emissiveTint(cellAt(c + ivec2(0, -1))) * wCard * float(isEmissive(cellAt(c + ivec2(0, -1))));
    halo += emissiveTint(cellAt(c + ivec2(1, 1))) * wDiag * float(isEmissive(cellAt(c + ivec2(1, 1))));
    halo += emissiveTint(cellAt(c + ivec2(-1, 1))) * wDiag * float(isEmissive(cellAt(c + ivec2(-1, 1))));
    halo += emissiveTint(cellAt(c + ivec2(1, -1))) * wDiag * float(isEmissive(cellAt(c + ivec2(1, -1))));
    halo += emissiveTint(cellAt(c + ivec2(-1, -1))) * wDiag * float(isEmissive(cellAt(c + ivec2(-1, -1))));
    return halo;
}

void main() {
    vec2 uv = clamp(v_uv, vec2(0.0), vec2(1.0));
    ivec2 c = ivec2(floor(uv * vec2(uGridSize)));
    c = clamp(c, ivec2(0), uGridSize - ivec2(1));

    uint m = cellAt(c);
    float n = grain(c);

    float minor = (mod(float(c.x), 16.0) < 1.0 || mod(float(c.y), 16.0) < 1.0) ? 0.018 : 0.0;
    float major = (mod(float(c.x), 64.0) < 1.0 || mod(float(c.y), 64.0) < 1.0) ? 0.045 : 0.0;
    vec3 bg = vec3(0.058, 0.073, 0.105) + vec3(minor + major);

    if (uPaletteMode == 2) {
        float t = float(m) / 13.0;
        fragColor = vec4(t, 1.0 - t, float(m & 1u), 1.0);
        return;
    }

    bool fancy = (uPaletteMode != 1 && uBlob != 0);

    if (m == M_EMPTY) {
        vec3 col = bg;
        if (fancy) col += emptyFringeHalo(c);
        fragColor = vec4(col, 1.0);
        return;
    }

    vec3 col = materialColor(m, n);
    float openTop = float(cellAt(c + ivec2(0, 1)) == M_EMPTY);
    float openLeft = float(cellAt(c + ivec2(-1, 0)) == M_EMPTY);
    float openRight = float(cellAt(c + ivec2(1, 0)) == M_EMPTY);
    float openBottom = float(cellAt(c + ivec2(0, -1)) == M_EMPTY);
    float shade = openTop * 0.10 + openLeft * 0.04 - (1.0 - openBottom) * 0.04;
    shade += uAoStrength * (openTop + openLeft + openRight + openBottom);
    col += vec3(shade);

    float flick = 1.0;
    if (uFlicker != 0 && (m == M_FIRE || m == M_LAVA)) {
        flick = sin(float(uFrame) * 0.35 + float(c.x) * 0.2 + float(c.y) * 0.17) * 0.5 + 0.5;
        col *= 0.88 + flick * 0.14;
    }

    if (fancy) {
        if (m == M_WATER) {
            if (cellAt(c + ivec2(0, -1)) == M_WATER) col *= 0.92;
            if (cellAt(c + ivec2(0, 1)) == M_EMPTY) col += vec3(0.05, 0.07, 0.09);
        }
        if (m == M_SMOKE) {
            float openCount = openTop + openLeft + openRight + openBottom;
            col += vec3(0.02, 0.03, 0.06) * openCount * 0.14;
            col = mix(col, col * 1.12, 0.22);
        }
        if (m == M_ICE) {
            if (openTop + openLeft + openRight + openBottom > 0.0)
                col += vec3(0.04, 0.07, 0.10) * 0.35;
        }
        if (m == M_ACID || m == M_PLANT) {
            float sh = sin(float(uFrame) * 0.2 + float(c.x) * 0.15 + n * 6.28318) * 0.5 + 0.5;
            col += vec3(0.03, 0.05, 0.02) * sh;
        }
        if (isEmissive(m)) {
            float sameN = float(cellAt(c + ivec2(1, 0)) == m) + float(cellAt(c + ivec2(-1, 0)) == m) +
                        float(cellAt(c + ivec2(0, 1)) == m) + float(cellAt(c + ivec2(0, -1)) == m);
            col += col * min(sameN * 0.04, 0.14);
            float emptyN = openTop + openLeft + openRight + openBottom;
            col += col * emptyN * 0.035;
        }
        if (m == M_FIRE) col += vec3(0.14, 0.05, 0.0) * flick;
        if (m == M_LAVA) col += vec3(0.16, 0.06, 0.0) * flick;
    }

    if (uGrain != 0) col += vec3((n - 0.5) * 0.06);

    if (uPaletteMode == 1) {
        col = mix(col, texture(uPalette, vec2((float(m) + 0.5) / 256.0, 0.5)).rgb, 0.35);
    }

    float alpha = 1.0;
    if (m == M_FIRE) alpha = 0.72;
    if (m == M_SMOKE) alpha = 0.50;
    if (m == M_WATER) alpha = 0.88;
    if (m == M_OIL) alpha = 0.90;
    if (m == M_ICE) alpha = 0.94;
    fragColor = vec4(min(col, vec3(1.0)), alpha);
}
