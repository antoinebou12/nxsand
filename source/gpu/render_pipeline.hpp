#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "gl_loader.hpp"
#include "shader_program.hpp"
#include "../game/game_settings.hpp"
#include "../sim/materials.hpp"
#include "../ui/layout.hpp"

namespace nx {

class RenderPipeline {
public:
    ShaderProgram palShader;
    ShaderProgram upscaleShader;
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
    GLint pal_uBlob = -1;
    GLint pal_uAo = -1;

    GLint up_uSrc = -1;
    GLint up_uSrcSize = -1;
    GLint up_uDstSize = -1;
    GLint up_uFilter = -1;

    int paletteMode_ = 0; // 0 pretty, 1 fast, 2 classic; 3 = material IDs (debug only)
    UpscaleFilter upscaleFilter_ = UpscaleFilter::Nearest;
    bool blobEnabled_ = true;
    VisualBloom bloom_ = VisualBloom::Off;
    int bloomBlurPasses_ = 4;
    float bloomScalar_ = 1.0f;
    float bloomExposure_ = 0.5f;
    float bloomGamma_ = 2.2f;
    float bloomSaturation_ = 2.0f;
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
    void setUpscaleFilter(UpscaleFilter filter);
    UpscaleFilter upscaleFilter() const { return upscaleFilter_; }
    void setBlobEnabled(bool on) { blobEnabled_ = on; }
    void setBloomLevel(VisualBloom level);
    bool bloomEnabled() const { return bloom_ != VisualBloom::Off; }
    void setFlickerEnabled(bool on) { flickerEnabled_ = on; }
    void setGrainEnabled(bool on) { grainEnabled_ = on; }
    void setAoStrength(float v) { aoStrength_ = v; }
    void releaseBloomTargets();

    // Solid / textured UI quads in pixel space (screen W/H).
    void drawSolidRect(float x, float y, float w, float h, float r, float g, float b, float a,
                       int screenW, int screenH);

    void drawTexturedRect(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                          float r, float g, float b, float a, int screenW, int screenH, GLuint tex);

    // Font glyphs: sample red channel as alpha (ui_quad uMode 2).
    void drawAlphaMaskRect(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                           float r, float g, float b, float a, int screenW, int screenH, GLuint tex);

    void beginUiFrame();
    void endUiFrame();

private:
    void ensureQuadVbo();
    void ensureUiQuadVbo();
    void drawUiQuad(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                    float r, float g, float b, float a, int screenW, int screenH, GLuint texture,
                    int mode);
    void flushUiBatch();
    void ensureBloomTargets(int simW, int simH);
    void ensureLookTargets(int simW, int simH);
    void releaseLookTargets();
    void drawPalettePass(GLuint simR8UI, int simW, int simH, uint32_t frame, int mode);
    void runBloomPipeline();
    void blitPostToPlayRegion(const PlayRegion& pr, int screenH, int simW, int simH, bool filtered);

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

    ShaderProgram bloomBrightShader;
    ShaderProgram bloomBlurShader;
    ShaderProgram bloomCompositeShader;

    GLint bright_uTex = -1;
    GLint blur_uTex = -1;
    GLint blur_uTexSize = -1;
    GLint blur_uDir = -1;
    GLint comp_uTex = -1;
    GLint comp_uBlurTex = -1;
    GLint comp_uBloomScalar = -1;
    GLint comp_uExposure = -1;
    GLint comp_uGamma = -1;
    GLint comp_uSaturation = -1;

    GLuint brightFbo = 0;
    GLuint brightTex = 0;
    int brightW = 0;
    int brightH = 0;

    GLuint blurFbo[2] = {0, 0};
    GLuint blurTex[2] = {0, 0};
    int blurW = 0;
    int blurH = 0;

    GLuint postFbo = 0;
    GLuint postTex = 0;
    int postW = 0;
    int postH = 0;

    int bloomSimW = 0;
    int bloomSimH = 0;

    GLuint lookFbo = 0;
    GLuint lookTex = 0;
    int lookW = 0;
    int lookH = 0;
};

} // namespace nx
