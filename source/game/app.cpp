#include "app.hpp"
#include "../gpu/gl_loader.hpp"
#include "../gpu/sim_backend.hpp"
#include "engine_settings.hpp"
#include "game_settings.hpp"
#include "gpu/gl_loader.hpp"
#include "gpu/shader_program.hpp"
#include "gpu/shader_cache.hpp"
#include "gpu/frame_graph.hpp"
#include "save/save.hpp"
#include "save/save_paths.hpp"
#include "save/physics_params_io.hpp"
#include "save/settings_io.hpp"
#include "ui/hud.hpp"
#include "ui/layout.hpp"
#include "ui/menu_fx.hpp"
#include "ui/sim_fx.hpp"
#include "ui/perf_overlay.hpp"
#include "ui/boot_screen.hpp"
#include "platform/screen_size.hpp"
#include "ui/brush_cursor.hpp"
#include "ui/material_wheel.hpp"
#include "ui/active_tiles_overlay.hpp"
#include "sim/brush_stroke.hpp"
#include "sim/materials.hpp"
#include "ui/theme.hpp"
#include "sim/sim_state.hpp"
#include "platform/input/haptics.hpp"
#include "platform/audio/tone_audio.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>
#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <vector>

namespace nx {

namespace {

#if defined(__SWITCH__)
void appendLaunchLog(const char* msg) {
    FILE* f = std::fopen("sdmc:/switch/nxsand/launch.log", "a");
    if (!f) return;
    std::fprintf(f, "%s\n", msg ? msg : "(null)");
    std::fclose(f);
}

void appendLaunchLogf(const char* fmt, ...) {
    char buf[768];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    appendLaunchLog(buf);
}
#endif

void applySdlOrientationHint(ScreenOrientation o) {
#if !defined(__SWITCH__)
    const char* v = "";
    switch (o) {
        case ScreenOrientation::Portrait:
            v = "Portrait PortraitUpsideDown";
            break;
        case ScreenOrientation::Landscape:
            v = "LandscapeLeft LandscapeRight";
            break;
        case ScreenOrientation::Auto:
        default:
            v = "";
            break;
    }
    SDL_SetHintWithPriority(SDL_HINT_ORIENTATIONS, v, SDL_HINT_OVERRIDE);
#else
    (void)o;
#endif
}

} // namespace

static std::string getenvStr(const char* k) {
    const char* v = SDL_getenv(k);
    return v ? std::string(v) : std::string{};
}

static bool getenvEnabled(const char* k) {
    std::string v = getenvStr(k);
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return !v.empty() && v != "0" && v != "false" && v != "off" && v != "no";
}

static void bootLogStage(const char* stage) {
    if (!getenvEnabled("NXSAND_BOOT_LOG") && !getenvEnabled("NXENGINE_BOOT_LOG")) return;
    static uint32_t t0 = SDL_GetTicks();
    std::cerr << "[boot +" << (SDL_GetTicks() - t0) << "ms] " << stage << "\n";
}

std::string App::resolveShaderDir() const {
    std::string e = getenvStr("NXSAND_SHADER_DIR");
    if (!e.empty()) return e;
    e = getenvStr("NXENGINE_SHADER_DIR");
    if (!e.empty()) return e;
#if defined(__SWITCH__)
    // main() chdir("romfs:/"); paths are relative to embedded romfs.
    return std::string("shaders");
#else
    return shaderDir;
#endif
}

bool App::init() {
    initError.clear();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0) {
        initError = std::string("SDL_Init: ") + SDL_GetError();
        std::cerr << initError << "\n";
        return false;
    }

    loadGameSettings(settings);
    forceComputeBackend_ =
#if defined(NXSAND_ENABLE_COMPUTE_DEFAULT) && NXSAND_ENABLE_COMPUTE_DEFAULT
        true ||
#endif
        getenvEnabled("NXSAND_ENABLE_COMPUTE") || getenvEnabled("NXENGINE_ENABLE_COMPUTE");
#if defined(__SWITCH__)
    settings.display.orientation = ScreenOrientation::Landscape;
#endif
    applySdlOrientationHint(settings.display.orientation);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    // GLES 3.1 for optional compute sim (Switch + desktop); context creation falls back to 3.0.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    Uint32 wflags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
#if defined(__SWITCH__)
    SDL_DisplayMode displayMode{};
    if (SDL_GetCurrentDisplayMode(0, &displayMode) == 0 && displayMode.w > 0 && displayMode.h > 0) {
        screenW = displayMode.w;
        screenH = displayMode.h;
        applyDrawableOrientation(screenW, screenH, settings.display.orientation);
    } else {
        screenW = RENDER_W;
        screenH = RENDER_H;
    }
    wflags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#else
    wflags |= SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
#endif
    window = SDL_CreateWindow(theme::APP_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              screenW, screenH, wflags);
    if (!window) {
        initError = std::string("SDL_CreateWindow: ") + SDL_GetError();
        std::cerr << initError << "\n";
        return false;
    }

    glCtx = SDL_GL_CreateContext(window);
    if (!glCtx) {
#if defined(__SWITCH__)
        appendLaunchLogf("SDL_GL_CreateContext ES 3.1 failed: %s", SDL_GetError());
#endif
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        glCtx = SDL_GL_CreateContext(window);
    }
#if defined(NX_DESKTOP) && defined(_WIN32)
    if (!glCtx) {
        SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        glCtx = SDL_GL_CreateContext(window);
        if (!glCtx) {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            glCtx = SDL_GL_CreateContext(window);
        }
    }
#endif
    if (!glCtx) {
        initError = std::string("SDL_GL_CreateContext: ") + SDL_GetError();
        std::cerr << initError << "\n";
        return false;
    }
    if (SDL_GL_MakeCurrent(window, glCtx) != 0) {
        initError = std::string("SDL_GL_MakeCurrent: ") + SDL_GetError();
        std::cerr << initError << "\n";
        return false;
    }
    SDL_GL_SetSwapInterval(1);

    queryDrawableSize(window, screenW, screenH, settings.display.orientation);
    {
        const auto simSz = resolveSimGridSize(screenW, screenH, settings.performance);
        sim.grid_w = simSz.first;
        sim.grid_h = simSz.second;
    }
    sim.brush_x = sim.grid_w / 2;
    sim.brush_y = sim.grid_h / 2;
    sim.prev_brush_x = sim.brush_x;
    sim.prev_brush_y = sim.brush_y;
    sim.brush_radius = settings.controls.brushRadius;
    lastScreenW_ = screenW;
    lastScreenH_ = screenH;

    if (!gl::load_gl_functions()) {
        initError = "OpenGL ES function loader failed (GLAD)";
        std::cerr << initError << "\n";
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenW, screenH);
    glClearColor(0.03f, 0.04f, 0.06f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(window);
    bootLogStage("first clear");

    shaderDir = resolveShaderDir();

    const char* glVer = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glSl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
#if defined(__SWITCH__)
    appendLaunchLogf("GL_VERSION=%s", glVer ? glVer : "?");
    appendLaunchLogf("GLSL_VERSION=%s", glSl ? glSl : "?");
#endif
#if !defined(NDEBUG)
    std::cerr << "GL_VERSION=" << (glVer ? glVer : "?")
              << " GLSL=" << (glSl ? glSl : "?") << "\n";
#endif

    {
        std::string computeErr;
        computeSimSupported_ = gl::check_compute_support(&computeErr);
        if (!forceComputeBackend_ && !computeSimSupported_ &&
            settings.performance.simBackend == SimBackend::Compute) {
            settings.performance.simBackend = SimBackend::Fragment;
            markGameSettingsDirty();
        }
#if defined(__SWITCH__)
        appendLaunchLogf("Compute sim: %s", computeSimSupported_ ? "yes" : computeErr.c_str());
#endif
#if !defined(NDEBUG)
        if (!computeSimSupported_ && !computeErr.empty()) {
            std::cerr << "Compute sim unavailable: " << computeErr << "\n";
        }
#endif
    }

    bootLogStage("render pipeline");
    render = std::make_unique<RenderPipeline>();
    if (!render->init(shaderDir)) {
        initError = "Render pipeline failed";
        const char* diag = lastShaderDiagnostics();
        if (diag && diag[0]) {
            initError += ": ";
            initError += diag;
        }
#if defined(__SWITCH__)
        appendLaunchLogf("RenderPipeline init failed: %s", initError.c_str());
#endif
        return false;
    }
    flushPendingShaderCacheSaves();
    {
        const char* palMode = SDL_getenv("NXSAND_PALETTE_MODE");
        if (!palMode) palMode = SDL_getenv("NXENGINE_PALETTE_MODE");
        if (palMode) {
            const int m = std::atoi(palMode);
            if (m >= 0 && m <= 2) render->setPaletteMode(m);
        }
    }

    bootLogStage("font atlas");
    if (!font.init()) {
        initError = "Font atlas init failed";
#if defined(__SWITCH__)
        appendLaunchLog(initError.c_str());
#endif
        return false;
    }
    presentBootProgress(0.12f, "Loading fonts...");

    bootLogStage("menu backdrop");
    if (!menuSim.init()) {
        initError = "Menu backdrop init failed";
#if defined(__SWITCH__)
        appendLaunchLog(initError.c_str());
#endif
        return false;
    }
    presentBootProgress(0.35f, "Loading menu...");

    simPipeline = std::make_unique<SimPipeline>();
    bootLogStage("sim pipeline deferred");

    loadPhysicsParams(physics);
    applyRuntimeSettings();
    SDL_GL_SetSwapInterval(settings.performance.targetFps == 60 ? 1 : 2);

#if defined(__SWITCH__)
    if (!ensureSwitchStorageReady()) {
        toast.show("SD saves unavailable (check microSD)", 4.0f);
    }
    presentBootProgress(0.92f, "Almost ready...");
#endif

    openFirstController(input);
    menu.resetMain();
    toneAudioSetLevel(settings.controls.sound);
    presentBootProgress(1.f, "Ready");
    bootLogStage("init complete");
    return true;
}

void App::presentBootProgress(float progress, const char* status) {
    if (!window || !glCtx || !render) return;
    render->beginUiFrame();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenW, screenH);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.03f, 0.04f, 0.06f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (font.tex != 0) {
        drawBootScreen(*render, font, screenW, screenH, progress, status);
    }
    render->endUiFrame();
    SDL_GL_SwapWindow(window);
}

struct SimCompileAudioGuard {
    SimCompileAudioGuard() { toneAudioSetOutputPaused(true); }
    ~SimCompileAudioGuard() { toneAudioSetOutputPaused(false); }
};

static void shaderCompileProgress(const char* stage, uint64_t elapsedMs, void* user) {
    auto* app = static_cast<App*>(user);
    if (!app || !stage) return;
    char buf[160];
    const unsigned long long sec = static_cast<unsigned long long>(elapsedMs / 1000u);
    std::snprintf(buf, sizeof(buf), "%s (%llus)", stage, sec);
    const float t = std::min(1.f, static_cast<float>(elapsedMs) / 90000.f);
    SDL_PumpEvents();
    app->presentBootProgress(0.5f + 0.2f * t, buf);
}

bool App::ensureSimPipelineReady() {
    if (!simPipeline) simPipeline = std::make_unique<SimPipeline>();
    if (simPipeline->ready()) return true;

    const SimCompileAudioGuard audioGuard;
    presentBootProgress(0.5f, "Compiling simulation...");
    glFinish();
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!resetGlContextForSimCompile()) {
        toast.show("Simulation failed to start", 2.5f);
        return false;
    }
    bootLogStage("sim pipeline compile");
    const auto compileStart = std::chrono::steady_clock::now();
    const bool showCompileProgress =
        getenvEnabled("NXSAND_SHADER_PROGRESS") || getenvEnabled("NXENGINE_SHADER_PROGRESS");
    if (showCompileProgress) setShaderCompileProgress(shaderCompileProgress, this);
    bool ok = initSimPipeline(sim.grid_w, sim.grid_h);
    flushPendingShaderCacheSaves();
    if (showCompileProgress) setShaderCompileProgress(nullptr, nullptr);
    if (!restoreUiPipelinesAfterSimCompile()) ok = false;
    if (!ok) {
        clearPendingShaderCacheSaves();
        const char* diag = lastShaderDiagnostics();
        if (diag && std::strstr(diag, "timed out")) {
            toast.show(
                "Shader compile timed out. Delete nxsand_save/shader_cache/ or set "
                "NXSAND_SHADER_CACHE=0.",
                4.f);
        } else {
            toast.show("Simulation failed to start", 2.5f);
        }
        if (scene == Scene::Menu) {
            presentBootProgress(1.f, "Ready");
        }
        return false;
    }
    if (getenvEnabled("NXSAND_BOOT_LOG") || getenvEnabled("NXENGINE_BOOT_LOG")) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - compileStart)
                            .count();
        std::cerr << "[boot] sim pipeline compile total " << ms << "ms\n";
    }
    if (SDL_getenv("NXSAND_DEBUG_GRID") || SDL_getenv("NXENGINE_DEBUG_GRID")) {
        const int mid = sim.grid_h / 2;
        for (int x = 0; x < sim.grid_w; x += 8) {
            simPipeline->paintDisk(x, mid, 1, MAT_SAND, nullptr, nullptr);
        }
        simPipeline->syncSimForSampling();
    }
    presentBootProgress(0.75f, "Starting simulation...");
    bootLogStage("sim pipeline ready");
    return true;
}

bool App::resetGlContextForSimCompile() {
    bootLogStage("gl context reset for sim compile");
    clearPendingShaderCacheSaves();
    menuSim.shutdown();
    font.shutdown();
    if (render) render->shutdown();
    if (simPipeline) simPipeline->shutdown();

    if (glCtx) {
        SDL_GL_DeleteContext(glCtx);
        glCtx = nullptr;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    glCtx = SDL_GL_CreateContext(window);
    if (!glCtx) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        glCtx = SDL_GL_CreateContext(window);
    }
#if defined(NX_DESKTOP) && defined(_WIN32)
    if (!glCtx) {
        SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        glCtx = SDL_GL_CreateContext(window);
        if (!glCtx) {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            glCtx = SDL_GL_CreateContext(window);
        }
    }
#endif
    if (!glCtx) {
        initError = std::string("SDL_GL_CreateContext: ") + SDL_GetError();
        return false;
    }
    if (SDL_GL_MakeCurrent(window, glCtx) != 0) {
        initError = std::string("SDL_GL_MakeCurrent: ") + SDL_GetError();
        return false;
    }
    if (!gl::load_gl_functions()) {
        initError = "OpenGL ES function loader failed (GLAD)";
        return false;
    }
    SDL_GL_SetSwapInterval(settings.performance.targetFps == 60 ? 1 : 2);
    queryDrawableSize(window, screenW, screenH, settings.display.orientation);
    simPipeline = std::make_unique<SimPipeline>();
    return true;
}

bool App::restoreUiPipelinesAfterSimCompile() {
    bootLogStage("render pipeline restore after sim compile");
    if (!render) render = std::make_unique<RenderPipeline>();
    if (!render->init(shaderDir)) {
        initError = "Render pipeline restore failed";
        return false;
    }
    flushPendingShaderCacheSaves();
    if (!font.init()) {
        initError = "Font atlas restore failed";
        return false;
    }
    if (!menuSim.init()) {
        initError = "Menu backdrop restore failed";
        return false;
    }
    applyRuntimeSettingsLight();
    return true;
}

SimBackend App::resolveSimBackend() const {
    if (!computeSimSupported_) return SimBackend::Fragment;
    if (forceComputeBackend_) return SimBackend::Compute;
    return settings.performance.simBackend;
}

bool App::initSimPipeline(int w, int h) {
    const SimBackend backend = resolveSimBackend();
    if (simPipeline->init(w, h, shaderDir, backend)) return true;
    if (backend != SimBackend::Compute) return false;
    computeSimSupported_ = false;
    if (!forceComputeBackend_ && settings.performance.simBackend == SimBackend::Compute) {
        settings.performance.simBackend = SimBackend::Fragment;
        markGameSettingsDirty();
    }
    setShaderDiagnostics("");
    if (simPipeline->init(w, h, shaderDir, SimBackend::Fragment)) {
        toast.show("Compute unavailable - using Fragment", 2.5f);
        return true;
    }
    return false;
}

void App::applyRuntimeSettingsLight() {
#if defined(__SWITCH__)
    settings.display.orientation = ScreenOrientation::Landscape;
#endif
    if (!forceComputeBackend_ && !computeSimSupported_ &&
        settings.performance.simBackend == SimBackend::Compute) {
        settings.performance.simBackend = SimBackend::Fragment;
        markGameSettingsDirty();
    }
    applySdlOrientationHint(settings.display.orientation);
    sim.brush_radius = std::clamp(settings.controls.brushRadius, 1, 64);
    toneAudioSetLevel(settings.controls.sound);
    SDL_GL_SetSwapInterval(settings.performance.targetFps == 60 ? 1 : 2);
    if (render) {
        render->setPaletteMode(settings.debug.showMaterialIds ? 3 : settings.visuals.paletteMode);
        render->setBlobEnabled(settings.visuals.paletteMode == 0);
        render->setBloomLevel(settings.visuals.bloom);
        const bool reduceFlash = settings.accessibility.reduceFlashing;
        render->setFlickerEnabled(settings.visuals.flicker && !reduceFlash);
        render->setGrainEnabled(settings.visuals.grain);
        float ao = 0.f;
        if (settings.visuals.ao == VisualAo::Low) ao = 0.025f;
        else if (settings.visuals.ao == VisualAo::High) ao = 0.045f;
        render->setAoStrength(ao);
        render->setUpscaleFilter(settings.visuals.upscaleFilter);
    }
    perf_.presetLabel = perfPresetLabel(settings.performance.mode);
}

void App::applyRuntimeSettingsHeavy() {
    const auto sz = resolveSimGridSize(screenW, screenH, settings.performance);
    const bool needResize =
        sz.first != sim.grid_w || sz.second != sim.grid_h;
    const bool needBackend =
        simPipeline && simPipeline->ready() &&
        simPipeline->backend() != resolveSimBackend();
    const bool needActiveRewake =
        settings.performance.activeTiles != ActiveTileMode::Off && sim.gridHasMatter;
    if ((needResize || needBackend || needActiveRewake) && !ensureSimPipelineReady()) return;

    if (simPipeline && simPipeline->ready() &&
        (sz.first != sim.grid_w || sz.second != sim.grid_h)) {
        if (!setSimGridSize(sz.first, sz.second, true)) {
            toast.show("Grid resize failed", 2.f);
        }
    }
    if (simPipeline && simPipeline->ready() &&
        simPipeline->backend() != resolveSimBackend()) {
        const SimBackend prev = simPipeline->backend();
        if (!setSimGridSize(sim.grid_w, sim.grid_h, true)) {
            toast.show("Sim shader switch failed", 2.f);
        } else if (simPipeline->backend() != prev) {
            char msg[48];
            std::snprintf(msg, sizeof(msg), "Sim shader: %s",
                          simBackendLabel(simPipeline->backend()));
            toast.show(msg, 1.8f);
        }
    }
    if (simPipeline && simPipeline->ready() &&
        settings.performance.activeTiles != ActiveTileMode::Off && sim.gridHasMatter) {
        simPipeline->activeTiles.rewakeRememberedBounds(2);
        sim.sleeping = false;
    }
}

void App::applyRuntimeSettings() {
    applyRuntimeSettingsLight();
    applyRuntimeSettingsHeavy();
}

void App::flushPendingHeavySettings() {
    if (!settingsHeavyApplyPending_) return;
    applyRuntimeSettingsHeavy();
    settingsHeavyApplyPending_ = false;
}

bool App::setSimGridSize(int w, int h, bool preserveContent) {
    if (w <= 0 || h <= 0) return false;
    if (!simPipeline) simPipeline = std::make_unique<SimPipeline>();
    const SimBackend wantBackend = resolveSimBackend();
    if (!simPipeline->ready()) {
        sim.grid_w = w;
        sim.grid_h = h;
        sim.brush_x = std::clamp(sim.brush_x, 0, w - 1);
        sim.brush_y = std::clamp(sim.brush_y, 0, h - 1);
        return true;
    }
    if (w == sim.grid_w && h == sim.grid_h && simPipeline->backend() == wantBackend) {
        return true;
    }

    std::vector<uint8_t> prev;
    const int oldW = sim.grid_w;
    const int oldH = sim.grid_h;
    if (preserveContent && simPipeline &&
        simPipeline->readGridTo(prev) &&
        static_cast<int>(prev.size()) == oldW * oldH) {
        flipGlRowsToTopDown(prev, oldW, oldH);
    } else {
        prev.clear();
    }

    sim.grid_w = w;
    sim.grid_h = h;
    sim.brush_x = std::clamp(sim.brush_x, 0, w - 1);
    sim.brush_y = std::clamp(sim.brush_y, 0, h - 1);

    if (render) render->releaseBloomTargets();

    simPipeline->shutdown();
    if (!initSimPipeline(w, h)) {
        initError = "Sim pipeline resize failed";
        return false;
    }
    if (!prev.empty()) {
        simPipeline->uploadGridTopDown(prev, oldW, oldH);
        sim.gridHasMatter = false;
        for (uint8_t cell : prev) {
            if (cell != MAT_EMPTY) {
                sim.gridHasMatter = true;
                break;
            }
        }
    } else {
        simPipeline->clearAll(MAT_EMPTY);
        sim.gridHasMatter = false;
    }
    return true;
}

void App::shutdown() {
    toneAudioShutdown();
    closeController(input);
    flushPhysicsParamsIfDirty(physics);
    flushGameSettingsIfDirty(settings);
    menuSim.shutdown();
    font.shutdown();
    if (render) render->shutdown();
    render.reset();
    if (simPipeline) simPipeline->shutdown();
    simPipeline.reset();
    if (glCtx) SDL_GL_DeleteContext(glCtx);
    glCtx = nullptr;
    if (window) SDL_DestroyWindow(window);
    window = nullptr;
    SDL_Quit();
}

void App::onEnterPlayFromMenu() {
    if (!ensureSimPipelineReady()) {
        scene = Scene::Menu;
        return;
    }
    flushPendingHeavySettings();
    playSaveSuppressFrames_ = kPlaySaveSuppressFrames;
    resetSimExplosionFx();
}

void App::beginSaveOverlayFor(PendingSaveKind kind) {
#if !defined(__SWITCH__)
    if (kind == PendingSaveKind::Slot)
        saveOverlay.begin("Saving...");
    else if (kind == PendingSaveKind::GameSettings || kind == PendingSaveKind::PhysicsSettings)
        saveOverlay.begin("Saving settings...");
#else
    (void)kind;
#endif
}

void App::enqueuePendingSave(PendingSaveKind kind) {
    if (kind == PendingSaveKind::None) return;
    for (size_t i = 0; i < pendingSaveCount_; ++i) {
        if (pendingSaveQueue_[i] == kind) return;
    }
    if (pendingSaveCount_ >= kPendingSaveQueueCap) return;
    const bool wasEmpty = pendingSaveCount_ == 0;
    pendingSaveQueue_[pendingSaveCount_++] = kind;
    if (wasEmpty) beginSaveOverlayFor(kind);
}

void App::schedulePendingHeavySettingsFlush() {
    if (settingsHeavyApplyPending_) heavyFlushScheduled_ = true;
}

void App::tickPendingHeavySettings() {
    if (!heavyFlushScheduled_) return;
    heavyFlushScheduled_ = false;
#if !defined(__SWITCH__)
    if (settingsHeavyApplyPending_) toast.show("Applying sim changes...", 1.2f);
#endif
    flushPendingHeavySettings();
}

void App::executePendingSave(PendingSaveKind kind) {
    switch (kind) {
        case PendingSaveKind::Slot: {
            const bool ok = saveGame(*this, pendingSlot_);
            if (pendingSlot_ == 1)
                toast.show(ok ? "Saved slot 1" : "Save failed", ok ? 1.0f : 1.4f);
            else
                toast.show(ok ? "Saved" : "Save failed", ok ? 1.2f : 1.5f);
            break;
        }
        case PendingSaveKind::GameSettings: {
            const bool wasDirty = gameSettingsDirty();
            const bool ok = flushGameSettingsIfDirty(settings);
            if (wasDirty && !ok)
                toast.show("Save settings failed", 1.5f);
            break;
        }
        case PendingSaveKind::PhysicsSettings: {
            const bool wasDirty = physicsParamsDirty();
            const bool ok = flushPhysicsParamsIfDirty(physics);
            if (wasDirty && !ok)
                toast.show("Save settings failed", 1.5f);
            break;
        }
        default: break;
    }
}

void App::tickPendingSave(double dtSec) {
    if (pendingSaveCount_ == 0) return;
#if !defined(__SWITCH__)
    if (!saveOverlay.active()) beginSaveOverlayFor(pendingSaveQueue_[0]);
    saveOverlay.tick(static_cast<float>(dtSec));
    if (!saveOverlay.readyForIo()) return;
#else
    (void)dtSec;
#endif
    const PendingSaveKind kind = pendingSaveQueue_[0];
    executePendingSave(kind);
    for (size_t i = 1; i < pendingSaveCount_; ++i)
        pendingSaveQueue_[i - 1] = pendingSaveQueue_[i];
    --pendingSaveCount_;
#if !defined(__SWITCH__)
    saveOverlay.end();
#endif
}

void App::requestSlotSave(int slot, bool fromQuickSave) {
    if (fromQuickSave && playSaveSuppressFrames_ > 0) return;
#if defined(__SWITCH__)
    const bool ok = saveGame(*this, slot);
    if (slot == 1)
        toast.show(ok ? "Saved slot 1" : "Save failed", ok ? 1.0f : 1.4f);
    else
        toast.show(ok ? "Saved" : "Save failed", ok ? 1.2f : 1.5f);
    return;
#endif
    if (pendingSaveCount_ > 0) return;
    pendingSlot_ = slot;
    enqueuePendingSave(PendingSaveKind::Slot);
}

void App::requestFlushGameSettings() {
    if (!gameSettingsDirty()) return;
    enqueuePendingSave(PendingSaveKind::GameSettings);
}

void App::requestFlushPhysicsSettings() {
    if (!physicsParamsDirty()) return;
    enqueuePendingSave(PendingSaveKind::PhysicsSettings);
}

void App::tickMenu(double dtSec) {
    menu.tick++;
    menuSim.tick(menu.tick,
                 settings.visuals.flicker && !settings.accessibility.reduceFlashing);
    tickMenuBackgroundFx(menu.tick, screenW, screenH);
    pollInput(input, false, true, window, nullptr, 0, 0, settings.controls.cursorSpeed,
              settings.controls.deadzone, settings.controls.invertY, settings.display.orientation,
              settings.accessibility.uiScale);
    if (input.quitRequested) return;

    if (input.menuPointerActive) {
        menu.handlePointer(*this, input.menuPointerX, input.menuPointerY,
                           input.menuPointerConfirm);
        input.menuPointerConfirm = false;
    }
    if (input.menuConfirm) {
        playTone(ToneId::UiConfirm);
        menu.handleConfirm(*this);
        input.menuConfirm = false;
    }
    if (input.menuBack) {
        playTone(ToneId::UiBack);
        if (menu.screen == MenuScreen::SettingsEdit) requestFlushPhysicsSettings();
        if (menu.screen == MenuScreen::EngineSettingsTab) {
            schedulePendingHeavySettingsFlush();
            requestFlushGameSettings();
        }
        menu.goBack(*this);
        input.menuBack = false;
    }
    static bool prevMenuUpHeld = false;
    static bool prevMenuDownHeld = false;
    const bool edgeMenuUp = input.menuUpHeld && !prevMenuUpHeld;
    const bool edgeMenuDown = input.menuDownHeld && !prevMenuDownHeld;
    prevMenuUpHeld = input.menuUpHeld;
    prevMenuDownHeld = input.menuDownHeld;
    if (edgeMenuUp || edgeMenuDown) playTone(ToneId::UiNav);

    static bool prevMenuLeftHeld = false;
    static bool prevMenuRightHeld = false;
    const bool edgeLeft = input.menuLeftHeld && !prevMenuLeftHeld;
    const bool edgeRight = input.menuRightHeld && !prevMenuRightHeld;
    prevMenuLeftHeld = input.menuLeftHeld;
    prevMenuRightHeld = input.menuRightHeld;

    const bool allowHRepeat = menuAllowsHorizontalRepeat(menu);
    const MenuRepeatPulses nav = menuRepeat_.tick(
        static_cast<float>(dtSec), input.menuUpHeld, input.menuDownHeld,
        allowHRepeat ? input.menuLeftHeld : false, allowHRepeat ? input.menuRightHeld : false);
    if (nav.up) menu.moveVertical(-1);
    if (nav.down) menu.moveVertical(1);
    if (allowHRepeat) {
        if (nav.left) menu.adjustHorizontal(*this, -1);
        if (nav.right) menu.adjustHorizontal(*this, 1);
    } else {
        if (edgeLeft) menu.adjustHorizontal(*this, -1);
        if (edgeRight) menu.adjustHorizontal(*this, 1);
    }

#if !defined(__SWITCH__)
    if (!simWarmupTriggered_ &&
        (getenvEnabled("NXSAND_WARM_SIM") || getenvEnabled("NXENGINE_WARM_SIM")) &&
        menu.tick >= 2 && menu.screen == MenuScreen::Main &&
        (!simPipeline || !simPipeline->ready())) {
        simWarmupTriggered_ = true;
        ensureSimPipelineReady();
    }
#endif
}

void App::tickPlay(double dtSec) {
    if (!ensureSimPipelineReady()) return;

    const bool profilerOn = settings.debug.profilerHud != ProfilerHud::Off;
    PlayRegion pr = getPlayRegionForScene(screenW, screenH, sim.grid_w, sim.grid_h,
                                          settings.display.fullscreenSim, !sim.paletteHidden,
                                          false, profilerOn);

    pollInput(input, menu.materialWheelOpen, false, window, &pr, sim.grid_w, sim.grid_h,
              settings.controls.cursorSpeed, settings.controls.deadzone,
              settings.controls.invertY, settings.display.orientation,
              settings.accessibility.uiScale);
    if (input.quitRequested) return;
    perf_.paintHeld = input.painting;
    perf_.eraseHeld = input.erasing;
    perf_.brushMaterial = static_cast<int>(sim.brush_mat);
    perf_.brushRadius = sim.brush_radius;

    if (input.openMenu) {
        requestFlushGameSettings();
        menu.materialWheelOpen = false;
        scene = Scene::Menu;
        menu.resetMain();
        hasEnteredPlay = true;
        input.openMenu = false;
        return;
    }

    if (playSaveSuppressFrames_ > 0) --playSaveSuppressFrames_;

    const int n = selectorMaterialCount();
    const int maxI = std::max(0, n - 1);

    if (menu.materialWheelOpen) {
        if (n > 0) {
#if !defined(__SWITCH__)
            if (input.materialWheelHoverIndex >= 0) {
                menu.materialWheelIndex =
                    std::clamp(input.materialWheelHoverIndex, 0, maxI);
            } else if (input.menuLeft || input.menuUp) {
                menu.materialWheelIndex = (menu.materialWheelIndex + n - 1) % n;
                input.menuLeft = false;
                input.menuUp = false;
            } else if (input.menuRight || input.menuDown) {
                menu.materialWheelIndex = (menu.materialWheelIndex + 1) % n;
                input.menuRight = false;
                input.menuDown = false;
            } else {
                const int idx = materialWheelIndexFromStick(input.ringStickX, input.ringStickY, n,
                                                            0.01f);
                if (idx >= 0) menu.materialWheelIndex = std::clamp(idx, 0, maxI);
            }
#else
            const int idx =
                materialWheelIndexFromStick(input.ringStickX, input.ringStickY, n, 0.01f);
            if (idx >= 0) menu.materialWheelIndex = std::clamp(idx, 0, maxI);
#endif
        }
        if (input.materialRingConfirm || input.menuConfirm) {
            if (n > 0) {
                sim.brush_mat = PICKER_MATERIALS[static_cast<size_t>(
                    std::clamp(menu.materialWheelIndex, 0, maxI))];
            }
            playTone(ToneId::MaterialPick);
            menu.materialWheelOpen = false;
            input.menuConfirm = false;
            input.materialRingConfirm = false;
        } else if (input.materialRingCancel || input.menuBack) {
            menu.materialWheelOpen = false;
            input.menuBack = false;
            input.materialRingCancel = false;
        } else if (input.toggleMaterialRing) {
            menu.materialWheelOpen = false;
            input.toggleMaterialRing = false;
        }
        input.menuUp = input.menuDown = input.menuLeft = input.menuRight = false;
    } else {
        if (input.toggleMaterialRing) {
            sim.sleeping = false;
            menu.materialWheelOpen = true;
            menu.materialWheelIndex = std::clamp(materialSelectorIndex(sim.brush_mat), 0, maxI);
            input.toggleMaterialRing = false;
        }

        if (input.dropper) {
            sim.brush_mat = simPipeline->sampleMaterial(sim.brush_x, sim.brush_y);
            toast.show(material_name(sim.brush_mat), 0.8f);
            input.dropper = false;
        }

        const int prevX = sim.brush_x;
        const int prevY = sim.brush_y;
        if (input.pointerSetsBrush) {
            sim.brush_x = input.pointerGx;
            sim.brush_y = input.pointerGy;
        }
        sim.brush_x += input.brushDx;
        sim.brush_y += input.brushDy;
        sim.brush_x = std::max(0, std::min(sim.grid_w - 1, sim.brush_x));
        sim.brush_y = std::max(0, std::min(sim.grid_h - 1, sim.brush_y));

        if (input.brushRadiusDelta != 0) {
            sim.brush_radius += input.brushRadiusDelta;
            sim.brush_radius = std::clamp(sim.brush_radius, 1, 64);
            settings.controls.brushRadius = sim.brush_radius;
            markGameSettingsDirty();
            requestFlushGameSettings();
            char radiusMsg[32];
            std::snprintf(radiusMsg, sizeof(radiusMsg), "Brush radius %d", sim.brush_radius);
            toast.show(radiusMsg, 0.55f);
            input.brushRadiusDelta = 0;
        }

        if (input.clearSandbox) {
            simPipeline->clearAll(MAT_EMPTY);
            sim.gridHasMatter = false;
            sim.sleeping = false;
            resetSimExplosionFx();
            toast.show("Cleared", 1.0f);
            input.clearSandbox = false;
        }

        if (input.quickSave) {
            requestSlotSave(1, true);
            input.quickSave = false;
        }

        const bool wantPaint = settings.accessibility.togglePaint
                                   ? (input.painting && !input.erasing)
                                   : (input.painting || input.erasing);
        if (wantPaint) {
            sim.gridHasMatter = true;
            sim.sleeping = false;
            perf_.beginPaint();
            int cmdCount = 0;
            int pdw = 0, pdh = 0;
            emitBrushStroke(*simPipeline, prevX, prevY, sim.brush_x, sim.brush_y, sim.brush_radius,
                            sim.brush_mat, input.erasing, cmdCount, pdw, pdh);
            {
                const int pad = sim.brush_radius + 8;
                const int bx0 = std::max(0, std::min(prevX, sim.brush_x) - pad);
                const int by0 = std::max(0, std::min(prevY, sim.brush_y) - pad);
                const int bx1 = std::min(sim.grid_w - 1, std::max(prevX, sim.brush_x) + pad);
                const int by1 = std::min(sim.grid_h - 1, std::max(prevY, sim.brush_y) + pad);
                notifySimExplosionWatch(bx0, by0, bx1, by1);
            }
            perf_.brushCommandCount += cmdCount;
            perf_.paintDirtyW = std::max(perf_.paintDirtyW, pdw);
            perf_.paintDirtyH = std::max(perf_.paintDirtyH, pdh);
            perf_.endPaint();
            pulsePaintRumble(input, settings.controls);
        }
        sim.prev_brush_x = sim.brush_x;
        sim.prev_brush_y = sim.brush_y;
    }

    constexpr int kSleepIdleFrames = 30;
    static int idleFrames = 0;
    const bool wantPaintNow = settings.accessibility.togglePaint
                                  ? (input.painting && !input.erasing)
                                  : (input.painting || input.erasing);
    if (wantPaintNow || menu.materialWheelOpen) {
        sim.sleeping = false;
        idleFrames = 0;
    }

    if (!sim.paused && !sim.sleeping) {
        static double simAccum = 0.0;
        const double kFixedDt = 1.0 / 60.0;
        constexpr double kSimBudgetMs = 20.0;
#if defined(__SWITCH__)
        int kSubstepsPerTick = effectiveSubsteps(settings.performance, true);
#else
        int kSubstepsPerTick = effectiveSubsteps(settings.performance, false);
        if (perf_.lastSimMs > kSimBudgetMs && kSubstepsPerTick > 1) {
            kSubstepsPerTick = 1;
        }
#endif
        // Cap catch-up to 2 ticks: deeper catch-up only exists to hide single-frame
        // hitches. Beyond that it stacks GPU work into the next swap and turns a hiccup
        // into a sustained low-FPS spiral. We'd rather drop sim time than fall further behind.
        int kMaxSteps = 2;
        if (perf_.lastSimMs > kSimBudgetMs) {
            kMaxSteps = 1;
        }
        simAccum += dtSec;
        int steps = 0;
        perf_.substeps = kSubstepsPerTick;
        while (simAccum >= kFixedDt && steps < kMaxSteps) {
            for (int s = 0; s < kSubstepsPerTick; ++s) {
                perf_.beginSim();
                simPipeline->step(sim.tick, physics, settings.performance.activeTiles);
                perf_.endSim();
                perf_.fragmentPasses += simPipeline->lastPasses();
                perf_.activeTileMode = static_cast<int>(simPipeline->lastActiveTileMode());
                perf_.activeTileCount = simPipeline->lastActiveTileCount();
                perf_.activeTileFallback = simPipeline->lastActiveTileFallback();
                ++sim.tick;
            }
            simAccum -= kFixedDt;
            ++steps;
        }
        if (simAccum > kFixedDt * 4.0) {
            simAccum = kFixedDt;
        }
        if (settings.performance.activeTiles != ActiveTileMode::Off) {
            simPipeline->activeTiles.tickOptimizer(settings.performance.activeTiles);
        }

        const bool profilerOn = settings.debug.profilerHud != ProfilerHud::Off;
        const PlayRegion fxPr =
            getPlayRegionForScene(screenW, screenH, sim.grid_w, sim.grid_h,
                                  settings.display.fullscreenSim, !sim.paletteHidden, false,
                                  profilerOn);
        expandSimExplosionWatch(sim.grid_w, sim.grid_h, 4);
        int ax0 = 0;
        int ay0 = 0;
        int ax1 = -1;
        int ay1 = -1;
        if (simPipeline->activeTiles.activeBounds(ax0, ay0, ax1, ay1, 1)) {
            notifySimExplosionWatch(ax0, ay0, ax1, ay1);
        }
        tickSimExplosionFx(*simPipeline, fxPr, sim.grid_w, sim.grid_h, sim.tick);
    }

    bool canEnterIdleSleep = false;
    if (settings.performance.activeTiles != ActiveTileMode::Off) {
        canEnterIdleSleep = (simPipeline->activeTiles.activeCount() == 0);
    } else {
        canEnterIdleSleep = !sim.gridHasMatter;
    }
    if (!sim.paused && canEnterIdleSleep) {
        if (++idleFrames >= kSleepIdleFrames) {
            sim.sleeping = true;
        }
    } else if (!sim.paused) {
        idleFrames = 0;
    }
    perf_.simSleeping = sim.sleeping;
    perf_.gridHasMatter = sim.gridHasMatter;
}

void App::renderFrame() {
    perf_.simW = sim.grid_w;
    perf_.simH = sim.grid_h;
    perf_.presetLabel = perfPresetLabel(settings.performance.mode);
    if (simPipeline && simPipeline->ready()) {
        perf_.simBackendLabel = simBackendLabel(simPipeline->backend());
    }
    render->beginUiFrame();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenW, screenH);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.03f, 0.04f, 0.06f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (scene == Scene::Play && simPipeline && simPipeline->ready()) {
        const bool profilerOn = settings.debug.profilerHud != ProfilerHud::Off;
        PlayRegion pr = getPlayRegionForScene(screenW, screenH, sim.grid_w, sim.grid_h,
                                              settings.display.fullscreenSim, !sim.paletteHidden,
                                              false, profilerOn);
        perf_.beginWorldRender();
        simPipeline->syncSimForSampling();
        render->drawSimulation(simPipeline->readTexture(), sim.grid_w, sim.grid_h, pr, screenH,
                               sim.tick, perf_.simMs);
        drawSimExplosionFx(*render, pr, sim.grid_w, sim.grid_h, screenW, screenH);
        perf_.endWorldRender();

        perf_.beginUi();
        drawHudSolid(*render, *this, pr);
        drawBrushCursor(*render, *this, pr);
        drawActiveTilesOverlay(*render, *this, pr);
        drawPerfOverlay(*render, font, perf_, settings.debug, pr, !sim.paletteHidden, screenW,
                        screenH, settings.accessibility.uiScale);
        if (menu.materialWheelOpen) {
            drawMaterialWheel(*render, font, *this);
        }
    } else {
        perf_.beginUi();
        drawMenuSolid(*render, font, *this);
    }

    drawToastSolid(*render, *this);
#if !defined(__SWITCH__)
    saveOverlay.draw(*render, font, screenW, screenH, settings.accessibility.uiScale);
#endif
    render->endUiFrame();
    perf_.endUi();
    perf_.beginPresent();
    SDL_GL_SwapWindow(window);
    perf_.endPresent();
}

void App::frame(double dtSec) {
    perf_.beginFrame();
    tickPendingSave(dtSec);
    tickPendingHeavySettings();
    if (window) queryDrawableSize(window, screenW, screenH, settings.display.orientation);
    if (screenW > 0 && screenH > 0 &&
        (screenW != lastScreenW_ || screenH != lastScreenH_)) {
        const bool preserve = lastScreenW_ > 0 && lastScreenH_ > 0 && simPipeline &&
                              simPipeline->ready();
        const auto simSz = resolveSimGridSize(screenW, screenH, settings.performance);
        if (!setSimGridSize(simSz.first, simSz.second, preserve)) {
            input.quitRequested = true;
            return;
        }
        lastScreenW_ = screenW;
        lastScreenH_ = screenH;
    }

    if (settings.performance.dynamicResolution && scene == Scene::Play && !input.painting &&
        !input.erasing) {
        static int stableFast = 0;
        if (perf_.frameMs > 28.0) {
            stableFast = 0;
            if (settings.performance.mode != PerfPreset::BatterySaver) {
                applyPerfPreset(settings.performance, PerfPreset::BatterySaver,
#if defined(__SWITCH__)
                                true
#else
                                false
#endif
                );
                applyPerfPresetPhysics(physics, PerfPreset::BatterySaver);
                applyRuntimeSettings();
            }
        } else if (perf_.frameMs < 13.0) {
            if (++stableFast > 300 && settings.performance.mode == PerfPreset::BatterySaver) {
                applyPerfPreset(settings.performance, PerfPreset::Balanced,
#if defined(__SWITCH__)
                                true
#else
                                false
#endif
                );
                applyPerfPresetPhysics(physics, PerfPreset::Balanced);
                applyRuntimeSettings();
                stableFast = 0;
            }
        }
    }
    if (scene == Scene::Menu) {
        tickMenu(dtSec);
    } else {
        tickPlay(dtSec);
    }
    toast.tick(dtSec);
    if (!input.quitRequested) renderFrame();
    perf_.endFrame();

#if !defined(__SWITCH__)
    static int s_titleFrames = 0;
    static auto s_titleLast = std::chrono::steady_clock::now();
    ++s_titleFrames;
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<double>(now - s_titleLast).count() >= 1.0) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s - %.0f FPS", theme::APP_TITLE,
                      static_cast<double>(perf_.fps));
        SDL_SetWindowTitle(window, buf);
        s_titleFrames = 0;
        s_titleLast = now;
    }
    (void)s_titleFrames;
#endif
}

} // namespace nx
