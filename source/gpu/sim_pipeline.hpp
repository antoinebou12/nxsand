// Ping-pong integer grid: fragment uses GL_R8UI; compute uses GL_R32UI for GLES imageStore.
#pragma once
#include <cstdint>
#include <vector>
#include "gl_loader.hpp"
#include "shader_program.hpp"
#include "../sim/sim_state.hpp"
#include "../sim/materials.hpp"
#include "../sim/physics_params.hpp"
#include "active_tiles.hpp"
#include "sim_backend.hpp"

namespace nx {

struct SimPassRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

class SimPipeline {
public:
    ShaderProgram simShader;
    ShaderProgram computeShader;
    ShaderProgram paintShader;
    GLuint tex[2] = {0, 0};
    GLuint fbo[2] = {0, 0};
    GLuint vao = 0;
    GLuint vbo = 0;

    int cur = 0;
    ActiveTileMap activeTiles;

    SimPipeline() = default;
    ~SimPipeline();

    bool init(int w, int h, const std::string& shaderDir, SimBackend backend);
    void shutdown();

    void clearAll(Material m);
    void paintDisk(int cx, int cy, int radius, Material m, int* outDirtyW = nullptr,
                   int* outDirtyH = nullptr);
    Material sampleMaterial(int cx, int cy);

    void step(uint32_t frameTick, const PhysicsParams& physics);
    void step(uint32_t frameTick, const PhysicsParams& physics, ActiveTileMode activeMode);
    int lastPasses() const { return lastPasses_; }
    bool lastActiveTileFallback() const { return lastActiveTileFallback_; }
    int lastActiveTileCount() const { return lastActiveTileCount_; }
    ActiveTileMode lastActiveTileMode() const { return lastActiveTileMode_; }
    SimBackend backend() const { return backend_; }

    GLuint readTexture() const { return tex[cur]; }

    bool readGridTo(std::vector<uint8_t>& out);
    void uploadGridTopDown(const std::vector<uint8_t>& data, int w, int h);

    void syncSimForSampling();

private:
    int gw = 0, gh = 0;
    SimBackend backend_ = SimBackend::Fragment;
    GLuint physicsUbo = 0;
    GLuint physicsBlockIndex = 0;
    static constexpr GLuint kPhysicsUboBinding = 2;

    GLint uSimLoc = -1;
    GLint uGridLoc = -1;
    GLint uPhaseLoc = -1;
    GLint uFrameLoc = -1;

    GLint comp_uGridLoc = -1;
    GLint comp_uPhaseLoc = -1;
    GLint comp_uFrameLoc = -1;
    GLint comp_uDispatchOriginLoc = -1;
    GLint comp_uDispatchLimitLoc = -1;

    GLint paint_uSimLoc = -1;
    GLint paint_uGridLoc = -1;
    GLint paint_uCenterLoc = -1;
    GLint paint_uRadiusLoc = -1;
    GLint paint_uMaterialLoc = -1;
    int lastPasses_ = 0;
    bool lastActiveTileFallback_ = false;
    int lastActiveTileCount_ = 0;
    ActiveTileMode lastActiveTileMode_ = ActiveTileMode::Off;

    GLenum gridInternalFormat_ = GL_R8UI;
    GLenum gridPixelFormat_ = GL_RED_INTEGER;
    GLenum gridImageFormat_ = GL_R8UI;
    GLenum gridPixelType_ = GL_UNSIGNED_BYTE;

    void uploadPhysics(const PhysicsParams& physics);
    void runPass(int phaseX, int phaseY, uint32_t frame, int x0 = 0, int y0 = 0, int w = -1,
                 int h = -1);
    void runPassRegions(int phaseX, int phaseY, uint32_t frame,
                        const std::vector<SimPassRect>& rects);
    void runComputePass(int phaseX, int phaseY, uint32_t frame,
                        const std::vector<SimPassRect>& rects);
    void stepFragment(uint32_t frameTick, const PhysicsParams& physics, ActiveTileMode activeMode);
    void stepCompute(uint32_t frameTick, const PhysicsParams& physics, ActiveTileMode activeMode);
    bool collectActivePassRects(ActiveTileMode activeMode, std::vector<SimPassRect>& rects);
    void unbindComputeImages();
    void detachSimFboAttachments();
    void attachSimFboAttachments();
};

} // namespace nx
