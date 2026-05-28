#version 300 es
// GLES 3.0 fragment Margolus pass over GL_R8UI ping-pong textures.

precision highp float;
precision highp int;
precision highp usampler2D;

in vec2 v_uv;
layout(location = 0) out uint outMat;

uniform highp usampler2D uSim;
uniform ivec2 uGridSize;
uniform ivec2 uPhase;
uniform uint uFrame;

#include "sim_ids.glsl"

uint cell(ivec2 c) {
    if (c.x < 0 || c.y < 0 || c.x >= uGridSize.x || c.y >= uGridSize.y) return M_WALL;
    return texelFetch(uSim, c, 0).r;
}

#include "sim_common.glsl"

void main() {
    ivec2 c = ivec2(gl_FragCoord.xy);
    if (c.x < 0 || c.y < 0 || c.x >= uGridSize.x || c.y >= uGridSize.y) {
        outMat = M_EMPTY;
        return;
    }
    outMat = simulateBlockCell(c);
}
