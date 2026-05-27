#pragma once
#include "../sim/materials.hpp"
#include "../game/engine_settings.hpp"

namespace nx {

class App;
class RenderPipeline;
class FontAtlas;

enum class MenuScreen {
    Main,
    Load,
    Save,
    Settings,
    SettingsEdit,
    EngineSettings,
    EngineSettingsTab,
};

struct MenuState {
    MenuScreen screen = MenuScreen::Main;
    int index = 0;
    int tick = 0;
    Material settingsMat = MAT_FIRE;
    int settingsParamRow = 0;

    EngineTab engineTab = EngineTab::Performance;
    bool materialWheelOpen = false;
    int materialWheelIndex = 0;

    void resetMain();
    void goBack(App& app);
    void moveVertical(int delta);
    void handleConfirm(App& app);
    void adjustHorizontal(App& app, int dir);
};

void drawMenuSolid(RenderPipeline& r, FontAtlas& font, App& app);

} // namespace nx
