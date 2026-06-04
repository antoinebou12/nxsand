#include "menu.hpp"
#include "../game/app.hpp"
#include "../game/benchmark_scene.hpp"
#include "../game/engine_settings.hpp"
#include "../gpu/font_atlas.hpp"
#include "../gpu/render_pipeline.hpp"
#include "../save/save.hpp"
#include "../save/physics_params_io.hpp"
#include "../sim/physics_params.hpp"
#include "../sim/physics_settings.hpp"
#if defined(__SWITCH__)
#include "../platform/launch_log.hpp"
#endif
#include "../ui/menu_fx.hpp"
#include "../ui/theme.hpp"
#include "menu_sim.hpp"
#include "menu_chrome.hpp"
#include "ui_copy.hpp"
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

bool menuAllowsHorizontalRepeat(const MenuState& m) {
    if (m.screen == MenuScreen::SettingsEdit) {
        const int pc = paramCountFor(m.settingsMat);
        if (m.index <= 0 || m.index > pc) return false;
        const ParamSpec* spec = paramSpecAt(m.settingsMat, m.index - 1);
        return spec && spec->step < 1.f;
    }
    if (m.screen == MenuScreen::EngineSettingsTab) {
        if (m.index <= 0) return false;
        return !engineTabRowIsToggle(m.engineTab, m.index - 1);
    }
    return false;
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

static void resetMenuRepeatOnNav(App& app) {
    app.resetMenuRepeat();
}

void MenuState::goBack(App& app) {
    resetMenuRepeatOnNav(app);
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
        app.requestFlushGameSettings();
    }
    if (screen == MenuScreen::Settings) {
        app.requestFlushPhysicsSettings();
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
        resetMenuRepeatOnNav(app);
        if (screen == MenuScreen::SettingsEdit) app.requestFlushPhysicsSettings();
        if (screen == MenuScreen::EngineSettingsTab) {
            app.schedulePendingHeavySettingsFlush();
            app.requestFlushGameSettings();
        }
        goBack(app);
        return;
    }

    if (screen == MenuScreen::Main) {
        switch (index) {
            case 0:
                resetMenuRepeatOnNav(app);
                app.flushPendingHeavySettings();
                if (!app.ensureSimPipelineReady()) {
                    app.toast.show("Simulation failed to start", 2.5f);
                    break;
                }
                app.simPipeline->clearAll(MAT_EMPTY);
                app.sim.gridHasMatter = false;
                app.sim.sleeping = true;
                app.hasEnteredPlay = true;
                app.scene = Scene::Play;
                app.sim.tick = 0;
                app.onEnterPlayFromMenu();
                app.toast.show("New empty sandbox", 1.0f);
                break;
            case 1:
                resetMenuRepeatOnNav(app);
                app.flushPendingHeavySettings();
                if (!app.ensureSimPipelineReady()) {
                    app.toast.show("Simulation failed to start", 2.5f);
                    break;
                }
                seedStarterScene(app);
                app.hasEnteredPlay = true;
                app.scene = Scene::Play;
                app.sim.tick = 0;
                app.onEnterPlayFromMenu();
                break;
            case 2:
                resetMenuRepeatOnNav(app);
                screen = MenuScreen::Load;
                index = 0;
                break;
            case 3:
                resetMenuRepeatOnNav(app);
                screen = MenuScreen::Save;
                index = 0;
                break;
            case 4:
                resetMenuRepeatOnNav(app);
                screen = MenuScreen::Settings;
                index = 0;
                break;
            case 5:
                resetMenuRepeatOnNav(app);
                screen = MenuScreen::EngineSettings;
                index = 0;
                break;
            case 6:
                if (!app.ensureSimPipelineReady()) {
                    app.toast.show("Simulation failed to start", 2.5f);
                    break;
                }
                app.simPipeline->clearAll(MAT_EMPTY);
                app.sim.gridHasMatter = false;
                app.sim.sleeping = true;
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
        } else {
            app.flushPendingHeavySettings();
            if (!app.ensureSimPipelineReady()) {
                app.toast.show("Simulation failed to start", 2.5f);
            } else if (loadGame(app, index + 1)) {
                app.hasEnteredPlay = true;
                app.scene = Scene::Play;
                app.onEnterPlayFromMenu();
                app.toast.show("Loaded", 1.2f);
            } else {
                app.toast.show("Load failed", 1.5f);
            }
        }
    } else if (screen == MenuScreen::Save) {
        app.requestSlotSave(index + 1);
    } else if (screen == MenuScreen::Settings) {
        resetMenuRepeatOnNav(app);
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
        resetMenuRepeatOnNav(app);
        engineTab = static_cast<EngineTab>(index);
        screen = MenuScreen::EngineSettingsTab;
        index = 1;
    } else if (screen == MenuScreen::EngineSettingsTab) {
        if (index > 0 && engineTab == EngineTab::Debug &&
            index - 1 == engineTabRowCount(EngineTab::Debug) - 1) {
            if (app.ensureSimPipelineReady()) {
                seedBenchmarkScene(app, app.settings.debug.benchmarkScene);
            }
        }
    }
}

void MenuState::adjustHorizontal(App& app, int dir) {
    if (screen == MenuScreen::EngineSettingsTab && index > 0) {
        adjustEngineTabRow(app, engineTab, index - 1, dir);
        return;
    }
    if (screen != MenuScreen::SettingsEdit) return;
    const int rows = paramCountFor(settingsMat) + 2;
    if (index <= 0 || index >= rows - 1) return;
    const ParamSpec* spec = paramSpecAt(settingsMat, index - 1);
    if (!spec) return;
    adjustParam(app.physics, settingsMat, spec->id, dir);
    markPhysicsParamsDirty();
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

static MenuLayout computeLayout(int W, int H, int itemCount, int selectedIndex,
                                bool hasBreadcrumb, bool mainMarkHeader,
                                const FontAtlas& font, float accessibilityScale);

static float mainMenuMarkY(const MenuLayout& L, const FontAtlas& font) {
    const float panelInset = 4.f * L.s;
    const float headerTop = L.panelY + panelInset;
    const float markH = menuMarkBoxHeight(font, L.s);
    const float markBand = menuMarkBandHeight(font, L.s);
    return headerTop + (markBand - markH) * 0.5f;
}

static MenuLayout computeLayout(int W, int H, int itemCount, int selectedIndex,
                                bool hasBreadcrumb, bool mainMarkHeader, const FontAtlas& font,
                                float accessibilityScale) {
    MenuLayout L{};
    L.s = theme::uiScale(W, H, accessibilityScale);
    const bool portrait = W < H;
    (void)portrait;
    const float sideMargin = 42.f * L.s;
    const float safeBottom = 64.f * L.s;
    const float safeTop = 22.f * L.s;
    L.panelW = std::min(590.f * L.s, std::max(220.f * L.s, float(W) - sideMargin * 2.f));
    const float panelInset = 4.f * L.s;
    const float headerGap = 12.f * L.s;
    const float ruleMargin = 10.f * L.s;
    float listPadTop = 46.f * L.s;
    if (hasBreadcrumb) {
        const float crumbBand = menuCrumbBandHeight(font, L.s);
        const float crumbScale = menuCrumbTextScale(L.s);
        const float linePx = float(font.lineH) * crumbScale;
        L.panelY = safeTop + 20.f * L.s;
        L.markY = L.panelY;
        L.titleY = L.panelY + panelInset + (crumbBand - linePx) * 0.5f;
        listPadTop = panelInset + crumbBand + headerGap + ruleMargin;
    } else if (mainMarkHeader) {
        const float markBand = menuMarkBandHeight(font, L.s);
        L.panelY = safeTop + 18.f * L.s;
        L.markY = L.panelY;
        L.titleY = L.panelY;
        listPadTop = panelInset + markBand + headerGap;
    } else {
        L.panelY = safeTop + 20.f * L.s;
        L.markY = L.panelY;
        L.titleY = L.panelY;
    }
    L.footerY = float(H) - safeBottom * 0.74f;
    const float listPadBottom = 30.f * L.s;
    const float panelChrome = listPadTop + listPadBottom;
    const float available = std::max(160.f * L.s, L.footerY - L.panelY - 32.f * L.s);
    L.rowH = std::clamp(46.f * L.s, 38.f * L.s, 54.f * L.s);
    const MenuListWindow win =
        computeMenuListWindow(itemCount, selectedIndex, available - panelChrome, L.rowH);
    L.visibleRows = win.visibleRows;
    L.firstRow = win.firstRow;
    L.panelH = L.rowH * float(L.visibleRows) + panelChrome;
    L.panelX = (float(W) - L.panelW) * 0.5f;
    L.listY0 = L.panelY + listPadTop;
    return L;
}

void MenuState::handlePointer(App& app, int px, int py, bool confirm) {
    const int total = menuItemCount(*this);
    if (total <= 0) return;
    const bool hasBreadcrumb = screen != MenuScreen::Main;
    const bool mainMarkHeader = !hasBreadcrumb && screen == MenuScreen::Main;
    const MenuLayout L =
        computeLayout(app.screenW, app.screenH, total, index, hasBreadcrumb, mainMarkHeader,
                      app.font, app.settings.accessibility.uiScale);
    const float padX = 28.f * L.s;
    const float x0 = L.panelX + padX;
    const float x1 = L.panelX + L.panelW - padX;
    if (float(px) < x0 || float(px) > x1) return;
    for (int row = 0; row < L.visibleRows; ++row) {
        const int i = L.firstRow + row;
        if (i < 0 || i >= total) continue;
        const float y0 = L.listY0 + float(row) * L.rowH;
        const float y1 = y0 + L.rowH;
        if (float(py) >= y0 && float(py) < y1) {
            index = i;
            if (confirm) handleConfirm(app);
            return;
        }
    }
}

static void drawSpaceGradient(RenderPipeline& rp, int W, int H) {
    const int bands = 5;
    const float bh = float(H) / float(bands);
    for (int i = 0; i < bands; ++i) {
        const float t = float(i) / float(bands - 1);
        const float u = std::fabs(t - 0.5f) * 2.f;
        const float rv = 0.060f + u * 0.010f;
        const float gv = 0.052f + u * 0.008f;
        const float bv = 0.105f + u * 0.018f;
        rp.drawSolidRect(0.f, bh * float(i), float(W), bh + 1.f, rv, gv, bv, 1.f, W, H);
    }
}

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

template <typename Fn>
static void drawVisibleRows(RenderPipeline& r, FontAtlas& font, int W, int H, const MenuLayout& L,
                            int totalRows, int selected, Fn labelFor) {
    for (int row = 0; row < L.visibleRows; ++row) {
        const int i = L.firstRow + row;
        if (i < 0 || i >= totalRows) continue;
        const float y = L.listY0 + float(row) * L.rowH;
        drawMenuRow(r, font, W, H, L, y, labelFor(i), i == selected);
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

void drawMenuSolid(RenderPipeline& r, FontAtlas& font, App& app) {
    const int W = app.screenW, H = app.screenH;
    const int items = menuItemCount(app.menu);
    const bool hasBreadcrumb = app.menu.screen != MenuScreen::Main;
    const bool mainMarkHeader =
        !hasBreadcrumb && app.menu.screen == MenuScreen::Main;
    const MenuLayout L = computeLayout(W, H, items, app.menu.index, hasBreadcrumb, mainMarkHeader,
                                       font, app.settings.accessibility.uiScale);
#if defined(__SWITCH__)
    static bool menuLayoutLogged = false;
    if (!menuLayoutLogged) {
        menuLayoutLogged = true;
        appendLaunchLogf("menu draw: screen=%dx%d items=%d index=%d scale=%.2f rowH=%.1f visible=%d first=%d lineH=%d",
                         W, H, items, app.menu.index, L.s, L.rowH, L.visibleRows, L.firstRow,
                         font.lineH);
    }
#endif
    const int tick = app.menu.tick;
    const bool mainMenuFlow = app.menu.screen == MenuScreen::Main || app.menu.screen == MenuScreen::Load ||
                              app.menu.screen == MenuScreen::Save;
    const bool showBackdrop =
        app.settings.display.menuChrome == MenuChrome::Full && mainMenuFlow;

    drawSpaceGradient(r, W, H);
    const bool showMainMark =
        app.menu.screen == MenuScreen::Main && mainMenuFlow;
    if (showBackdrop) {
        app.menuSim.draw(r, W, H, L.s);
        drawMenuBackgroundFx(r, W, H, tick);
    } else {
        drawMenuBackgroundFx(r, W, H, tick);
    }

    const float scrimTop = L.panelY - 12.f * L.s;
    const float scrimBottom =
        std::max(L.panelY + L.panelH + 14.f * L.s, L.footerY + 32.f * L.s);
    const float chromeStrength = mainMenuFlow ? 0.38f : 1.f;
    drawMenuChromeScrim(r, L, W, H, scrimTop, scrimBottom, chromeStrength);
    drawMenuGlassPanel(r, L, W, H, chromeStrength);

    const float cx = float(W) * 0.5f;

    if (showMainMark) {
        drawMenuMark(r, font, cx, mainMenuMarkY(L, font), L, W, H, chromeStrength);
        drawMenuHeaderRuleAt(r, L, L.listY0 - 10.f * L.s, W, H);
    }

    if (hasBreadcrumb) {
        const float crumbScale = menuCrumbTextScale(L.s);
        const float crumbBand = menuCrumbBandHeight(font, L.s);
        const float headerTop = L.panelY + 4.f * L.s;
        const float crumbY = menuBaselineInBand(headerTop, crumbBand, font, crumbScale);
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
        fitLabel(crumbFit, sizeof(crumbFit), crumb, L.panelW - 56.f * L.s, crumbScale);
        font.drawTextCentered(r, cx, crumbY, crumbScale, crumbFit, 0.72f, 0.78f, 0.88f, 1.f, W,
                              H);
        drawMenuHeaderRuleAt(r, L, L.listY0 - 10.f * L.s, W, H);
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

    drawHintPill(r, font, cx, L.footerY, L.s,
                 ui_copy::menuNavHint(app.menu.screen, app.hasEnteredPlay), W, H,
                 mainMenuFlow ? chromeStrength : 1.f);
}

} // namespace nx
