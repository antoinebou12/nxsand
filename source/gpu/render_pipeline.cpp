#include "render_pipeline.hpp"
#include <array>
#include <algorithm>
#include <iostream>

namespace nx {

static const float kFsQuad[] = {
    -1.f, -1.f, 0.f, 0.f,
     3.f, -1.f, 2.f, 0.f,
    -1.f,  3.f, 0.f, 2.f,
};

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
    for (int i = 0; i < 2; ++i) {
        if (glowFbo[i]) glDeleteFramebuffers(1, &glowFbo[i]);
        if (glowTex[i]) glDeleteTextures(1, &glowTex[i]);
        glowFbo[i] = glowTex[i] = 0;
    }
    palShader = ShaderProgram{};
    uiShader = ShaderProgram{};
    glowExtractShader = ShaderProgram{};
    glowBlurShader = ShaderProgram{};
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
    buildPaletteTexture();

    std::string vfull = shaderDir + "/fullscreen.vert";
    std::string fpal  = shaderDir + "/palette_lookup.frag";
    if (!palShader.loadFromFiles(vfull, fpal)) return false;

    palShader.use();
    pal_uSim   = palShader.uniformLocation("uSim");
    pal_uPal   = palShader.uniformLocation("uPalette");
    pal_uGrid  = palShader.uniformLocation("uGridSize");
    pal_uFrame = palShader.uniformLocation("uFrame");
    pal_uMode  = palShader.uniformLocation("uPaletteMode");

    std::string vui = shaderDir + "/ui_quad.vert";
    std::string fui = shaderDir + "/ui_quad.frag";
    if (!uiShader.loadFromFiles(vui, fui)) return false;

    uiShader.use();
    ui_uScreen = uiShader.uniformLocation("uScreen");
    ui_uTex    = uiShader.uniformLocation("uTex");
    ui_uMode   = uiShader.uniformLocation("uMode");

    std::string vext = shaderDir + "/glow_extract.frag";
    std::string vblur = shaderDir + "/glow_blur.frag";
    if (!glowExtractShader.loadFromFiles(vfull, vext)) return false;
    if (!glowBlurShader.loadFromFiles(vfull, vblur)) return false;

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

void RenderPipeline::drawSimulation(GLuint simR8UI, int simW, int simH, const PlayRegion& pr,
                                     int screenH, uint32_t frame, double simMsForStressMode) {
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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(pr.x, screenH - pr.y - pr.h, pr.w, pr.h);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    palShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, palTex);
    glUniform1i(pal_uPal, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, simR8UI);
    glUniform1i(pal_uSim, 1);

    glUniform2i(pal_uGrid, simW, simH);
    glUniform1ui(pal_uFrame, frame);
    if (pal_uMode >= 0) {
        glUniform1i(pal_uMode, mode);
    }

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderPipeline::drawSolidRect(float x, float y, float w, float h, float r, float g, float b,
                                   float a, int screenW, int screenH) {
    // Bind a complete texture for uTex (mode 0 does not sample). Texture 0 is incomplete on GLES
    // and can crash or fault some drivers (e.g. Switch) even when the sampler is unused.
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

void RenderPipeline::drawGlow(GLuint simR8UI, int simW, int simH, const PlayRegion& pr,
                               int screenW, int screenH, uint32_t /*frame*/) {
    if (!glowEnabled_) return;
    if (glowW <= 0) {
        glowW = 256;
        glowH = 144;
        for (int i = 0; i < 2; ++i) {
            glGenTextures(1, &glowTex[i]);
            glBindTexture(GL_TEXTURE_2D, glowTex[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, glowW, glowH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glGenFramebuffers(1, &glowFbo[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, glowFbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glowTex[i], 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, glowFbo[0]);
    glViewport(0, 0, glowW, glowH);
    glDisable(GL_BLEND);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glowExtractShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, palTex);
    glUniform1i(glowExtractShader.uniformLocation("uPalette"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, simR8UI);
    glUniform1i(glowExtractShader.uniformLocation("uSim"), 1);
    glUniform2i(glowExtractShader.uniformLocation("uGridSize"), simW, simH);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    const GLint uTex = glowBlurShader.uniformLocation("uTex");
    const GLint uTexel = glowBlurShader.uniformLocation("uTexel");
    const GLint uDir = glowBlurShader.uniformLocation("uDir");
    const GLint uRadius = glowBlurShader.uniformLocation("uRadius");
    const float radii[] = {1.0f, 2.75f, 6.5f, 13.0f};

    for (int pass = 0; pass < 8; ++pass) {
        int src = (pass & 1) == 0 ? 0 : 1;
        int dst = 1 - src;
        glBindFramebuffer(GL_FRAMEBUFFER, glowFbo[dst]);
        glViewport(0, 0, glowW, glowH);
        glowBlurShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, glowTex[src]);
        glUniform1i(uTex, 0);
        glUniform2f(uTexel, 1.f / float(glowW), 1.f / float(glowH));
        glUniform2f(uDir, (pass & 1) == 0 ? 1.f : 0.f, (pass & 1) == 0 ? 0.f : 1.f);
        glUniform1f(uRadius, radii[pass / 2]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
    // Glow FBO uses GL bottom-left tex coords (fullscreen.vert); UI quads are top-left.
    // Flip V so the halo aligns with drawSimulation (which flips via glViewport).
    flushUiBatch();
    drawTexturedRect(float(pr.x), float(pr.y), float(pr.w), float(pr.h), 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 1.f,
                     0.82f, screenW, screenH, glowTex[0]);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

} // namespace nx
