#version 300 es
// UI quad transform: pixel-space (0,0)..(screenW, screenH) -> NDC.
precision highp float;

layout(location = 0) in vec2 a_pos;     // pixel-space
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;

out vec2 v_uv;
out vec4 v_color;

uniform vec2 uScreen; // (screenW, screenH)

void main() {
    v_uv = a_uv;
    v_color = a_color;
    vec2 ndc = (a_pos / uScreen) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
