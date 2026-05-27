#version 300 es
// Fullscreen triangle vertex shader. Source `quad_uv` is provided as an
// attribute or computed from gl_VertexID; we use the attribute path so the
// same vert works for sim passes (NDC -1..1) and UI quads (transformed).
precision highp float;

layout(location = 0) in vec2 a_pos;   // -1..1 clip-space corners
layout(location = 1) in vec2 a_uv;    // 0..1 texture coords

out vec2 v_uv;

void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
