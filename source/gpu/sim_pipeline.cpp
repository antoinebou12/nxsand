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

// Compute sim writes via imageStore; brush and palette sample the same textures.
// Without these barriers, texelFetch can see pre-step data and sand appears frozen.
void barrierComputePass() {
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void barrierComputeToTexture() {
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
}

void barrierTextureToCompute() {
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT |
                    GL_TEXTURE_UPDATE_BARRIER_BIT);
}

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
    computeShader = ShaderProgram{};
    paintShader = ShaderProgram{};
    backend_ = SimBackend::Fragment;
}

bool SimPipeline::init(int w, int h, const std::string& shaderDir, SimBackend backend) {
    gw = w;
    gh = h;
    backend_ = backend;
    gridInternalFormat_ = backend_ == SimBackend::Compute ? GL_R32UI : GL_R8UI;
    gridPixelFormat_ = GL_RED_INTEGER;
    gridImageFormat_ = backend_ == SimBackend::Compute ? GL_R32UI : GL_R8UI;
    gridPixelType_ = backend_ == SimBackend::Compute ? GL_UNSIGNED_INT : GL_UNSIGNED_BYTE;

    const std::string vertPath = shaderDir + "/fullscreen.vert";
    const std::string simPath = shaderDir + "/sim.frag";
    const std::string compPath = shaderDir + "/sim.comp";
    const std::string paintPath = shaderDir + "/paint.frag";

    if (backend_ == SimBackend::Compute) {
        setShaderCompileStage("Compiling sim.comp…");
        if (!computeShader.loadComputeFromFile(compPath)) {
            const char* diag = lastShaderDiagnostics();
            setShaderDiagnostics(diag && diag[0] ? std::string("sim.comp: ") + diag
                                                   : "sim.comp load failed");
            return false;
        }
        computeShader.use();
        comp_uGridLoc = computeShader.uniformLocation("uGridSize");
        comp_uPhaseLoc = computeShader.uniformLocation("uPhase");
        comp_uFrameLoc = computeShader.uniformLocation("uFrame");
        comp_uDispatchOriginLoc = computeShader.uniformLocation("uDispatchOrigin");
        comp_uDispatchLimitLoc = computeShader.uniformLocation("uDispatchLimit");
        physicsBlockIndex = glGetUniformBlockIndex(computeShader.program, "PhysicsBlock");
        if (physicsBlockIndex == GL_INVALID_INDEX) {
            std::cerr << "compute shader missing PhysicsBlock UBO\n";
            setShaderDiagnostics("sim.comp missing PhysicsBlock UBO");
            return false;
        }
        glUniformBlockBinding(computeShader.program, physicsBlockIndex, kPhysicsUboBinding);
    } else {
        setShaderCompileStage("Compiling sim.frag…");
        if (!simShader.loadFromFiles(vertPath, simPath)) {
            const char* diag = lastShaderDiagnostics();
            setShaderDiagnostics(diag && diag[0] ? std::string("sim.frag: ") + diag
                                                 : "sim.frag load failed");
            return false;
        }
        simShader.use();
        uSimLoc = simShader.uniformLocation("uSim");
        uGridLoc = simShader.uniformLocation("uGridSize");
        uPhaseLoc = simShader.uniformLocation("uPhase");
        uFrameLoc = simShader.uniformLocation("uFrame");
        physicsBlockIndex = glGetUniformBlockIndex(simShader.program, "PhysicsBlock");
        if (physicsBlockIndex == GL_INVALID_INDEX) {
            std::cerr << "sim shader missing PhysicsBlock UBO\n";
            setShaderDiagnostics("sim.frag missing PhysicsBlock UBO");
            return false;
        }
        glUniformBlockBinding(simShader.program, physicsBlockIndex, kPhysicsUboBinding);
    }

    setShaderCompileStage("Compiling paint.frag…");
    if (!paintShader.loadFromFiles(vertPath, paintPath)) {
        const char* diag = lastShaderDiagnostics();
        setShaderDiagnostics(diag && diag[0] ? std::string("paint.frag: ") + diag : "paint.frag load failed");
        return false;
    }

    paintShader.use();
    paint_uSimLoc = paintShader.uniformLocation("uSim");
    paint_uGridLoc = paintShader.uniformLocation("uGridSize");
    paint_uCenterLoc = paintShader.uniformLocation("uCenter");
    paint_uRadiusLoc = paintShader.uniformLocation("uRadius");
    paint_uMaterialLoc = paintShader.uniformLocation("uMaterial");

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
        glTexStorage2D(GL_TEXTURE_2D, 1, gridInternalFormat_, gw, gh);
        const GLenum texErr = glGetError();
        if (texErr != GL_NO_ERROR) {
            std::cerr << "sim texture allocation failed " << i << " err=" << texErr << "\n";
            setShaderDiagnostics("GPU sim texture allocation failed. err=" +
                                 std::to_string(static_cast<unsigned>(texErr)));
            return false;
        }

        glGenFramebuffers(1, &fbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[i], 0);
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (st != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "FBO incomplete " << i << " status=" << st << "\n";
            setShaderDiagnostics("GPU FBO incomplete (sim texture). status=" +
                                 std::to_string(static_cast<unsigned>(st)));
            return false;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    activeTiles.reset(gw, gh);
    clearAll(MAT_EMPTY);

    if (backend_ == SimBackend::Compute) {
        while (glGetError() != GL_NO_ERROR) {
        }
        detachSimFboAttachments();
        glBindImageTexture(0, tex[0], 0, GL_FALSE, 0, GL_READ_ONLY, gridImageFormat_);
        glBindImageTexture(1, tex[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, gridImageFormat_);
        const GLenum imgErr = glGetError();
        unbindComputeImages();
        attachSimFboAttachments();
        if (imgErr != GL_NO_ERROR) {
            std::cerr << "compute sim: glBindImageTexture failed err=" << imgErr << "\n";
            setShaderDiagnostics("Compute grid image bind failed (GL_R32UI image)");
            return false;
        }
    }
    return true;
}

void SimPipeline::detachSimFboAttachments() {
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SimPipeline::attachSimFboAttachments() {
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[i], 0);
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (st != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "FBO reattach incomplete " << i << " status=" << st << "\n";
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SimPipeline::unbindComputeImages() {
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, gridImageFormat_);
    glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, gridImageFormat_);
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
    if (backend_ == SimBackend::Compute) {
        barrierTextureToCompute();
    }
}

void SimPipeline::syncSimForSampling() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (backend_ == SimBackend::Compute) {
        barrierComputeToTexture();
    }
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

    if (backend_ == SimBackend::Compute) {
        unbindComputeImages();
        barrierComputeToTexture();
    }

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
    if (backend_ == SimBackend::Compute) {
        barrierTextureToCompute();
    }
}

Material SimPipeline::sampleMaterial(int cx, int cy) {
    const int gy = gh - 1 - cy;
    if (cx < 0 || cx >= gw || gy < 0 || gy >= gh) return MAT_EMPTY;

    if (backend_ == SimBackend::Compute) {
        unbindComputeImages();
        barrierComputeToTexture();
    }

    GLuint pix = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[cur]);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(cx, gy, 1, 1, GL_RED_INTEGER, gridPixelType_, &pix);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return sanitizeBrushMaterial(static_cast<uint8_t>(pix & 0xffu));
}

void SimPipeline::runPass(int phaseX, int phaseY, uint32_t frame, int x0, int y0, int w, int h) {
    std::vector<SimPassRect> rects;
    if (w > 0 && h > 0) {
        rects.push_back({x0, y0, w, h});
    }
    runPassRegions(phaseX, phaseY, frame, rects);
}

void SimPipeline::runPassRegions(int phaseX, int phaseY, uint32_t frame,
                                 const std::vector<SimPassRect>& rects) {
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
    bool partial = !rects.empty();
    if (rects.size() == 1) {
        const SimPassRect& r = rects.front();
        partial = r.x > 0 || r.y > 0 || r.w < gw || r.h < gh;
    }

    if (partial) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[read]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[write]);
        glBlitFramebuffer(0, 0, gw, gh, 0, 0, gw, gh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo[write]);
    glViewport(0, 0, gw, gh);
    glDisable(GL_SCISSOR_TEST);
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
    if (partial) {
        glEnable(GL_SCISSOR_TEST);
        for (const SimPassRect& r : rects) {
            if (r.w <= 0 || r.h <= 0) continue;
            glScissor(r.x, r.y, r.w, r.h);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    } else {
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
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

void SimPipeline::runComputePass(int phaseX, int phaseY, uint32_t frame,
                                 const std::vector<SimPassRect>& rects) {
    const int read = cur;
    const int write = 1 - cur;
    bool partial = !rects.empty();
    if (rects.size() == 1) {
        const SimPassRect& r = rects.front();
        partial = r.x > 0 || r.y > 0 || r.w < gw || r.h < gh;
    }

    if (partial) {
        barrierComputeToTexture();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[read]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[write]);
        glBlitFramebuffer(0, 0, gw, gh, 0, 0, gw, gh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        barrierTextureToCompute();
    }

    detachSimFboAttachments();

    computeShader.use();
    glBindBufferBase(GL_UNIFORM_BUFFER, kPhysicsUboBinding, physicsUbo);
    glBindImageTexture(0, tex[read], 0, GL_FALSE, 0, GL_READ_ONLY, gridImageFormat_);
    glBindImageTexture(1, tex[write], 0, GL_FALSE, 0, GL_WRITE_ONLY, gridImageFormat_);
    glUniform2i(comp_uGridLoc, gw, gh);
    glUniform2i(comp_uPhaseLoc, phaseX, phaseY);
    glUniform1ui(comp_uFrameLoc, frame);
    if (!partial) {
        glUniform2i(comp_uDispatchOriginLoc, 0, 0);
        glUniform2i(comp_uDispatchLimitLoc, gw, gh);
        const GLuint groupsX = static_cast<GLuint>((gw + 15) / 16);
        const GLuint groupsY = static_cast<GLuint>((gh + 15) / 16);
        glDispatchCompute(groupsX, groupsY, 1);
    } else {
        for (const SimPassRect& r : rects) {
            if (r.w <= 0 || r.h <= 0) continue;
            glUniform2i(comp_uDispatchOriginLoc, r.x, r.y);
            glUniform2i(comp_uDispatchLimitLoc, r.x + r.w, r.y + r.h);
            const GLuint groupsX = static_cast<GLuint>((r.w + 15) / 16);
            const GLuint groupsY = static_cast<GLuint>((r.h + 15) / 16);
            glDispatchCompute(groupsX, groupsY, 1);
        }
    }
    barrierComputePass();
    unbindComputeImages();
    attachSimFboAttachments();
    cur = write;
}

void SimPipeline::stepCompute(uint32_t frameTick, const PhysicsParams& physics,
                              ActiveTileMode activeMode) {
    lastPasses_ = 0;
    lastActiveTileMode_ = activeMode;
    lastActiveTileFallback_ = false;
    lastActiveTileCount_ = activeTiles.activeCount();
    barrierTextureToCompute();
    uploadPhysics(physics);
    std::vector<SimPassRect> rects;
    const bool usePartial = collectActivePassRects(activeMode, rects);
    static const int phases[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    for (int p = 0; p < 4; ++p) {
        runComputePass(phases[p][0], phases[p][1], frameTick, usePartial ? rects : std::vector<SimPassRect>{});
        ++lastPasses_;
    }
    unbindComputeImages();
    barrierComputeToTexture();
}

void SimPipeline::stepFragment(uint32_t frameTick, const PhysicsParams& physics,
                               ActiveTileMode activeMode) {
    lastPasses_ = 0;
    lastActiveTileMode_ = activeMode;
    lastActiveTileFallback_ = false;
    lastActiveTileCount_ = activeTiles.activeCount();
    uploadPhysics(physics);
    static const int phases[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    std::vector<SimPassRect> rects;
    const bool usePartial = collectActivePassRects(activeMode, rects);
    for (int p = 0; p < 4; ++p) {
        runPassRegions(phases[p][0], phases[p][1], frameTick,
                       usePartial ? rects : std::vector<SimPassRect>{});
        ++lastPasses_;
    }
}

void SimPipeline::step(uint32_t frameTick, const PhysicsParams& physics, ActiveTileMode activeMode) {
    lastActiveTileMode_ = activeMode;
    lastActiveTileCount_ = activeTiles.activeCount();
    if (activeMode != ActiveTileMode::Off && lastActiveTileCount_ == 0) {
        lastPasses_ = 0;
        lastActiveTileFallback_ = false;
        return;
    }
    if (backend_ == SimBackend::Compute) {
        stepCompute(frameTick, physics, activeMode);
        return;
    }
    stepFragment(frameTick, physics, activeMode);
}

bool SimPipeline::collectActivePassRects(ActiveTileMode activeMode,
                                         std::vector<SimPassRect>& rects) {
    rects.clear();
    if (activeMode == ActiveTileMode::Off) {
        return false;
    }

    const int expand = activeMode == ActiveTileMode::Aggressive ? 1 : 2;
    std::vector<ActiveTileRun> runs;
    if (!activeTiles.activeRuns(runs, expand)) {
        lastActiveTileFallback_ = true;
        return false;
    }

    int bx0 = gw;
    int by0 = gh;
    int bx1 = -1;
    int by1 = -1;
    int activeArea = 0;
    rects.reserve(runs.size());
    for (const ActiveTileRun& run : runs) {
        bx0 = std::min(bx0, run.x0);
        by0 = std::min(by0, run.y0);
        bx1 = std::max(bx1, run.x1);
        by1 = std::max(by1, run.y1);

        const int glX = std::max(0, run.x0 - 2);
        const int topY = std::max(0, run.y0 - 2);
        const int bottomY = std::min(gh - 1, run.y1 + 2);
        const int rightX = std::min(gw - 1, run.x1 + 2);
        const int glW = rightX - glX + 1;
        const int glH = bottomY - topY + 1;
        if (glW <= 0 || glH <= 0) continue;
        activeArea += glW * glH;
        rects.push_back({glX, gh - bottomY - 1, glW, glH});
    }

    if (bx1 >= bx0 && by1 >= by0) {
        activeTiles.rememberBounds(bx0, by0, bx1, by1);
    }

    const int gridArea = std::max(1, gw * gh);
    const bool tooLarge = activeArea > (gridArea * 45) / 100;
    const bool tooManyRuns = rects.size() > 64;
    if (rects.empty() || tooLarge || tooManyRuns) {
        rects.clear();
        lastActiveTileFallback_ = true;
        return false;
    }
    return true;
}

bool SimPipeline::readGridTo(std::vector<uint8_t>& out) {
    return readRegionTopDown(0, 0, gw, gh, out);
}

bool SimPipeline::readRegionTopDown(int x0, int y0, int w, int h, std::vector<uint8_t>& out) {
    if (x0 < 0 || y0 < 0 || w <= 0 || h <= 0 || x0 + w > gw || y0 + h > gh) return false;
    out.assign(static_cast<size_t>(w * h), MAT_EMPTY);
    if (backend_ == SimBackend::Compute) {
        barrierComputeToTexture();
    }
    attachSimFboAttachments();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[cur]);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    if (gridPixelType_ == GL_UNSIGNED_BYTE) {
        for (int row = 0; row < h; ++row) {
            const int gy = gh - 1 - (y0 + row);
            uint8_t* dst = out.data() + static_cast<size_t>(row * w);
            glReadPixels(x0, gy, w, 1, GL_RED_INTEGER, GL_UNSIGNED_BYTE, dst);
        }
    } else {
        std::vector<GLuint> row(static_cast<size_t>(w), 0);
        for (int r = 0; r < h; ++r) {
            const int gy = gh - 1 - (y0 + r);
            glReadPixels(x0, gy, w, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, row.data());
            for (int x = 0; x < w; ++x) {
                out[static_cast<size_t>(r * w + x)] = static_cast<uint8_t>(row[static_cast<size_t>(x)] & 0xffu);
            }
        }
    }
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
    std::vector<uint8_t> gl8(static_cast<size_t>(gw * gh), 0);
    for (int y = 0; y < gh; ++y) {
        for (int x = 0; x < gw; ++x) {
            gl8[static_cast<size_t>((gh - 1 - y) * gw + x)] =
                top[static_cast<size_t>(y * gw + x)];
        }
    }
    std::vector<GLuint> gl32;
    const void* upload = gl8.data();
    if (gridPixelType_ == GL_UNSIGNED_INT) {
        gl32.resize(gl8.size());
        for (size_t i = 0; i < gl8.size(); ++i) {
            gl32[i] = static_cast<GLuint>(gl8[i]);
        }
        upload = gl32.data();
    }
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gw, gh, gridPixelFormat_, gridPixelType_, upload);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    cur = 0;
    activeTiles.wakeFromGridTopDown(top.data(), gw, gh);
    if (backend_ == SimBackend::Compute) {
        barrierTextureToCompute();
    }
}

} // namespace nx
