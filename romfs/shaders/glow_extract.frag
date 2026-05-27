#version 300 es
precision highp float;
precision highp int;
precision highp usampler2D;

in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

uniform usampler2D uSim;
uniform sampler2D  uPalette;
uniform ivec2      uGridSize;

const uint M_FIRE  = 3u;
const uint M_SMOKE = 4u;
const uint M_LAVA  = 8u;
const uint M_OIL   = 10u;

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    ivec2 c = ivec2(v_uv * vec2(uGridSize));
    c = clamp(c, ivec2(0), uGridSize - ivec2(1));
    uint m = texelFetch(uSim, c, 0).r;
    float jitter = hash12(vec2(c) + vec2(13.7, 71.2));

    vec4 col = texture(uPalette, vec2((float(m) + 0.5) / 256.0, 0.5));
    vec3 emit = vec3(0.0);
    float strength = 0.0;
    if (m == M_FIRE) {
        strength = 1.15 + jitter * 0.45;
        emit = vec3(1.0, 0.50, 0.13) * strength;
    } else if (m == M_LAVA) {
        strength = 0.90 + jitter * 0.22;
        emit = mix(vec3(1.0, 0.20, 0.04), vec3(1.0, 0.72, 0.16), jitter) * strength;
    } else if (m == M_SMOKE) {
        strength = 0.08;
        emit = vec3(0.28, 0.20, 0.15) * strength;
    } else if (m == M_OIL) {
        strength = 0.025;
        emit = col.rgb * strength;
    }
    fragColor = vec4(emit, clamp(strength, 0.0, 1.0));
}
