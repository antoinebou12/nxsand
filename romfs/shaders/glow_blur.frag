#version 300 es
precision highp float;

in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

uniform sampler2D uTex;
uniform vec2      uTexel;
uniform vec2      uDir; // (1,0) horizontal or (0,1) vertical
uniform float     uRadius;

void main() {
    vec2 stride = uTexel * uDir * max(1.0, uRadius);
    vec4 sum = texture(uTex, v_uv) * 0.227027;
    sum += texture(uTex, v_uv + stride) * 0.1945946;
    sum += texture(uTex, v_uv - stride) * 0.1945946;
    sum += texture(uTex, v_uv + stride * 2.0) * 0.1216216;
    sum += texture(uTex, v_uv - stride * 2.0) * 0.1216216;
    sum += texture(uTex, v_uv + stride * 3.0) * 0.054054;
    sum += texture(uTex, v_uv - stride * 3.0) * 0.054054;
    fragColor = sum;
}
