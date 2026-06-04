#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "gl_loader.hpp"

namespace nx {

class RenderPipeline;

class FontAtlas {
public:
    struct GlyphInfo {
        float u0 = 0.f;
        float v0 = 0.f;
        float u1 = 0.f;
        float v1 = 0.f;
        int width = 0;
        int height = 0;
        float bearingX = 0.f;
        float bearingY = 0.f;
        int advance = 8;
    };

    GLuint tex = 0;
    mutable int atlasW = 1024;
    mutable int atlasH = 1024;
    int lineH = 20;
    int baseline = 16;
    int pixelSize = 20;
    /// Rasterize glyphs at pixelSize * bakeScale; layout metrics stay in pixelSize units.
    int bakeScale = 2;

    bool init();
    /// True when GL atlas and FreeType face are ready for drawing.
    bool isReady() const;
    void shutdown();
    /// Drop GL atlas only; keeps FreeType / pl session for fast re-init.
    void invalidateGlTexture();
    /// Drop GL texture + FreeType state; next init() rebuilds a clean atlas.
    void resetGlResources();

    float textWidth(const std::string& text, float scale) const;

    void drawText(RenderPipeline& rp, float x, float y, float scale, const std::string& text,
                  float cr, float cg, float cb, float ca, int screenW, int screenH) const;

    void drawTextCentered(RenderPipeline& rp, float cx, float y, float scale, const std::string& text,
                          float cr, float cg, float cb, float ca, int screenW, int screenH) const;

    void prewarmCommonGlyphs();

private:

    mutable std::unordered_map<uint32_t, GlyphInfo> glyphs_;
    mutable std::vector<uint8_t> atlasPixels_;
    mutable int penX_ = 2;
    mutable int penY_ = 2;
    mutable int rowH_ = 0;

    const GlyphInfo& ensureGlyph(uint32_t codepoint) const;
    bool growAtlas() const;
    bool uploadAtlas() const;
    void uploadGlyphRegion(int x, int y, int w, int h) const;
};

} // namespace nx
