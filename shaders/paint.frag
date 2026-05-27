#version 300 es
precision highp float;
precision highp int;
precision highp usampler2D;

in vec2 v_uv;
layout(location = 0) out uint outMat;

uniform highp usampler2D uSim;
uniform ivec2 uGridSize;
uniform ivec2 uCenter;
uniform int uRadius;
uniform uint uMaterial;

void main() {
    ivec2 c = ivec2(gl_FragCoord.xy);
    c = clamp(c, ivec2(0), uGridSize - ivec2(1));

    uint mat = texelFetch(uSim, c, 0).r;
    ivec2 d = c - uCenter;
    int rr = max(0, uRadius);
    if (d.x * d.x + d.y * d.y <= rr * rr) {
        mat = uMaterial;
    }
    outMat = mat;
}
