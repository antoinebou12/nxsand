#pragma once
#include <SDL2/SDL.h>
#include <memory>
#include <string>
#include "gpu/render_pipeline.hpp"
#include "gpu/sim_pipeline.hpp"
#include "platform/input/input.hpp"
#include "sim/physics_params.hpp"
#include "sim/sim_state.hpp"
#include "ui/menu.hpp"
#include "ui/menu_sim.hpp"
#include "ui/toast.hpp"
#include "gpu/font_atlas.hpp"
#include "gpu/perf_stats.hpp"
#include "game_settings.hpp"

namespace nx {

enum class Scene { Menu, Play };

class App {
public:
    SDL_Window* window = nullptr;
    SDL_GLContext glCtx = nullptr;
    int screenW = 1280;
    int screenH = 720;

    Scene scene = Scene::Menu;
    /// True after the first transition to Play (New/Load/resume). Main-menu B/Esc only resumes when set.
    bool hasEnteredPlay = false;
    SimState sim{};
    PhysicsParams physics{};
    GameSettings settings{};
    InputState input{};
    MenuState menu{};
    MenuSim   menuSim{};
    Toast toast{};

    std::unique_ptr<SimPipeline> simPipeline;
    std::unique_ptr<RenderPipeline> render;
    FontAtlas font;

    std::string shaderDir = "shaders";
    std::string initError;

    bool init();
    void shutdown();
    void frame(double dtSec);
    bool setSimGridSize(int w, int h, bool preserveContent);
    void applyRuntimeSettings();

private:
    void tickMenu(double dtSec);
    void tickPlay(double dtSec);
    void renderFrame();
    std::string resolveShaderDir() const;

    int lastScreenW_ = 0;
    int lastScreenH_ = 0;

    PerfStats perf_{};
};

} // namespace nx
