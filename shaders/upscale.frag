#version 300 es
precision highp float;

in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

uniform sampler2D uSrc;
uniform vec2 uSrcSize;
uniform vec2 uDstSize;
uniform int uFilter;

const int FILTER_TENT = 1;
const int FILTER_MITCHELL = 2;
const int FILTER_CATMULLROM = 3;
const int FILTER_LANCZOS3 = 4;

float tent1d(float t) {
    t = abs(t);
    return t < 1.0 ? 1.0 - t : 0.0;
}

float mitchell1d(float t, float B, float C) {
    t = abs(t);
    float tt = t * t;
    if (t < 1.0) {
        return (((12.0 - 9.0 * B - 6.0 * C) * (t * tt)) + ((-18.0 + 12.0 * B + 6.0 * C) * tt) +
                (6.0 - 2.0 * B)) / 6.0;
    }
    if (t < 2.0) {
        return (((-B - 6.0 * C) * (t * tt)) + ((6.0 * B + 30.0 * C) * tt) +
                ((-12.0 * B - 48.0 * C) * t) + (8.0 * B + 24.0 * C)) / 6.0;
    }
    return 0.0;
}

float sinc1d(float x) {
    if (abs(x) < 1e-4) return 1.0;
    const float pi = 3.14159265;
    float px = pi * x;
    return sin(px) / px;
}

float lanczos3_1d(float t) {
    t = abs(t);
    if (t >= 3.0) return 0.0;
    return sinc1d(t) * sinc1d(t / 3.0);
}

float weight1d(float t, int filt) {
    if (filt == FILTER_TENT) return tent1d(t);
    if (filt == FILTER_MITCHELL) return mitchell1d(t, 1.0 / 3.0, 1.0 / 3.0);
    if (filt == FILTER_CATMULLROM) return mitchell1d(t, 0.0, 0.5);
    if (filt == FILTER_LANCZOS3) return lanczos3_1d(t);
    return tent1d(t);
}

vec2 srcCenterFromDstUv(vec2 uv) {
    vec2 dstPx = uv * uDstSize - vec2(0.5);
    vec2 scale = uDstSize / uSrcSize;
    return (dstPx + vec2(0.5)) / scale - vec2(0.5);
}

vec4 sampleSrcPixel(vec2 pix) {
    ivec2 c = ivec2(clamp(pix, vec2(0.0), uSrcSize - vec2(1.0)));
    return texelFetch(uSrc, c, 0);
}

vec4 sampleSeparable(vec2 center, int filt) {
    int radius = (filt == FILTER_LANCZOS3) ? 3 : 2;
    vec2 ic = floor(center);
    vec4 sum = vec4(0.0);
    float wsum = 0.0;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            vec2 p = ic + vec2(float(dx), float(dy));
            float w = weight1d(center.x - p.x, filt) * weight1d(center.y - p.y, filt);
            if (w == 0.0) continue;
            sum += sampleSrcPixel(p) * w;
            wsum += w;
        }
    }

    if (wsum > 1e-6) return sum / wsum;
    return sampleSrcPixel(center);
}

void main() {
    vec2 center = srcCenterFromDstUv(v_uv);
    fragColor = sampleSeparable(center, uFilter);
    fragColor.a = 1.0;
}
