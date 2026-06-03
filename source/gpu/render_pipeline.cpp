#include "render_pipeline.hpp"
#include "upscale_filters.hpp"
#include <array>
#include <algorithm>
#include <iostream>

namespace nx {

static const float kFsQuad[] = {
    -1.f, -1.f, 0.f, 0.f,
     3.f, -1.f, 2.f, 0.f,
    -1.f,  3.f, 0.f, 2.f,
};

namespace {

void allocLinearRgbaTexture(GLuint tex, int w, int h) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

void attachColorFbo(GLuint fbo, GLuint tex) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
}

int bloomTierDim(int simDim, int divisor) {
    return std::max(8, simDim / divisor);
}

} // namespace

RenderPipeline::~RenderPipeline() {
    shutdown();
}

void RenderPipeline::shutdown() {
    if (uiVbo) glDeleteBuffers(1, &uiVbo);
    if (uiVao) glDeleteVertexArrays(1, &uiVao);
    uiVbo = uiVao = 0;
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vbo = vao = 0;
    if (palTex) glDeleteTextures(1, &palTex);
    palTex = 0;
    if (lookTex) glDeleteTextures(1, &lookTex);
    lookTex = 0;
    if (lookFbo) glDeleteFramebuffers(1, &lookFbo);
    lookFbo = 0;
    lookW = lookH = 0;
    releaseBloomTargets();
    palShader = ShaderProgram{};
    upscaleShader = ShaderProgram{};
    uiShader = ShaderProgram{};
    bloomBrightShader = ShaderProgram{};
    bloomBlurShader = ShaderProgram{};
    bloomCompositeShader = ShaderProgram{};
    shaderDir_.clear();
}

void RenderPipeline::buildPaletteTexture() {
    if (!palTex) glGenTextures(1, &palTex);
    auto cols = build_palette();
    std::array<uint8_t, 256 * 4> px{};
    for (int i = 0; i < 256; ++i) {
        uint32_t c = cols[static_cast<size_t>(i)];
        px[static_cast<size_t>(i * 4 + 0)] = uint8_t(c & 0xff);
        px[static_cast<size_t>(i * 4 + 1)] = uint8_t((c >> 8) & 0xff);
        px[static_cast<size_t>(i * 4 + 2)] = uint8_t((c >> 16) & 0xff);
        px[static_cast<size_t>(i * 4 + 3)] = uint8_t((c >> 24) & 0xff);
    }
    glBindTexture(GL_TEXTURE_2D, palTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool RenderPipeline::init(const std::string& shaderDir) {
    shaderDir_ = shaderDir;
    buildPaletteTexture();

    std::string vui = shaderDir + "/ui_quad.vert";
    std::string fui = shaderDir + "/ui_quad.frag";
    if (!uiShader.loadFromFiles(vui, fui)) return false;

    uiShader.use();
    ui_uScreen = uiShader.uniformLocation("uScreen");
    ui_uTex    = uiShader.uniformLocation("uTex");
    ui_uMode   = uiShader.uniformLocation("uMode");

    return true;
}

bool RenderPipeline::initWorldShaders() {
    if (palShader.program != 0 && upscaleShader.program != 0) return true;
    if (shaderDir_.empty()) return false;

    buildPaletteTexture();

    std::string vfull = shaderDir_ + "/fullscreen.vert";
    std::string fpal  = shaderDir_ + "/palette_lookup.frag";
    if (!palShader.loadFromFiles(vfull, fpal)) return false;

    palShader.use();
    pal_uSim   = palShader.uniformLocation("uSim");
    pal_uPal   = palShader.uniformLocation("uPalette");
    pal_uGrid  = palShader.uniformLocation("uGridSize");
    pal_uFrame = palShader.uniformLocation("uFrame");
    pal_uMode  = palShader.uniformLocation("uPaletteMode");
    pal_uFlicker = palShader.uniformLocation("uFlicker");
    pal_uGrain = palShader.uniformLocation("uGrain");
    pal_uBlob = palShader.uniformLocation("uBlob");
    pal_uAo = palShader.uniformLocation("uAoStrength");

    std::string fbright = shaderDir_ + "/bloom_bright.frag";
    std::string fblur = shaderDir_ + "/bloom_blur.frag";
    std::string fcomp = shaderDir_ + "/bloom_composite.frag";
    if (!bloomBrightShader.loadFromFiles(vfull, fbright)) return false;
    if (!bloomBlurShader.loadFromFiles(vfull, fblur)) return false;
    if (!bloomCompositeShader.loadFromFiles(vfull, fcomp)) return false;

    bloomBrightShader.use();
    bright_uTex = bloomBrightShader.uniformLocation("uTex");

    bloomBlurShader.use();
    blur_uTex = bloomBlurShader.uniformLocation("uTex");
    blur_uTexSize = bloomBlurShader.uniformLocation("uTexSize");
    blur_uDir = bloomBlurShader.uniformLocation("uDir");

    bloomCompositeShader.use();
    comp_uTex = bloomCompositeShader.uniformLocation("uTex");
    comp_uBlurTex = bloomCompositeShader.uniformLocation("uBlurTex");
    comp_uBloomScalar = bloomCompositeShader.uniformLocation("uBloomScalar");
    comp_uExposure = bloomCompositeShader.uniformLocation("uExposure");
    comp_uGamma = bloomCompositeShader.uniformLocation("uGamma");
    comp_uSaturation = bloomCompositeShader.uniformLocation("uSaturation");

    std::string fup = shaderDir_ + "/upscale.frag";
    if (!upscaleShader.loadFromFiles(vfull, fup)) return false;
    upscaleShader.use();
    up_uSrc = upscaleShader.uniformLocation("uSrc");
    up_uSrcSize = upscaleShader.uniformLocation("uSrcSize");
    up_uDstSize = upscaleShader.uniformLocation("uDstSize");
    up_uFilter = upscaleShader.uniformLocation("uFilter");

    ensureQuadVbo();
    return true;
}

void RenderPipeline::ensureQuadVbo() {
    if (vao) return;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kFsQuad), kFsQuad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void RenderPipeline::ensureUiQuadVbo() {
    if (uiVao) return;
    glGenVertexArrays(1, &uiVao);
    glGenBuffers(1, &uiVbo);
    glBindVertexArray(uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(UiVertex) * kUiBatchMaxVerts, nullptr, GL_STREAM_DRAW);
    uiBatch_.reserve(512);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(UiVertex), (void*)(4 * sizeof(float)));
    glBindVertexArray(0);
}

void RenderPipeline::flushUiBatch() {
    if (uiBatch_.empty()) return;

    ensureUiQuadVbo();
    glBindVertexArray(uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, uiBatch_.size() * sizeof(UiVertex), uiBatch_.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, uiBatchScreenW_, uiBatchScreenH_);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    uiShader.use();
    glUniform2f(ui_uScreen, float(uiBatchScreenW_), float(uiBatchScreenH_));
    glUniform1i(ui_uTex, 0);
    glUniform1i(ui_uMode, uiBatchMode_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, uiBatchTex_);

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(uiBatch_.size()));

    glBindVertexArray(0);
    uiBatch_.clear();
    uiBatchMode_ = -1;
}

void RenderPipeline::beginUiFrame() {
    flushUiBatch();
    uiBatch_.clear();
    uiBatchMode_ = -1;
}

void RenderPipeline::endUiFrame() {
    flushUiBatch();
}

void RenderPipeline::drawUiQuad(float x, float y, float w, float h, float u0, float v0, float u1,
                                float v1, float r, float g, float b, float a, int screenW,
                                int screenH, GLuint texture, int mode) {
    const bool batchBreak =
        !uiBatch_.empty() &&
        (texture != uiBatchTex_ || mode != uiBatchMode_ || screenW != uiBatchScreenW_ ||
         screenH != uiBatchScreenH_);
    if (batchBreak || uiBatch_.size() + 6 > kUiBatchMaxVerts) {
        flushUiBatch();
    }

    uiBatchTex_ = texture;
    uiBatchMode_ = mode;
    uiBatchScreenW_ = screenW;
    uiBatchScreenH_ = screenH;

    uiBatch_.push_back({x, y, u0, v0, r, g, b, a});
    uiBatch_.push_back({x + w, y, u1, v0, r, g, b, a});
    uiBatch_.push_back({x, y + h, u0, v1, r, g, b, a});
    uiBatch_.push_back({x + w, y, u1, v0, r, g, b, a});
    uiBatch_.push_back({x + w, y + h, u1, v1, r, g, b, a});
    uiBatch_.push_back({x, y + h, u0, v1, r, g, b, a});
}

void RenderPipeline::setPaletteMode(int mode) {
    paletteMode_ = mode;
}

void RenderPipeline::setUpscaleFilter(UpscaleFilter filter) {
    upscaleFilter_ = filter;
}

void RenderPipeline::releaseLookTargets() {
    if (lookTex) glDeleteTextures(1, &lookTex);
    lookTex = 0;
    if (lookFbo) glDeleteFramebuffers(1, &lookFbo);
    lookFbo = 0;
    lookW = lookH = 0;
}

void RenderPipeline::ensureLookTargets(int simW, int simH) {
    if (simW <= 0 || simH <= 0) return;
    if (simW == lookW && simH == lookH && lookTex != 0) return;

    releaseLookTargets();
    lookW = simW;
    lookH = simH;

    glGenTextures(1, &lookTex);
    glBindTexture(GL_TEXTURE_2D, lookTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, lookW, lookH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffers(1, &lookFbo);
    attachColorFbo(lookFbo, lookTex);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderPipeline::drawPalettePass(GLuint simR8UI, int simW, int simH, uint32_t frame, int mode) {
    palShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, palTex);
    glUniform1i(pal_uPal, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, simR8UI);
    glUniform1i(pal_uSim, 1);

    glUniform2i(pal_uGrid, simW, simH);
    glUniform1ui(pal_uFrame, frame);
    if (pal_uMode >= 0) glUniform1i(pal_uMode, mode);
    if (pal_uFlicker >= 0) glUniform1i(pal_uFlicker, flickerEnabled_ ? 1 : 0);
    if (pal_uGrain >= 0) glUniform1i(pal_uGrain, grainEnabled_ ? 1 : 0);
    const bool blobOn = blobEnabled_ && mode != 1;
    if (pal_uBlob >= 0) glUniform1i(pal_uBlob, blobOn ? 1 : 0);
    if (pal_uAo >= 0) glUniform1f(pal_uAo, aoStrength_);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderPipeline::setBloomLevel(VisualBloom level) {
    bloom_ = level;
    switch (level) {
        case VisualBloom::Low:
            bloomBlurPasses_ = 4;
            bloomScalar_ = 1.0f;
            bloomExposure_ = 0.5f;
            bloomGamma_ = 2.2f;
            bloomSaturation_ = 2.0f;
            break;
        case VisualBloom::Off:
        default:
            bloomBlurPasses_ = 0;
            bloomScalar_ = 0.f;
            break;
    }
}

void RenderPipeline::releaseBloomTargets() {
    if (brightTex) glDeleteTextures(1, &brightTex);
    brightTex = 0;
    if (brightFbo) glDeleteFramebuffers(1, &brightFbo);
    brightFbo = 0;
    brightW = brightH = 0;

    for (int i = 0; i < 2; ++i) {
        if (blurFbo[i]) glDeleteFramebuffers(1, &blurFbo[i]);
        if (blurTex[i]) glDeleteTextures(1, &blurTex[i]);
        blurFbo[i] = blurTex[i] = 0;
    }
    blurW = blurH = 0;

    if (postTex) glDeleteTextures(1, &postTex);
    postTex = 0;
    if (postFbo) glDeleteFramebuffers(1, &postFbo);
    postFbo = 0;
    postW = postH = 0;

    bloomSimW = bloomSimH = 0;
}

void RenderPipeline::ensureBloomTargets(int simW, int simH) {
    if (simW <= 0 || simH <= 0) return;

    const int bw = bloomTierDim(simW, 8);
    const int bh = bloomTierDim(simH, 8);
    const int blw = bloomTierDim(simW, 16);
    const int blh = bloomTierDim(simH, 16);

    if (simW == bloomSimW && simH == bloomSimH && brightTex != 0) return;

    releaseBloomTargets();
    bloomSimW = simW;
    bloomSimH = simH;
    brightW = bw;
    brightH = bh;
    blurW = blw;
    blurH = blh;
    postW = simW;
    postH = simH;

    glGenTextures(1, &brightTex);
    allocLinearRgbaTexture(brightTex, brightW, brightH);
    glGenFramebuffers(1, &brightFbo);
    attachColorFbo(brightFbo, brightTex);

    for (int i = 0; i < 2; ++i) {
        glGenTextures(1, &blurTex[i]);
        allocLinearRgbaTexture(blurTex[i], blurW, blurH);
        glGenFramebuffers(1, &blurFbo[i]);
        attachColorFbo(blurFbo[i], blurTex[i]);
    }

    glGenTextures(1, &postTex);
    allocLinearRgbaTexture(postTex, postW, postH);
    glGenFramebuffers(1, &postFbo);
    attachColorFbo(postFbo, postTex);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderPipeline::runBloomPipeline() {
    if (!bloomEnabled() || bloomBlurPasses_ <= 0 || lookTex == 0) return;
    ensureBloomTargets(bloomSimW > 0 ? bloomSimW : lookW, bloomSimH > 0 ? bloomSimH : lookH);

    glDisable(GL_BLEND);
    glBindVertexArray(vao);

    glBindFramebuffer(GL_FRAMEBUFFER, brightFbo);
    glViewport(0, 0, brightW, brightH);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    bloomBrightShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, lookTex);
    glUniform1i(bright_uTex, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    GLuint blurSrc = brightTex;
    float srcW = float(brightW);
    float srcH = float(brightH);
    for (int pass = 0; pass < bloomBlurPasses_; ++pass) {
        const int dst = pass & 1;
        glBindFramebuffer(GL_FRAMEBUFFER, blurFbo[dst]);
        glViewport(0, 0, blurW, blurH);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        bloomBlurShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, blurSrc);
        glUniform1i(blur_uTex, 0);
        glUniform2f(blur_uTexSize, srcW, srcH);
        const bool vertical = (pass & 1) == 0;
        glUniform2f(blur_uDir, vertical ? 0.f : 1.f, vertical ? 1.f : 0.f);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        blurSrc = blurTex[dst];
        srcW = float(blurW);
        srcH = float(blurH);
    }

    const int blurOut = (bloomBlurPasses_ & 1) == 0 ? 0 : 1;

    glBindFramebuffer(GL_FRAMEBUFFER, postFbo);
    glViewport(0, 0, postW, postH);
    glClearColor(0.03f, 0.04f, 0.06f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    bloomCompositeShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, lookTex);
    glUniform1i(comp_uTex, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, blurTex[blurOut]);
    glUniform1i(comp_uBlurTex, 1);
    glUniform1f(comp_uBloomScalar, bloomScalar_);
    glUniform1f(comp_uExposure, bloomExposure_);
    glUniform1f(comp_uGamma, bloomGamma_);
    glUniform1f(comp_uSaturation, bloomSaturation_);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderPipeline::blitPostToPlayRegion(const PlayRegion& pr, int screenH, int simW, int simH,
                                          bool filtered) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(pr.x, screenH - pr.y - pr.h, pr.w, pr.h);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const UpscaleFilter filter = filtered ? upscaleFilter_ : UpscaleFilter::Nearest;
    upscaleShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, postTex);
    glUniform1i(up_uSrc, 0);
    glUniform2f(up_uSrcSize, float(simW), float(simH));
    glUniform2f(up_uDstSize, float(pr.w), float(pr.h));
    glUniform1i(up_uFilter, upscaleFilterShaderId(filter));

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderPipeline::drawSimulation(GLuint simR8UI, int simW, int simH, const PlayRegion& pr,
                                     int screenH, uint32_t frame, double simMsForStressMode) {
    if (!initWorldShaders()) return;

    int mode = paletteMode_;
#if !defined(__SWITCH__)
    if (simMsForStressMode > 14.0 && mode == 0) {
        mode = 1;
    }
#else
    if (simMsForStressMode > 22.0 && mode == 0) {
        mode = 1;
    }
#endif

    const bool filtered = upscaleFilter_ != UpscaleFilter::Nearest;
    const bool bloom = bloomEnabled() && bloomBlurPasses_ > 0;
    const bool needsLook = filtered || bloom;

    if (!needsLook) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(pr.x, screenH - pr.y - pr.h, pr.w, pr.h);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawPalettePass(simR8UI, simW, simH, frame, mode);
        return;
    }

    ensureLookTargets(simW, simH);
    bloomSimW = simW;
    bloomSimH = simH;

    glBindFramebuffer(GL_FRAMEBUFFER, lookFbo);
    glViewport(0, 0, simW, simH);
    glDisable(GL_BLEND);
    glClearColor(0.03f, 0.04f, 0.06f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    drawPalettePass(simR8UI, simW, simH, frame, mode);

    if (bloom) {
        runBloomPipeline();
        blitPostToPlayRegion(pr, screenH, simW, simH, filtered);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(pr.x, screenH - pr.y - pr.h, pr.w, pr.h);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    upscaleShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, lookTex);
    glUniform1i(up_uSrc, 0);
    glUniform2f(up_uSrcSize, float(simW), float(simH));
    glUniform2f(up_uDstSize, float(pr.w), float(pr.h));
    glUniform1i(up_uFilter, upscaleFilterShaderId(upscaleFilter_));

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderPipeline::drawSolidRect(float x, float y, float w, float h, float r, float g, float b,
                                   float a, int screenW, int screenH) {
    const GLuint bindTex = palTex ? palTex : 0;
    drawUiQuad(x, y, w, h, 0.f, 0.f, 1.f, 1.f, r, g, b, a, screenW, screenH, bindTex, 0);
}

void RenderPipeline::drawTexturedRect(float x, float y, float w, float h, float u0, float v0,
                                      float u1, float v1, float r, float g, float b, float a,
                                      int screenW, int screenH, GLuint texture) {
    drawUiQuad(x, y, w, h, u0, v0, u1, v1, r, g, b, a, screenW, screenH, texture, 1);
}

void RenderPipeline::drawAlphaMaskRect(float x, float y, float w, float h, float u0, float v0,
                                       float u1, float v1, float r, float g, float b, float a,
                                       int screenW, int screenH, GLuint texture) {
    drawUiQuad(x, y, w, h, u0, v0, u1, v1, r, g, b, a, screenW, screenH, texture, 2);
}

} // namespace nx
