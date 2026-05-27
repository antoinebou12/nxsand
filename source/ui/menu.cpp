#include "menu.hpp"
#include "../game/app.hpp"
#include "../game/benchmark_scene.hpp"
#include "../game/engine_settings.hpp"
#include "../gpu/font_atlas.hpp"
#include "../gpu/render_pipeline.hpp"
#include "../save/save.hpp"
#include "../save/settings_io.hpp"
#include "../save/physics_params_io.hpp"
#include "../sim/physics_params.hpp"
#include "../sim/physics_settings.hpp"
#include "../ui/menu_fx.hpp"
#include "../ui/theme.hpp"
#include "menu_sim.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace nx {

static constexpr int kMainItemCount = 8;
static constexpr int kSlotItemCount = 4;

static int menuItemCount(const MenuState& m) {
    switch (m.screen) {
        case MenuScreen::Main: return kMainItemCount;
        case MenuScreen::Load:
        case MenuScreen::Save: return kSlotItemCount;
        case MenuScreen::Settings: return settingsMaterialCount() + 1;
        case MenuScreen::SettingsEdit: return paramCountFor(m.settingsMat) + 2;
        case MenuScreen::EngineSettings: return static_cast<int>(EngineTab::Count) + 1;
        case MenuScreen::EngineSettingsTab: return engineTabRowCount(m.engineTab) + 1;
    }
    return 0;
}

static bool menuIsBackItem(const MenuState& m, int i) {
    if (m.screen == MenuScreen::Load || m.screen == MenuScreen::Save)
        return i == kSlotItemCount - 1;
    if (m.screen == MenuScreen::Settings) return i == settingsMaterialCount();
    if (m.screen == MenuScreen::SettingsEdit) return i == 0;
    if (m.screen == MenuScreen::EngineSettings) return i == static_cast<int>(EngineTab::Count);
    if (m.screen == MenuScreen::EngineSettingsTab) return i == 0;
    return false;
}

static const char* menuBreadcrumb(const MenuState& m) {
    switch (m.screen) {
        case MenuScreen::Load: return "Load Game";
        case MenuScreen::Save: return "Save Game";
        case MenuScreen::Settings: return "Element Settings";
        case MenuScreen::SettingsEdit: return material_name(m.settingsMat);
        case MenuScreen::EngineSettings: return "Engine Settings";
        case MenuScreen::EngineSettingsTab: return engineTabLabel(m.engineTab);
        default: return "";
    }
}

void MenuState::resetMain() {
    screen = MenuScreen::Main;
    index = 0;
    materialWheelOpen = false;
}

void MenuState::goBack(App& app) {
    if (screen == MenuScreen::SettingsEdit) {
        screen = MenuScreen::Settings;
        index = 0;
        settingsParamRow = 0;
        return;
    }
    if (screen == MenuScreen::EngineSettingsTab) {
        screen = MenuScreen::EngineSettings;
        index = static_cast<int>(engineTab);
        return;
    }
    if (screen == MenuScreen::EngineSettings) {
        flushGameSettingsIfDirty(app.settings);
    }
    if (screen != MenuScreen::Main) {
        screen = MenuScreen::Main;
        index = 0;
        return;
    }
    if (app.hasEnteredPlay)
        app.scene = Scene::Play;
}

void MenuState::handleConfirm(App& app) {
    if (menuIsBackItem(*this, index)) {
        if (screen == MenuScreen::SettingsEdit) flushPhysicsParamsIfDirty(app.physics);
        if (screen == MenuScreen::EngineSettingsTab) flushGameSettingsIfDirty(app.settings);
        goBack(app);
        return;
    }

    if (screen == MenuScreen::Main) {
        switch (index) {
            case 0:
                app.simPipeline->clearAll(MAT_EMPTY);
                app.hasEnteredPlay = true;
                app.scene = Scene::Play;
                app.sim.tick = 0;
                app.toast.show("New empty sandbox", 1.0f);
                break;
            case 1:
                seedStarterScene(app);
                app.hasEnteredPlay = true;
                app.scene = Scene::Play;
                app.sim.tick = 0;
                break;
            case 2:
                screen = MenuScreen::Load;
                index = 0;
                break;
            case 3:
                screen = MenuScreen::Save;
                index = 0;
                break;
            case 4:
                screen = MenuScreen::Settings;
                index = 0;
                break;
            case 5:
                screen = MenuScreen::EngineSettings;
                index = 0;
                break;
            case 6:
                app.simPipeline->clearAll(MAT_EMPTY);
                app.toast.show("Cleared", 1.0f);
                break;
            case 7:
                app.input.quitRequested = true;
                break;
            default: break;
        }
    } else if (screen == MenuScreen::Load) {
        const SlotMeta meta = getSlotMeta(index + 1);
        if (meta.empty) {
            app.toast.show("Slot is empty", 1.5f);
        } else if (loadGame(app, index + 1)) {
            app.hasEnteredPlay = true;
            app.scene = Scene::Play;
            app.toast.show("Loaded", 1.2f);
        } else {
            app.toast.show("Load failed", 1.5f);
        }
    } else if (screen == MenuScreen::Save) {
        if (saveGame(app, index + 1))
            app.toast.show("Saved", 1.2f);
        else
            app.toast.show("Save failed", 1.5f);
    } else if (screen == MenuScreen::Settings) {
        settingsMat = settingsMaterialAt(index);
        settingsParamRow = 0;
        screen = MenuScreen::SettingsEdit;
        index = 0;
    } else if (screen == MenuScreen::SettingsEdit) {
        const int rows = paramCountFor(settingsMat) + 2;
        if (index == rows - 1) {
            app.physics = PhysicsParams{};
            markPhysicsParamsDirty();
            app.toast.show("Defaults reset", 1.0f);
        }
    } else if (screen == MenuScreen::EngineSettings) {
        engineTab = static_cast<EngineTab>(index);
        screen = MenuScreen::EngineSettingsTab;
        index = 1;
    } else if (screen == MenuScreen::EngineSettingsTab) {
        if (index > 0 && engineTab == EngineTab::Debug &&
            index - 1 == engineTabRowCount(EngineTab::Debug) - 1) {
            seedBenchmarkScene(app, app.settings.debug.benchmarkScene);
        }
    }
}

void MenuState::adjustHorizontal(App& app, int dir) {
    if (screen == MenuScreen::EngineSettingsTab && index > 0) {
        adjustEngineTabRow(app, engineTab, index - 1, dir);
        flushGameSettingsIfDirty(app.settings);
        return;
    }
    if (screen != MenuScreen::SettingsEdit) return;
    const int rows = paramCountFor(settingsMat) + 2;
    if (index <= 0 || index >= rows - 1) return;
    const ParamSpec* spec = paramSpecAt(settingsMat, index - 1);
    if (!spec) return;
    adjustParam(app.physics, settingsMat, spec->id, dir);
    markPhysicsParamsDirty();
    flushPhysicsParamsIfDirty(app.physics);
}

static void clampIndex(MenuState& m) {
    const int max = menuItemCount(m) - 1;
    if (m.index > max) m.index = max;
    if (m.index < 0) m.index = 0;
}

void MenuState::moveVertical(int delta) {
    index += delta;
    clampIndex(*this);
}

static void formatSlotLabel(int slot, char* buf, size_t bufSize) {
    const SlotMeta meta = getSlotMeta(slot);
    if (meta.empty) {
        std::snprintf(buf, bufSize, "Slot %d - empty", slot);
        return;
    }
    if (meta.timestampMs > 0) {
        const std::time_t t = static_cast<std::time_t>(meta.timestampMs / 1000);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &t);
#else
        localtime_r(&t, &local);
#endif
        char when[32];
        if (std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &local) > 0) {
            std::snprintf(buf, bufSize, "Slot %d - %s", slot, when);
            return;
        }
    }
    std::snprintf(buf, bufSize, "Slot %d - saved", slot);
}

struct MenuLayout {
    float s;
    float panelX;
    float panelY;
    float panelW;
    float panelH;
    float rowH;
    float listY0;
    float titleY;
    float logoY;
    float footerY;
    int visibleRows;
    int firstRow;
};

static MenuLayout computeLayout(int W, int H, int itemCount, int selectedIndex,
                                bool hasBreadcrumb, float accessibilityScale) {
    MenuLayout L{};
    L.s = theme::uiScale(W, H, accessibilityScale);
    const bool portrait = W < H;
#if defined(__SWITCH__)
    const float sideMargin = portrait ? 92.f * L.s : 156.f * L.s;
    const float safeBottom = 132.f * L.s;
    const float safeTop = 34.f * L.s;
#else
    const float sideMargin = 42.f * L.s;
    const float safeBottom = 64.f * L.s;
    const float safeTop = 22.f * L.s;
#endif
    L.panelW = std::min(590.f * L.s, std::max(220.f * L.s, float(W) - sideMargin * 2.f));
    L.titleY = safeTop + 18.f * L.s;
    L.logoY = std::max(10.f * L.s, L.titleY - 54.f * L.s);
    const float titleBlock = hasBreadcrumb ? 128.f * L.s : 108.f * L.s;
    L.panelY = L.titleY + titleBlock;
    L.footerY = float(H) - safeBottom * 0.74f;
    const float panelChrome = 72.f * L.s;
    const float available = std::max(160.f * L.s, L.footerY - L.panelY - 32.f * L.s);
    L.rowH = std::clamp(46.f * L.s, 38.f * L.s, 54.f * L.s);
    const MenuListWindow win =
        computeMenuListWindow(itemCount, selectedIndex, available - panelChrome, L.rowH);
    L.visibleRows = win.visibleRows;
    L.firstRow = win.firstRow;
    L.panelH = L.rowH * float(L.visibleRows) + panelChrome;
    L.panelX = (float(W) - L.panelW) * 0.5f;
    L.listY0 = L.panelY + 42.f * L.s;
    return L;
}

static void drawSpaceGradient(RenderPipeline& rp, int W, int H) {
    const int bands = 5;
    const float bh = float(H) / float(bands);
    for (int i = 0; i < bands; ++i) {
        const float t = float(i) / float(bands - 1);
        const float u = std::fabs(t - 0.5f) * 2.f;
        const float rv = 0.08f + u * 0.02f;
        const float gv = 0.06f + u * 0.02f;
        const float bv = 0.14f + u * 0.04f;
        rp.drawSolidRect(0.f, bh * float(i), float(W), bh + 1.f, rv, gv, bv, 1.f, W, H);
    }
}

#if !defined(__SWITCH__)
static void drawCornerGlows(RenderPipeline& r, int W, int H) {
    const float gw = float(W) * 0.55f;
    const float gh = float(H) * 0.5f;
    r.drawSolidRect(-gw * 0.2f, float(H) - gh, gw, gh, 0.30f, 0.92f, 0.77f, 0.07f, W, H);
    r.drawSolidRect(float(W) - gw * 0.8f, -gh * 0.15f, gw, gh * 0.85f, 0.63f, 0.31f, 1.f, 0.06f, W, H);
}

static void drawLavaGradient(RenderPipeline& r, int W, int H) {
    const float gh = float(H) * 0.35f;
    const float y0 = float(H) - gh;
    r.drawSolidRect(0.f, y0, float(W), gh * 0.4f, 1.f, 0.35f, 0.12f, 0.04f, W, H);
    r.drawSolidRect(0.f, y0 + gh * 0.4f, float(W), gh * 0.35f, 1.f, 0.24f, 0.08f, 0.10f, W, H);
    r.drawSolidRect(0.f, y0 + gh * 0.75f, float(W), gh * 0.25f, 1.f, 0.16f, 0.04f, 0.18f, W, H);
}
#endif

static void drawMenuChromeScrim(RenderPipeline& r, const MenuLayout& L, int W, int H, float topY,
                                float bottomY) {
    const float padX = 28.f * L.s;
    const float x = L.panelX - padX;
    const float w = L.panelW + padX * 2.f;
    const float y = topY;
    const float h = std::max(8.f, bottomY - topY);
    r.drawSolidRect(x, y, w, h, 0.05f, 0.06f, 0.10f, 0.94f, W, H);
}

static void drawGlassPanel(RenderPipeline& r, const MenuLayout& L, int W, int H) {
    const float inset = 4.f * L.s;
    const float x = L.panelX + inset;
    const float y = L.panelY + inset;
    const float w = L.panelW - inset * 2.f;
    const float h = L.panelH - inset * 2.f;
    r.drawSolidRect(x, y, w, h, 0.08f, 0.09f, 0.14f, 0.55f, W, H);
    r.drawSolidRect(x, y, w, h, 0.22f, 0.18f, 0.45f, 0.18f, W, H);
    r.drawSolidRect(x, y, w, h * 0.22f, 1.f, 1.f, 1.f, 0.06f, W, H);
    r.drawSolidRect(x, y + h - 2.f, w, 2.f, 0.30f, 0.92f, 0.77f, 0.22f, W, H);
}

#if !defined(__SWITCH__)
static void drawAnimatedLogo(RenderPipeline& r, float cx, float cy, int tick, int W, int H) {
    constexpr int n = 32;
    for (int i = 0; i < n; ++i) {
        const float a = (float(i) / float(n)) * 6.2831853f + tick * 0.018f;
        const float wave = std::sin(tick * 0.04f + i * 0.6f) * 7.f;
        const float rad = 40.f + wave;
        const float x = cx + std::cos(a) * rad;
        const float y = cy + std::sin(a) * rad * 0.55f + std::sin(tick * 0.07f + i * 0.4f) * 4.f;
        const int phase = (i + tick / 2) % n;
        float cr, cg, cb;
        if (phase < n / 3) {
            cr = 0.37f;
            cg = 0.92f;
            cb = 0.83f;
        } else if (phase < (2 * n) / 3) {
            cr = 0.78f;
            cg = 0.63f;
            cb = 0.31f;
        } else {
            cr = 0.23f;
            cg = 0.60f;
            cb = 0.91f;
        }
        const float sz = 2.5f + (i % 3 == 0 ? 1.5f : 0.f);
        r.drawSolidRect(x - sz, y - sz, sz * 2.f, sz * 2.f, cr, cg, cb, 1.f, W, H);
    }
}
#endif

static void fitLabel(char* dst, size_t dstSize, const char* label, float rowW, float textScale) {
    if (!dst || dstSize == 0) return;
    if (!label) label = "";
    const int maxChars = std::max(8, int(rowW / std::max(1.f, textScale * 8.5f)));
    std::snprintf(dst, dstSize, "%s", label);
    const size_t len = std::strlen(dst);
    if (int(len) <= maxChars || maxChars >= int(dstSize) - 1) return;
    const int keep = std::max(5, maxChars - 3);
    dst[keep] = '.';
    dst[keep + 1] = '.';
    dst[keep + 2] = '.';
    dst[keep + 3] = '\0';
}

static void drawRow(RenderPipeline& r, FontAtlas& font, int W, int H, const MenuLayout& L, float y,
                    const char* label, bool selected) {
    const float padX = 28.f * L.s;
    const float x = L.panelX + padX;
    const float rowW = L.panelW - padX * 2.f;
    const float rowH = L.rowH - 8.f * L.s;
    const float a = selected ? 0.96f : 0.70f;
    r.drawSolidRect(x, y, rowW, rowH, selected ? 0.16f : 0.09f, selected ? 0.22f : 0.13f,
                    selected ? 0.30f : 0.19f, a, W, H);
    // Subtle bottom hairline gives each row a visible baseline so they don't run
    // into each other when the panel sits over a busy backdrop.
    if (!selected) {
        r.drawSolidRect(x + 8.f * L.s, y + rowH - 1.f, rowW - 16.f * L.s, 1.f,
                        0.35f, 0.42f, 0.55f, 0.18f, W, H);
    }
    if (selected) {
        r.drawSolidRect(x, y, 6.f * L.s, rowH, 0.30f, 0.79f, 0.77f, 1.f, W, H);
    }
    char fitted[96];
#if defined(__SWITCH__)
    const float textScale = 1.24f * L.s;
    const float textY = y + 11.f * L.s;
#else
    const float textScale = 1.46f * L.s;
    const float textY = y + 12.f * L.s;
#endif
    fitLabel(fitted, sizeof(fitted), label, rowW - 46.f * L.s, textScale);
    font.drawText(r, x + 32.f * L.s, textY, textScale, fitted, 0.92f, 0.96f, 1.f, 1.f, W, H);
}

template <typename Fn>
static void drawVisibleRows(RenderPipeline& r, FontAtlas& font, int W, int H, const MenuLayout& L,
                            int totalRows, int selected, Fn labelFor) {
    for (int row = 0; row < L.visibleRows; ++row) {
        const int i = L.firstRow + row;
        if (i < 0 || i >= totalRows) continue;
        const float y = L.listY0 + float(row) * L.rowH;
        drawRow(r, font, W, H, L, y, labelFor(i), i == selected);
    }
    const float x = L.panelX + L.panelW - 18.f * L.s;
    const float top = L.listY0;
    const float h = L.rowH * float(L.visibleRows) - 8.f * L.s;
    if (L.firstRow > 0) {
        r.drawSolidRect(x, top + 4.f * L.s, 8.f * L.s, 8.f * L.s, 0.65f, 0.76f, 0.86f, 0.65f, W, H);
    }
    if (L.firstRow + L.visibleRows < totalRows) {
        r.drawSolidRect(x, top + h - 12.f * L.s, 8.f * L.s, 8.f * L.s, 0.65f, 0.76f, 0.86f, 0.65f,
                        W, H);
    }
}

static const char* navHintFor(MenuScreen screen, bool hasEnteredPlay) {
    switch (screen) {
        case MenuScreen::Main:
#if defined(__SWITCH__)
            return hasEnteredPlay ? "D-pad move   A select   B resume" : "D-pad move   A select";
#else
            return hasEnteredPlay ? "Arrows move   Enter select   Esc resume"
                                    : "Arrows move   Enter select";
#endif
        case MenuScreen::SettingsEdit:
        case MenuScreen::EngineSettingsTab:
#if defined(__SWITCH__)
            return "D-pad adjust   A confirm   B back";
#else
            return "Up/Down pick   Left/Right adjust   Enter confirm   Esc back";
#endif
        default:
#if defined(__SWITCH__)
            return "D-pad move   A select   B back";
#else
            return "Arrows move   Enter select   Esc back";
#endif
    }
}

void drawMenuSolid(RenderPipeline& r, FontAtlas& font, App& app) {
    const int W = app.screenW, H = app.screenH;
    const int items = menuItemCount(app.menu);
    const bool hasBreadcrumb = app.menu.screen != MenuScreen::Main;
    const MenuLayout L =
        computeLayout(W, H, items, app.menu.index, hasBreadcrumb, app.settings.accessibility.uiScale);
    const int tick = app.menu.tick;
    const bool mainMenuFlow = app.menu.screen == MenuScreen::Main || app.menu.screen == MenuScreen::Load ||
                              app.menu.screen == MenuScreen::Save;
    const bool showBackdrop =
        app.settings.display.menuChrome == MenuChrome::Full && mainMenuFlow;

    drawSpaceGradient(r, W, H);
    if (showBackdrop) {
        app.menuSim.draw(r, W, H, L.s);
        drawMenuBackgroundFx(r, W, H, tick);
#if !defined(__SWITCH__)
        drawCornerGlows(r, W, H);
        drawLavaGradient(r, W, H);
        drawAnimatedLogo(r, float(W) * 0.5f, L.logoY, tick, W, H);
#endif
    } else {
        drawMenuBackgroundFx(r, W, H, tick);
    }

    const float scrimTop = L.logoY - 24.f * L.s;
    const float scrimBottom = L.panelY + L.panelH + 14.f * L.s;
    drawMenuChromeScrim(r, L, W, H, scrimTop, scrimBottom);
    drawGlassPanel(r, L, W, H);

    const float cx = float(W) * 0.5f;

#if defined(__SWITCH__)
    const float titleScale = 1.28f * L.s;
    const float subScale = 0.74f * L.s;
    const float crumbScale = 0.62f * L.s;
#else
    const float titleScale = 2.2f * L.s;
    const float subScale = 1.3f * L.s;
    const float crumbScale = 1.1f * L.s;
#endif
    font.drawTextCentered(r, cx, L.titleY, titleScale, theme::APP_TITLE, 0.30f, 0.86f, 0.82f, 1.f,
                          W, H);
    {
#if defined(__SWITCH__)
        const float subY = L.titleY + 30.f * L.s;
        const float crumbY = L.titleY + 52.f * L.s;
#else
        const float subY = L.titleY + 24.f * L.s;
        const float crumbY = L.titleY + 46.f * L.s;
#endif
        char sub[80];
#if defined(__SWITCH__)
        std::snprintf(sub, sizeof(sub), "v%s", theme::APP_VERSION);
#else
        std::snprintf(sub, sizeof(sub), "Sand Simulation  |  v%s", theme::APP_VERSION);
#endif
        font.drawTextCentered(r, cx, subY, subScale, sub, 0.58f, 0.63f, 0.69f, 1.f, W, H);

    if (app.menu.screen != MenuScreen::Main) {
        char crumb[96];
        if (app.menu.screen == MenuScreen::SettingsEdit) {
            std::snprintf(crumb, sizeof(crumb), "Main Menu  >  Element Settings  >  %s settings",
                          material_name(app.menu.settingsMat));
        } else if (app.menu.screen == MenuScreen::EngineSettingsTab) {
            std::snprintf(crumb, sizeof(crumb), "Main Menu  >  Engine Settings  >  %s",
                          engineTabLabel(app.menu.engineTab));
        } else {
            std::snprintf(crumb, sizeof(crumb), "Main Menu  >  %s", menuBreadcrumb(app.menu));
        }
        char crumbFit[96];
        fitLabel(crumbFit, sizeof(crumbFit), crumb, L.panelW, crumbScale);
        font.drawTextCentered(r, cx, crumbY, crumbScale, crumbFit, 0.50f, 0.55f, 0.65f, 1.f, W,
                              H);
    }
    }

    char buf[96];

    if (app.menu.screen == MenuScreen::Main) {
        static const char* labels[] = {"New Sandbox", "Demo Sandbox", "Load Game", "Save Game",
                                       "Element Settings", "Engine Settings", "Clear Sandbox",
                                       "Quit"};
        drawVisibleRows(r, font, W, H, L, kMainItemCount, app.menu.index,
                        [&](int i) { return labels[i]; });
    } else if (app.menu.screen == MenuScreen::Load || app.menu.screen == MenuScreen::Save) {
        drawVisibleRows(r, font, W, H, L, kSlotItemCount, app.menu.index, [&](int i) {
            if (i == 3) return static_cast<const char*>("< Back");
            formatSlotLabel(i + 1, buf, sizeof(buf));
            return static_cast<const char*>(buf);
        });
    } else if (app.menu.screen == MenuScreen::Settings) {
        const int total = settingsMaterialCount() + 1;
        drawVisibleRows(r, font, W, H, L, total, app.menu.index, [&](int i) {
            if (i == settingsMaterialCount()) return static_cast<const char*>("< Back");
            return material_name(settingsMaterialAt(i));
        });
    } else if (app.menu.screen == MenuScreen::SettingsEdit) {
        Material m = app.menu.settingsMat;
        const int pc = paramCountFor(m);
        const int total = pc + 2;
        drawVisibleRows(r, font, W, H, L, total, app.menu.index, [&](int row) {
            if (row == 0) return static_cast<const char*>("< Back");
            if (row == pc + 1) return static_cast<const char*>("Reset defaults");
            const ParamSpec* spec = paramSpecAt(m, row - 1);
            if (!spec) return static_cast<const char*>("");
            const float v = getParam(app.physics, m, spec->id);
            char valBuf[16];
            formatParamValue(valBuf, sizeof(valBuf), spec, v);
            std::snprintf(buf, sizeof(buf), "%s: %s", spec->label, valBuf);
            return static_cast<const char*>(buf);
        });
    } else if (app.menu.screen == MenuScreen::EngineSettings) {
        const int total = static_cast<int>(EngineTab::Count) + 1;
        drawVisibleRows(r, font, W, H, L, total, app.menu.index, [&](int i) {
            if (i == static_cast<int>(EngineTab::Count)) return static_cast<const char*>("< Back");
            return engineTabLabel(static_cast<EngineTab>(i));
        });
    } else if (app.menu.screen == MenuScreen::EngineSettingsTab) {
        const int rows = engineTabRowCount(app.menu.engineTab);
        char rowBuf[96];
        drawVisibleRows(r, font, W, H, L, rows + 1, app.menu.index, [&](int i) {
            if (i == 0) return static_cast<const char*>("< Back");
            engineTabRowLabel(app.menu.engineTab, i - 1, app.settings, rowBuf, sizeof(rowBuf));
            return static_cast<const char*>(rowBuf);
        });
    }

    font.drawTextCentered(r, cx, L.footerY, 0.90f * L.s,
                          navHintFor(app.menu.screen, app.hasEnteredPlay), 0.45f, 0.50f, 0.58f, 0.85f,
                          W, H);
}

} // namespace nx
