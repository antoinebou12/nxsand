// Ping-pong GL_R8UI grid + 4-phase Margolus via GLES 3.0 fragment passes (shaders/sim.frag).
#pragma once
#include <cstdint>
#include <vector>
#include "gl_loader.hpp"
#include "shader_program.hpp"
#include "../sim/sim_state.hpp"
#include "../sim/materials.hpp"
#include "../sim/physics_params.hpp"
#include "active_tiles.hpp"

namespace nx {

class SimPipeline {
public:
    ShaderProgram simShader;
    ShaderProgram paintShader;
    GLuint tex[2] = {0, 0};
    GLuint fbo[2] = {0, 0};
    GLuint vao = 0;
    GLuint vbo = 0;

    int cur = 0;
    ActiveTileMap activeTiles;

    SimPipeline() = default;
    ~SimPipeline();

    bool init(int w, int h, const std::string& shaderDir);
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

    GLuint readTexture() const { return tex[cur]; }

    bool readGridTo(std::vector<uint8_t>& out);
    void uploadGridTopDown(const std::vector<uint8_t>& data, int w, int h);

    void syncSimForSampling();

private:
    int gw = 0, gh = 0;
    GLuint physicsUbo = 0;
    GLuint physicsBlockIndex = 0;
    static constexpr GLuint kPhysicsUboBinding = 2;

    GLint uSimLoc = -1;
    GLint uGridLoc = -1;
    GLint uPhaseLoc = -1;
    GLint uFrameLoc = -1;

    GLint paint_uSimLoc = -1;
    GLint paint_uGridLoc = -1;
    GLint paint_uCenterLoc = -1;
    GLint paint_uRadiusLoc = -1;
    GLint paint_uMaterialLoc = -1;
    int lastPasses_ = 0;
    bool lastActiveTileFallback_ = false;
    int lastActiveTileCount_ = 0;
    ActiveTileMode lastActiveTileMode_ = ActiveTileMode::Off;

    void uploadPhysics(const PhysicsParams& physics);
    void runPass(int phaseX, int phaseY, uint32_t frame, int x0 = 0, int y0 = 0, int w = -1,
                 int h = -1);
};

} // namespace nx
