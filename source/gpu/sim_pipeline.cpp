#include "sim_pipeline.hpp"
#include "shader_program.hpp"
#include "../sim/physics_gpu.hpp"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace nx {

namespace {

static const float kFsQuad[] = {
    -1.f, -1.f, 0.f, 0.f,
     3.f, -1.f, 2.f, 0.f,
    -1.f,  3.f, 0.f, 2.f,
};

} // namespace

SimPipeline::~SimPipeline() {
    shutdown();
}

void SimPipeline::shutdown() {
    if (physicsUbo) glDeleteBuffers(1, &physicsUbo);
    physicsUbo = 0;
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vbo = vao = 0;
    for (int i = 0; i < 2; ++i) {
        if (fbo[i]) glDeleteFramebuffers(1, &fbo[i]);
        if (tex[i]) glDeleteTextures(1, &tex[i]);
        fbo[i] = tex[i] = 0;
    }
    simShader = ShaderProgram{};
    paintShader = ShaderProgram{};
}

bool SimPipeline::init(int w, int h, const std::string& shaderDir) {
    gw = w;
    gh = h;

    const std::string vertPath = shaderDir + "/fullscreen.vert";
    const std::string simPath = shaderDir + "/sim.frag";
    const std::string paintPath = shaderDir + "/paint.frag";

    if (!simShader.loadFromFiles(vertPath, simPath)) {
        const char* diag = lastShaderDiagnostics();
        setShaderDiagnostics(diag && diag[0] ? std::string("sim.frag: ") + diag : "sim.frag load failed");
        return false;
    }
    if (!paintShader.loadFromFiles(vertPath, paintPath)) {
        const char* diag = lastShaderDiagnostics();
        setShaderDiagnostics(diag && diag[0] ? std::string("paint.frag: ") + diag : "paint.frag load failed");
        return false;
    }

    simShader.use();
    uSimLoc = simShader.uniformLocation("uSim");
    uGridLoc = simShader.uniformLocation("uGridSize");
    uPhaseLoc = simShader.uniformLocation("uPhase");
    uFrameLoc = simShader.uniformLocation("uFrame");

    paintShader.use();
    paint_uSimLoc = paintShader.uniformLocation("uSim");
    paint_uGridLoc = paintShader.uniformLocation("uGridSize");
    paint_uCenterLoc = paintShader.uniformLocation("uCenter");
    paint_uRadiusLoc = paintShader.uniformLocation("uRadius");
    paint_uMaterialLoc = paintShader.uniformLocation("uMaterial");

    physicsBlockIndex = glGetUniformBlockIndex(simShader.program, "PhysicsBlock");
    if (physicsBlockIndex == GL_INVALID_INDEX) {
        std::cerr << "sim shader missing PhysicsBlock UBO\n";
        setShaderDiagnostics("sim.frag missing PhysicsBlock UBO");
        return false;
    }
    glUniformBlockBinding(simShader.program, physicsBlockIndex, kPhysicsUboBinding);

    glGenBuffers(1, &physicsUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, physicsUbo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(PhysicsParamsGPU), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, kPhysicsUboBinding, physicsUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

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

    for (int i = 0; i < 2; ++i) {
        glGenTextures(1, &tex[i]);
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, gw, gh, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);

        glGenFramebuffers(1, &fbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[i], 0);
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (st != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "FBO incomplete " << i << " status=" << st << "\n";
            setShaderDiagnostics("GPU FBO incomplete (R8UI sim texture). status=" +
                                 std::to_string(static_cast<unsigned>(st)));
            return false;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    activeTiles.reset(gw, gh);
    clearAll(MAT_EMPTY);
    return true;
}

void SimPipeline::clearAll(Material m) {
    GLint prevViewport[4] = {0, 0, 0, 0};
    GLint prevFbo = 0;
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);

    GLuint v = static_cast<GLuint>(m);
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glViewport(0, 0, gw, gh);
        GLuint clearVals[4] = {v, 0, 0, 0};
        glClearBufferuiv(GL_COLOR, 0, clearVals);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    cur = 0;
    activeTiles.sleepAll();
}

void SimPipeline::syncSimForSampling() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SimPipeline::paintDisk(int cx, int cy, int radius, Material m, int* outDirtyW,
                            int* outDirtyH) {
    const int gy = gh - 1 - cy;
    if (cx < 0 || cx >= gw || gy < 0 || gy >= gh) return;

    GLint prevViewport[4] = {0, 0, 0, 0};
    GLint prevDrawFbo = 0;
    GLint prevReadFbo = 0;
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);

    const int x0 = std::max(0, cx - radius);
    const int x1 = std::min(gw - 1, cx + radius);
    const int y0 = std::max(0, gy - radius);
    const int y1 = std::min(gh - 1, gy + radius);
    const int bw = x1 - x0 + 1;
    const int bh = y1 - y0 + 1;
    if (outDirtyW) *outDirtyW = bw;
    if (outDirtyH) *outDirtyH = bh;

    activeTiles.markDisk(cx, cy, radius);

    const int read = cur;
    const int write = 1 - cur;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[read]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[write]);
    glBlitFramebuffer(0, 0, gw, gh, 0, 0, gw, gh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[write]);
    glViewport(x0, y0, bw, bh);
    glDisable(GL_BLEND);

    paintShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex[read]);
    glUniform1i(paint_uSimLoc, 0);
    glUniform2i(paint_uGridLoc, gw, gh);
    glUniform2i(paint_uCenterLoc, cx, gy);
    glUniform1i(paint_uRadiusLoc, radius);
    glUniform1ui(paint_uMaterialLoc, static_cast<GLuint>(m));

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(prevDrawFbo));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prevReadFbo));
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    cur = write;
}

Material SimPipeline::sampleMaterial(int cx, int cy) {
    const int gy = gh - 1 - cy;
    if (cx < 0 || cx >= gw || gy < 0 || gy >= gh) return MAT_EMPTY;

    uint8_t pix = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[cur]);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(cx, gy, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &pix);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return sanitizeBrushMaterial(pix);
}

void SimPipeline::runPass(int phaseX, int phaseY, uint32_t frame, int x0, int y0, int w, int h) {
    GLint prevViewport[4] = {0, 0, 0, 0};
    GLint prevFbo = 0;
    GLboolean prevScissor = GL_FALSE;
    GLint prevScissorBox[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    prevScissor = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, prevScissorBox);

    const int read = cur;
    const int write = 1 - cur;
    const bool partial = w > 0 && h > 0 && (x0 > 0 || y0 > 0 || w < gw || h < gh);

    if (partial) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[read]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[write]);
        glBlitFramebuffer(0, 0, gw, gh, 0, 0, gw, gh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo[write]);
    glViewport(0, 0, gw, gh);
    if (partial) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(x0, y0, w, h);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
    glDisable(GL_BLEND);

    simShader.use();
    glBindBufferBase(GL_UNIFORM_BUFFER, kPhysicsUboBinding, physicsUbo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex[read]);
    glUniform1i(uSimLoc, 0);
    glUniform2i(uGridLoc, gw, gh);
    glUniform2i(uPhaseLoc, phaseX, phaseY);
    glUniform1ui(uFrameLoc, frame);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    if (prevScissor)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
    glScissor(prevScissorBox[0], prevScissorBox[1], prevScissorBox[2], prevScissorBox[3]);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo));
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    cur = write;
}

void SimPipeline::uploadPhysics(const PhysicsParams& physics) {
    PhysicsParamsGPU gpu = to_gpu(physics, gw);
    glBindBuffer(GL_UNIFORM_BUFFER, physicsUbo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(gpu), &gpu);
}

void SimPipeline::step(uint32_t frameTick, const PhysicsParams& physics) {
    step(frameTick, physics, ActiveTileMode::Off);
}

void SimPipeline::step(uint32_t frameTick, const PhysicsParams& physics, ActiveTileMode activeMode) {
    lastPasses_ = 0;
    lastActiveTileMode_ = activeMode;
    lastActiveTileFallback_ = false;
    lastActiveTileCount_ = activeTiles.activeCount();
    uploadPhysics(physics);
    static const int phases[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    int sx = 0;
    int syTop = 0;
    int sx1 = gw - 1;
    int syTop1 = gh - 1;
    bool usePartial = false;
    if (activeMode != ActiveTileMode::Off) {
        const int expand = activeMode == ActiveTileMode::Aggressive ? 1 : 2;
        usePartial = activeTiles.activeBounds(sx, syTop, sx1, syTop1, expand);
        const int activeArea = usePartial ? (sx1 - sx + 1) * (syTop1 - syTop + 1) : 0;
        const int gridArea = std::max(1, gw * gh);
        const bool periodicFull = (frameTick % (activeMode == ActiveTileMode::Aggressive ? 180u : 90u)) == 0u;
        const bool tooLarge = activeArea > (gridArea * 45) / 100;
        if (!usePartial || periodicFull || tooLarge) {
            usePartial = false;
            lastActiveTileFallback_ = true;
        }
    }
    int glX = 0;
    int glY = 0;
    int glW = gw;
    int glH = gh;
    if (usePartial) {
        glX = std::max(0, sx - 2);
        const int topY = std::max(0, syTop - 2);
        const int bottomY = std::min(gh - 1, syTop1 + 2);
        const int rightX = std::min(gw - 1, sx1 + 2);
        glW = rightX - glX + 1;
        glH = bottomY - topY + 1;
        glY = gh - bottomY - 1;
    }
    for (int p = 0; p < 4; ++p) {
        runPass(phases[p][0], phases[p][1], frameTick, glX, glY, glW, glH);
        ++lastPasses_;
    }
}

bool SimPipeline::readGridTo(std::vector<uint8_t>& out) {
    out.assign(static_cast<size_t>(gw * gh), 0);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[cur]);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, gw, gh, GL_RED_INTEGER, GL_UNSIGNED_BYTE, out.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void SimPipeline::uploadGridTopDown(const std::vector<uint8_t>& data, int w, int h) {
    std::vector<uint8_t> top(static_cast<size_t>(gw * gh), static_cast<uint8_t>(MAT_EMPTY));
    for (int y = 0; y < gh; ++y) {
        int sy = (y * h) / gh;
        if (sy >= h) sy = h - 1;
        for (int x = 0; x < gw; ++x) {
            int sx = (x * w) / gw;
            if (sx >= w) sx = w - 1;
            top[static_cast<size_t>(y * gw + x)] = data[static_cast<size_t>(sy * w + sx)];
        }
    }
    std::vector<uint8_t> gl(static_cast<size_t>(gw * gh), 0);
    for (int y = 0; y < gh; ++y) {
        for (int x = 0; x < gw; ++x) {
            gl[static_cast<size_t>((gh - 1 - y) * gw + x)] =
                top[static_cast<size_t>(y * gw + x)];
        }
    }
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gw, gh, GL_RED_INTEGER, GL_UNSIGNED_BYTE, gl.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    cur = 0;
    activeTiles.wakeAll();
}

} // namespace nx
