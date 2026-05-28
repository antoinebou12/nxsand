#pragma once
#include "menu.hpp"

namespace nx {

class RenderPipeline;
class FontAtlas;

float menuMarkTextScale(float uiScale);
float menuCrumbTextScale(float uiScale);
float menuMarkBoxHeight(const FontAtlas& font, float uiScale);
float menuMarkBandHeight(const FontAtlas& font, float uiScale);
float menuCrumbBandHeight(const FontAtlas& font, float uiScale);
float menuBaselineInBand(float boxTop, float bandH, const FontAtlas& font, float textScale);

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
