#include "font_atlas.hpp"
#include "render_pipeline.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#if defined(__SWITCH__)
#include <switch.h>
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

namespace nx {

namespace {

struct FtLibrary {
    FT_Library lib = nullptr;
    FT_Face face = nullptr;
    ~FtLibrary() {
        if (face) FT_Done_Face(face);
        if (lib) FT_Done_FreeType(lib);
    }
};

static bool readableFontFile(const std::string& path) {
    if (path.empty()) return false;
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

static std::string resolveFontPath() {
#if defined(__SWITCH__)
    return {};
#else
    const char* overridePath = std::getenv("NXSAND_FONT_PATH");
    if (overridePath && overridePath[0]) {
        if (readableFontFile(overridePath)) return overridePath;
        std::cerr << "Font override not readable: " << overridePath << "\n";
    }
    const char* candidates[] = {
        "romfs/fonts/NotoSans-Regular.ttf",
        "fonts/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/inter/Inter-Regular.ttf",
        "/usr/share/fonts/opentype/inter/Inter-Regular.otf",
        "/usr/share/fonts/truetype/noto/NotoSansDisplay-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:/Windows/Fonts/SegoeUIVariable.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/segoeuisl.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (const char* p : candidates) {
        if (readableFontFile(p)) return p;
    }
    return {};
#endif
}

static int decodeUtf8(const char* s, size_t len, size_t& i, uint32_t& out) {
    if (i >= len) return 0;
    unsigned char c0 = static_cast<unsigned char>(s[i]);
    if (c0 < 0x80) {
        out = c0;
        i += 1;
        return 1;
    }
    if ((c0 & 0xE0) == 0xC0 && i + 1 < len) {
        unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        if ((c1 & 0xC0) == 0x80) {
            out = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
            i += 2;
            return 2;
        }
    }
    if ((c0 & 0xF0) == 0xE0 && i + 2 < len) {
        unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
        if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80) {
            out = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
            i += 3;
            return 3;
        }
    }
    if ((c0 & 0xF8) == 0xF0 && i + 3 < len) {
        unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
        unsigned char c3 = static_cast<unsigned char>(s[i + 3]);
        if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
            out = ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            i += 4;
            return 4;
        }
    }
    out = '?';
    i += 1;
    return 1;
}

static FtLibrary g_ft;
#if defined(__SWITCH__)
static bool g_plInitialized = false;
#endif

static void releaseFontResources() {
    if (g_ft.face) {
        FT_Done_Face(g_ft.face);
        g_ft.face = nullptr;
    }
    if (g_ft.lib) {
        FT_Done_FreeType(g_ft.lib);
        g_ft.lib = nullptr;
    }
#if defined(__SWITCH__)
    if (g_plInitialized) {
        plExit();
        g_plInitialized = false;
    }
#endif
}

} // namespace

void FontAtlas::prewarmCommonGlyphs() {
    static const char* kWarm =
        " !\"#$%&'()*+,-./0123456789:;<=>?@"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
        "SELECT MATERIALSand Water Wall Plant Fire Lava Acid Smoke Stone Oil Ice "
        "ACTIVE D-PAD MOVE OK BACK TAB CLOSE"
        "New Sandbox Demo Load Save Game Element Engine Settings Clear Quit "
        "Main Menu defaults Reset Visuals Performance Compute Shader Battery "
        "Balanced Quality fullscreen orientation portrait landscape nearest "
        "conservative sleeping substeps fragment upscale bloom grain flicker";
    for (const char* p = kWarm; *p; ++p) {
        ensureGlyph(static_cast<unsigned char>(*p));
    }
}

bool FontAtlas::growAtlas() const {
    if (atlasW >= 2048) return false;
    atlasW *= 2;
    atlasH *= 2;
    atlasPixels_.assign(static_cast<size_t>(atlasW * atlasH), 0);
    penX_ = 2;
    penY_ = 2;
    rowH_ = 0;
    glyphs_.clear();
    uploadAtlas();
    return true;
}

bool FontAtlas::uploadAtlas() const {
    if (!tex) return false;
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasW, atlasH, 0, GL_RED, GL_UNSIGNED_BYTE,
                  atlasPixels_.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void FontAtlas::uploadGlyphRegion(int x, int y, int w, int h) const {
    if (!tex || w <= 0 || h <= 0) return;
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, atlasW);
    const uint8_t* row = atlasPixels_.data() + static_cast<size_t>(y) * static_cast<size_t>(atlasW) +
                         static_cast<size_t>(x);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RED, GL_UNSIGNED_BYTE, row);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

const FontAtlas::GlyphInfo& FontAtlas::ensureGlyph(uint32_t codepoint) const {
    auto it = glyphs_.find(codepoint);
    if (it != glyphs_.end()) return it->second;

    if (!g_ft.face) {
        static GlyphInfo empty{};
        return empty;
    }

    const float invBake = 1.f / float(std::max(1, bakeScale));

    for (;;) {
        const FT_Int32 loadFlags = FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL;
        FT_Error err = FT_Load_Char(g_ft.face, codepoint, loadFlags);
        if (err) {
            GlyphInfo miss{};
            miss.advance = pixelSize / 2;
            glyphs_[codepoint] = miss;
            return glyphs_[codepoint];
        }

        FT_GlyphSlot slot = g_ft.face->glyph;
        FT_Bitmap& bitmap = slot->bitmap;
        const int bw = static_cast<int>(bitmap.width);
        const int bh = static_cast<int>(bitmap.rows);

        if (penX_ + bw + 3 > atlasW) {
            penX_ = 2;
            penY_ += rowH_ + 3;
            rowH_ = 0;
        }
        if (penY_ + bh + 3 > atlasH) {
            if (!growAtlas()) {
                GlyphInfo miss{};
                miss.advance = pixelSize / 2;
                glyphs_[codepoint] = miss;
                return glyphs_[codepoint];
            }
            continue;
        }

        const int atlasX = penX_;
        const int atlasY = penY_;

        GlyphInfo g{};
        g.width = bw;
        g.height = bh;
        g.bearingX = float(slot->bitmap_left) * invBake;
        g.bearingY = float(slot->bitmap_top) * invBake;
        g.advance = std::max(1, int(std::lround(float(slot->advance.x >> 6) * invBake)));
        g.u0 = float(atlasX) / float(atlasW);
        g.v0 = float(atlasY) / float(atlasH);
        g.u1 = float(atlasX + bw) / float(atlasW);
        g.v1 = float(atlasY + bh) / float(atlasH);

        if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY && bw > 0 && bh > 0) {
            for (int y = 0; y < bh; ++y) {
                const uint8_t* src = bitmap.buffer + y * bitmap.pitch;
                for (int x = 0; x < bw; ++x) {
                    atlasPixels_[static_cast<size_t>((atlasY + y) * atlasW + (atlasX + x))] = src[x];
                }
            }
            uploadGlyphRegion(atlasX, atlasY, bw, bh);
        }

        penX_ += bw + 3;
        rowH_ = std::max(rowH_, bh);
        glyphs_[codepoint] = g;
        return glyphs_[codepoint];
    }
}

bool FontAtlas::init() {
    if (tex) return true;

    FT_Error err = FT_Init_FreeType(&g_ft.lib);
    if (err) {
        std::cerr << "FT_Init_FreeType failed: " << err << "\n";
        return false;
    }

    std::string path = resolveFontPath();
#if defined(__SWITCH__)
    Result rc = plInitialize(PlServiceType_User);
    if (R_FAILED(rc)) {
        std::cerr << "plInitialize failed: 0x" << std::hex << rc << std::dec << "\n";
        releaseFontResources();
        return false;
    }
    g_plInitialized = true;

    PlFontData font{};
    rc = plGetSharedFontByType(&font, PlSharedFontType_Standard);
    if (R_FAILED(rc)) {
        std::cerr << "plGetSharedFontByType failed: 0x" << std::hex << rc << std::dec << "\n";
        releaseFontResources();
        return false;
    }

    err = FT_New_Memory_Face(g_ft.lib, static_cast<const FT_Byte*>(font.address),
                             static_cast<FT_Long>(font.size), 0, &g_ft.face);
    path = "Switch shared font";
#else
    if (path.empty()) {
        std::cerr << "No usable desktop font found. Set NXSAND_FONT_PATH to a .ttf/.otf file.\n";
        releaseFontResources();
        return false;
    }
    err = FT_New_Face(g_ft.lib, path.c_str(), 0, &g_ft.face);
#endif
    if (err) {
        std::cerr << "FT_New_Face failed for " << path << ": " << err << "\n";
        releaseFontResources();
        return false;
    }

    pixelSize = 22;
    bakeScale = 2;
    const int rasterPx = pixelSize * bakeScale;
    err = FT_Set_Pixel_Sizes(g_ft.face, 0, rasterPx);
    if (err) {
        std::cerr << "FT_Set_Pixel_Sizes failed: " << err << "\n";
        releaseFontResources();
        return false;
    }

    lineH = std::max(pixelSize, int(std::lround(float(g_ft.face->size->metrics.height >> 6) /
                                                float(bakeScale))));
    baseline = std::max(pixelSize * 7 / 10,
                        int(std::lround(float(g_ft.face->size->metrics.ascender >> 6) /
                                        float(bakeScale))));

    atlasW = 1024;
    atlasH = 1024;
    atlasPixels_.assign(static_cast<size_t>(atlasW * atlasH), 0);
    penX_ = 2;
    penY_ = 2;
    rowH_ = 0;
    glyphs_.clear();

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasW, atlasH, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    prewarmCommonGlyphs();
    std::cout << "Font: " << path << " (" << glyphs_.size() << " glyphs pre-warmed)\n";
    return tex != 0;
}

void FontAtlas::shutdown() {
    releaseFontResources();
    glyphs_.clear();
    atlasPixels_.clear();
    if (tex) glDeleteTextures(1, &tex);
    tex = 0;
}

float FontAtlas::textWidth(const std::string& text, float scale) const {
    float w = 0.f;
    size_t i = 0;
    const size_t len = text.size();
    while (i < len) {
        uint32_t cp = 0;
        if (decodeUtf8(text.data(), len, i, cp) <= 0) break;
        if (cp == '\n') break;
        const GlyphInfo& g = ensureGlyph(cp);
        w += float(g.advance) * scale;
    }
    return w;
}

void FontAtlas::drawText(RenderPipeline& rp, float x, float y, float scale, const std::string& text,
                         float cr, float cg, float cb, float ca, int screenW, int screenH) const {
    if (!tex) return;
    float penX = x;
    size_t i = 0;
    const size_t len = text.size();
    while (i < len) {
        uint32_t cp = 0;
        if (decodeUtf8(text.data(), len, i, cp) <= 0) break;
        if (cp == '\n') {
            penX = x;
            y += float(lineH) * scale;
            continue;
        }
        const GlyphInfo& g = ensureGlyph(cp);
        if (g.width > 0 && g.height > 0) {
            const float invBake = 1.f / float(std::max(1, bakeScale));
            const float dx = penX + g.bearingX * scale;
            const float dy = y + float(baseline) * scale - g.bearingY * scale;
            const float gw = std::max(0.5f, float(g.width) * scale * invBake);
            const float gh = std::max(0.5f, float(g.height) * scale * invBake);
            rp.drawAlphaMaskRect(dx, dy, gw, gh, g.u0, g.v0, g.u1, g.v1, cr, cg, cb, ca, screenW,
                                 screenH, tex);
        }
        penX += float(g.advance) * scale;
    }
}

void FontAtlas::drawTextCentered(RenderPipeline& rp, float cx, float y, float scale,
                                 const std::string& text, float cr, float cg, float cb, float ca,
                                 int screenW, int screenH) const {
    drawText(rp, cx - textWidth(text, scale) * 0.5f, y, scale, text, cr, cg, cb, ca, screenW, screenH);
}

} // namespace nx
