#pragma once
#include <SDL2/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include "gpu/render_pipeline.hpp"
#include "gpu/sim_pipeline.hpp"
#include "platform/input/input.hpp"
#include "platform/input/menu_repeat.hpp"
#include "sim/physics_params.hpp"
#include "sim/sim_state.hpp"
#include "ui/menu.hpp"
#include "ui/menu_sim.hpp"
#include "ui/save_overlay.hpp"
#include "ui/toast.hpp"
#include "gpu/font_atlas.hpp"
#include "gpu/perf_stats.hpp"
#include "game_settings.hpp"

namespace nx {

enum class Scene { Menu, Play };

enum class PendingSaveKind {
    None,
    Slot,
    GameSettings,
    PhysicsSettings,
};

class App {
public:
    static constexpr int kPlaySaveSuppressFrames = 30;

    SDL_Window* window = nullptr;
    SDL_GLContext glCtx = nullptr;
    int screenW = 1280;
    int screenH = 720;
    int framebufferW = 1280;
    int framebufferH = 720;

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
    SaveOverlay saveOverlay{};

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
    void applyRuntimeSettingsLight();
    void applyRuntimeSettingsHeavy();
    void flushPendingHeavySettings();
    void schedulePendingHeavySettingsFlush();
    void markSettingsHeavyApplyPending() { settingsHeavyApplyPending_ = true; }

    bool computeSimSupported() const { return computeSimSupported_; }
    bool forceComputeBackend() const { return forceComputeBackend_; }

    void onEnterPlayFromMenu();
    bool ensureSimPipelineReady();
    void requestSlotSave(int slot, bool fromQuickSave = false);
    void requestFlushGameSettings();
    void requestFlushPhysicsSettings();

    void presentBootProgress(float progress, const char* status);
    void presentCompileOverlay(float progress);
    void syncScreenMetrics();
    void ensureUiFontReady();
    void resetMenuRepeat() { menuRepeat_.reset(); }
    void releaseMemoryBeforeSimCompile();
    void reloadAudioAfterSimCompile();
    void ensureAudioReady();

private:
    bool computeSimSupported_ = false;
    bool forceComputeBackend_ = false;
    SimBackend resolveSimBackend() const;
    bool initSimPipeline(int w, int h);
    bool resetGlContextForSimCompile();
#if defined(__SWITCH__)
    bool prepareLightSimCompileOnSwitch();
#endif
    bool restoreUiPipelinesAfterSimCompile();
    void tickMenu(double dtSec);
    void tickPlay(double dtSec);
    void renderFrame();
    std::string resolveShaderDir() const;
    void tickPendingSave(double dtSec);
    void tickPendingHeavySettings();
    void executePendingSave(PendingSaveKind kind);
    void enqueuePendingSave(PendingSaveKind kind);
    void beginSaveOverlayFor(PendingSaveKind kind);

    static constexpr size_t kPendingSaveQueueCap = 4;

    int lastScreenW_ = 0;
    int lastScreenH_ = 0;
    int playSaveSuppressFrames_ = 0;

    std::array<PendingSaveKind, kPendingSaveQueueCap> pendingSaveQueue_{};
    size_t pendingSaveCount_ = 0;
    double pendingSaveDelaySec_ = 0.0;
    int pendingSlot_ = 1;
    bool settingsHeavyApplyPending_ = false;
    bool heavyFlushScheduled_ = false;
    bool simWarmupTriggered_ = false;
    bool audioReady_ = false;
    bool screenMetricsLogged_ = false;
    bool playEntryLogged_ = false;
    bool playTickLogged_ = false;
    bool playRenderLogged_ = false;
    bool simStartupFailed_ = false;

    bool playStepLogged_ = false;

    PerfStats perf_{};
    MenuRepeatState menuRepeat_{};
};

} // namespace nx
