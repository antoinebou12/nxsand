#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "gl_loader.hpp"
#include "shader_program.hpp"
#include "../sim/materials.hpp"
#include "../ui/layout.hpp"

namespace nx {

class RenderPipeline {
public:
    ShaderProgram palShader;
    ShaderProgram uiShader;
    GLuint palTex = 0;   // 256x1 RGBA8
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint uiVao = 0;
    GLuint uiVbo = 0;

    GLint pal_uSim = -1;
    GLint pal_uPal = -1;
    GLint pal_uGrid = -1;
    GLint pal_uFrame = -1;
    GLint pal_uMode = -1;
    GLint pal_uFlicker = -1;
    GLint pal_uGrain = -1;
    GLint pal_uAo = -1;

    int paletteMode_ = 0; // 0 pretty, 1 fast, 2 debug
    bool glowEnabled_ = false;
    bool flickerEnabled_ = true;
    bool grainEnabled_ = false;
    float aoStrength_ = 0.04f;

    GLint ui_uScreen = -1;
    GLint ui_uTex = -1;
    GLint ui_uMode = -1;

    RenderPipeline() = default;
    ~RenderPipeline();

    bool init(const std::string& shaderDir);
    void shutdown();

    void buildPaletteTexture();

    // Draw sim (R32UI / uint material id) scaled into play region on default framebuffer.
    void drawSimulation(GLuint simR8UI, int simW, int simH, const PlayRegion& pr, int screenH,
                         uint32_t frame, double simMsForStressMode = 0.0);

    void setPaletteMode(int mode);
    int paletteMode() const { return paletteMode_; }
    void setGlowEnabled(bool on) { glowEnabled_ = on; }
    bool glowEnabled() const { return glowEnabled_; }
    void setFlickerEnabled(bool on) { flickerEnabled_ = on; }
    void setGrainEnabled(bool on) { grainEnabled_ = on; }
    void setAoStrength(float v) { aoStrength_ = v; }

    // Solid / textured UI quads in pixel space (screen W/H).
    void drawSolidRect(float x, float y, float w, float h, float r, float g, float b, float a,
                       int screenW, int screenH);

    void drawTexturedRect(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                          float r, float g, float b, float a, int screenW, int screenH, GLuint tex);

    // Font glyphs: sample red channel as alpha (ui_quad uMode 2).
    void drawAlphaMaskRect(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                           float r, float g, float b, float a, int screenW, int screenH, GLuint tex);

    // Additive glow composite over play region (samples sim R32UI).
    void drawGlow(GLuint simR8UI, int simW, int simH, const PlayRegion& pr, int screenW,
                  int screenH, uint32_t frame);

    void beginUiFrame();
    void endUiFrame();

private:
    void ensureQuadVbo();
    void ensureUiQuadVbo();
    void drawUiQuad(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                    float r, float g, float b, float a, int screenW, int screenH, GLuint texture,
                    int mode);
    void flushUiBatch();

    struct UiVertex {
        float x, y, u, v;
        float r, g, b, a;
    };
    static constexpr size_t kUiBatchMaxVerts = 16384;
    std::vector<UiVertex> uiBatch_;
    GLuint uiBatchTex_ = 0;
    int uiBatchMode_ = -1;
    int uiBatchScreenW_ = 0;
    int uiBatchScreenH_ = 0;

    ShaderProgram glowExtractShader;
    ShaderProgram glowBlurShader;
    GLuint glowFbo[2] = {0, 0};
    GLuint glowTex[2] = {0, 0};
    int glowW = 0, glowH = 0;
    GLuint lookFbo = 0;
    GLuint lookTex = 0;
};

} // namespace nx
