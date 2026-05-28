#pragma once
#include "menu.hpp"

namespace nx {

class RenderPipeline;
class FontAtlas;

void drawMenuChromeScrim(RenderPipeline& r, const MenuLayout& L, int W, int H, float topY,
                         float bottomY, float strength = 1.f);
void drawMenuGlassPanel(RenderPipeline& r, const MenuLayout& L, int W, int H,
                        float strength = 1.f);
void drawMenuHeaderRule(RenderPipeline& r, const MenuLayout& L, int W, int H);
void drawMenuHeaderRuleAt(RenderPipeline& r, const MenuLayout& L, float y, int W, int H);
void drawMenuMark(RenderPipeline& r, FontAtlas& font, float cx, float markY, const MenuLayout& L,
                  int W, int H, float strength = 1.f);
void drawMenuRow(RenderPipeline& r, FontAtlas& font, int W, int H, const MenuLayout& L, float y,
                 const char* label, bool selected);
void drawHintPill(RenderPipeline& r, FontAtlas& font, float cx, float y, float s, const char* hint,
                  int W, int H, float strength = 1.f);

} // namespace nx
