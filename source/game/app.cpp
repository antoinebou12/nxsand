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
#include "platform/audio/menu_music.hpp"
#include "platform/audio/tone_audio.hpp"
#include "platform/launch_log.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cmath>
#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace nx {

namespace {

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
#if defined(__SWITCH__)
    appendLaunchLog(stage);
#endif
    if (!getenvEnabled("NXSAND_BOOT_LOG") && !getenvEnabled("NXENGINE_BOOT_LOG")) return;
    static uint32_t t0 = SDL_GetTicks();
    std::cerr << "[boot +" << (SDL_GetTicks() - t0) << "ms] " << stage << "\n";
}

#if defined(__SWITCH__)
static const char* simBackendName(SimBackend b) {
    switch (b) {
        case SimBackend::Compute: return "compute";
        case SimBackend::Fragment: return "fragment";
    }
    return "?";
}
#endif

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
    bootLogStage("init: begin");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0) {
        initError = std::string("SDL_Init: ") + SDL_GetError();
        std::cerr << initError << "\n";
        return false;
    }
    bootLogStage("init: SDL ok");

    if (!ensureSaveStorageAtLaunch()) {
#if defined(__SWITCH__)
        appendLaunchLog("save dir: sdmc:/switch/nxsand/ unavailable (check microSD)");
#endif
    }
    bootLogStage("init: save directory");

    loadGameSettings(settings);
    bootLogStage("init: settings loaded");
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
    bootLogStage("init: window ok");

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
    bootLogStage("init: GL context ok");
    if (SDL_GL_MakeCurrent(window, glCtx) != 0) {
        initError = std::string("SDL_GL_MakeCurrent: ") + SDL_GetError();
        std::cerr << initError << "\n";
        return false;
    }
    SDL_GL_SetSwapInterval(1);

    queryDrawableSize(window, screenW, screenH, settings.display.orientation);
    queryGlFramebufferSize(window, framebufferW, framebufferH);
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
    bootLogStage("init: GLAD ok");
#if defined(__SWITCH__)
    appendLaunchLogf("parallel_shader_compile: %s",
                     gl::parallel_shader_compile_available() ? "yes" : "no");
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, framebufferW, framebufferH);
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
        appendLaunchLogf("switch: saved sim backend=%s supported=%s",
                         simBackendName(settings.performance.simBackend),
                         computeSimSupported_ ? "yes" : "no");
        appendLaunchLog("switch: boot sim deferred (first play compile)");
        const bool forceFragment =
            getenvEnabled("NXSAND_FORCE_FRAGMENT") || getenvEnabled("NXENGINE_FORCE_FRAGMENT");
        if (forceFragment) {
            settings.performance.simBackend = SimBackend::Fragment;
            markGameSettingsDirty();
            appendLaunchLog("switch: NXSAND_FORCE_FRAGMENT set");
        }
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
#if !defined(__SWITCH__)
    flushPendingShaderCacheSaves();
#endif
    presentBootProgress(0.05f, "Loading shaders...");
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
#if defined(__SWITCH__)
    appendLaunchLogf("font ready: tex=%u lineH=%d", static_cast<unsigned>(font.tex), font.lineH);
#endif
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
    if (!ensureSaveStorageAtLaunch()) {
        toast.show("SD saves unavailable (check microSD)", 4.0f);
    }
    presentBootProgress(0.92f, "Almost ready...");
#else
    if (!ensureSaveStorageAtLaunch()) {
        std::cerr << "Warning: could not create save folder " << saveDirectory() << "\n";
    }
#endif

    openFirstController(input);
    menu.resetMain();
    resetMenuRepeat();
    syncScreenMetrics();
    presentBootProgress(1.f, "Ready");
    bootLogStage("init complete");
    return true;
}

void App::ensureUiFontReady() {
    if (font.isReady()) return;
    font.invalidateGlTexture();
    if (!font.init()) {
#if defined(__SWITCH__)
        appendLaunchLog("font ensureUiFontReady: init failed");
#endif
    }
}

void App::syncScreenMetrics() {
    if (!window) return;
    queryDrawableSize(window, screenW, screenH, settings.display.orientation);
    queryGlFramebufferSize(window, framebufferW, framebufferH);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, framebufferW, framebufferH);
    if (render) render->prepareUiDraw(screenW, screenH, framebufferW, framebufferH);
#if defined(__SWITCH__)
    if (!screenMetricsLogged_) {
        screenMetricsLogged_ = true;
        int winW = 0, winH = 0;
        SDL_GetWindowSize(window, &winW, &winH);
        appendLaunchLogf("layout %dx%d GL FB %dx%d window %dx%d uiScale=%.2f rawPortrait=%d uiRotate=0", screenW,
                         screenH, framebufferW, framebufferH, winW, winH,
                         theme::uiScale(screenW, screenH, settings.accessibility.uiScale),
                         switchPortraitFramebuffer(framebufferW, framebufferH, screenW, screenH)
                             ? 1
                             : 0);
    }
#endif
}

void App::presentBootProgress(float progress, const char* status) {
    if (!window || !glCtx || !render) return;
    syncScreenMetrics();
    render->beginUiFrame();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.03f, 0.04f, 0.06f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    ensureUiFontReady();
    if (font.isReady()) {
        drawBootScreen(*render, font, screenW, screenH, progress, status);
    } else {
        drawCompileOverlay(*render, screenW, screenH, progress);
    }
    render->endUiFrame();
    SDL_GL_SwapWindow(window);
}

void App::presentCompileOverlay(float progress) {
    if (!window || !glCtx || !render) return;
    syncScreenMetrics();
    render->beginUiFrame();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.03f, 0.04f, 0.06f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    drawCompileOverlay(*render, screenW, screenH, progress);
    render->endUiFrame();
    SDL_GL_SwapWindow(window);
}

void App::releaseMemoryBeforeSimCompile() {
#if defined(__SWITCH__)
    menuMusicReleaseTheme();
    toneAudioReleaseCachedWavs();
#endif
}

void App::reloadAudioAfterSimCompile() {
    if (!audioReady_) return;
    if (menuMusicInit()) {
        menuMusicSetActive(scene == Scene::Menu && settings.audio.menuMusic);
    }
}

#if defined(__SWITCH__)
static void compileStatusText(const char* stage, uint64_t elapsedMs, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    const unsigned long long sec = static_cast<unsigned long long>(elapsedMs / 1000u);
    const char* line = "Compiling sim...";
    if (stage) {
        if (std::strstr(stage, "Linking sim.frag") || std::strstr(stage, "Linking sim.comp")) {
            line = "Linking sim...";
        } else if (std::strstr(stage, "Compiling sim.frag") ||
                   std::strstr(stage, "Compiling sim.comp")) {
            line = "Compiling sim...";
        } else if (std::strstr(stage, "Compiling paint")) {
            line = "Paint shader...";
        } else if (std::strstr(stage, "palette_lookup")) {
            line = "Palette shader...";
        } else if (std::strstr(stage, "upscale")) {
            line = "Upscale shader...";
        } else if (std::strstr(stage, "ui_quad")) {
            line = "UI shaders...";
        } else if (std::strstr(stage, "Preparing")) {
            line = "Preparing sim...";
        }
    }
    std::snprintf(out, outSize, "%s %llus", line, sec);
}
#endif

struct SimCompileAudioGuard {
    SimCompileAudioGuard() { toneAudioSetOutputPaused(true); }
    ~SimCompileAudioGuard() { toneAudioSetOutputPaused(false); }
};

static void shaderCompileProgress(const char* stage, uint64_t elapsedMs, void* user) {
    auto* app = static_cast<App*>(user);
    if (!app || !stage) return;
#if defined(__SWITCH__)
    static std::string s_lastShaderStageLog;
    if (s_lastShaderStageLog != stage) {
        s_lastShaderStageLog = stage;
        appendLaunchLogf("shader stage: %s", stage);
    }
#endif
    char buf[160];
    const unsigned long long sec = static_cast<unsigned long long>(elapsedMs / 1000u);
    std::snprintf(buf, sizeof(buf), "%s (%llus)", stage, sec);
    float linkBudgetMs = 90000.f;
#if defined(__SWITCH__)
    if (stage && (std::strstr(stage, "Linking sim.frag") || std::strstr(stage, "Linking sim.comp") ||
                  std::strstr(stage, "Compiling sim.frag") ||
                  std::strstr(stage, "Compiling sim.comp"))) {
        linkBudgetMs = 300000.f;
    }
#endif
    const float t = std::min(1.f, static_cast<float>(elapsedMs) / linkBudgetMs);
    SDL_PumpEvents();
#if defined(__SWITCH__)
    char status[96];
    compileStatusText(stage, elapsedMs, status, sizeof(status));
    app->ensureUiFontReady();
    if (app->font.isReady()) {
        app->presentBootProgress(0.5f + 0.2f * t, status);
    } else {
        app->presentCompileOverlay(0.5f + 0.2f * t);
    }
#else
    app->presentBootProgress(0.5f + 0.2f * t, buf);
#endif
}

#if defined(__SWITCH__)
bool App::prepareLightSimCompileOnSwitch() {
    bootLogStage("sim compile prep (switch)");
    appendLaunchLog("sim compile start");
    prepareGlContextForShaderLink();
    if (simPipeline) simPipeline->shutdown();
    return true;
}
#endif

bool App::ensureSimPipelineReady() {
    if (!simPipeline) simPipeline = std::make_unique<SimPipeline>();
    if (simPipeline->ready()) return true;
#if defined(__SWITCH__)
    if (simStartupFailed_) {
        appendLaunchLog("ensureSimPipeline: previous failure, skipping retry");
        return false;
    }
#endif

#if defined(__SWITCH__)
    appendLaunchLogTimed("boot phase: ensureSimPipeline begin");
    appendLaunchLogf("ensureSimPipeline: begin %dx%d backend=%s",
                     sim.grid_w, sim.grid_h, simBackendName(resolveSimBackend()));
#endif
    const SimCompileAudioGuard audioGuard;
#if defined(__SWITCH__)
    releaseMemoryBeforeSimCompile();
    presentBootProgress(0.35f, "Preparing simulation shaders...");
#else
    presentBootProgress(0.5f, "Preparing simulation shaders...");
#endif
    glFinish();
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#if defined(__SWITCH__)
    if (!prepareLightSimCompileOnSwitch()) {
        toast.show("Simulation failed to start", 2.5f);
        return false;
    }
#else
    if (!resetGlContextForSimCompile()) {
        toast.show("Simulation failed to start", 2.5f);
        return false;
    }
#endif
    bootLogStage("sim pipeline compile");
#if defined(__SWITCH__)
    appendLaunchLogTimed("boot phase: sim compile begin");
#endif
    const auto compileStart = std::chrono::steady_clock::now();
#if defined(__SWITCH__)
    const bool showCompileProgress = true;
#else
    const bool showCompileProgress =
        getenvEnabled("NXSAND_SHADER_PROGRESS") || getenvEnabled("NXENGINE_SHADER_PROGRESS");
#endif
    if (showCompileProgress) setShaderCompileProgress(shaderCompileProgress, this);
    bool ok = initSimPipeline(sim.grid_w, sim.grid_h);
#if !defined(__SWITCH__)
    flushPendingShaderCacheSaves();
#endif
#if defined(__SWITCH__)
    if (ok) {
        appendLaunchLogTimed("boot phase: sim compile end");
        appendLaunchLog("sim compile ok");
    } else {
        const char* diag = lastShaderDiagnostics();
        appendLaunchLogf("sim compile failed: %s", (diag && diag[0]) ? diag : "?");
    }
#endif
    if (showCompileProgress) setShaderCompileProgress(nullptr, nullptr);
    if (!restoreUiPipelinesAfterSimCompile()) {
#if defined(__SWITCH__)
        appendLaunchLogf("ensureSimPipeline: ui restore failed: %s",
                         initError.empty() ? "?" : initError.c_str());
#endif
        ok = false;
    }
    if (ok && render) {
#if defined(__SWITCH__)
        appendLaunchLogTimed("boot phase: world warmup begin");
#endif
        presentBootProgress(0.85f, "Loading render shaders...");
        if (!render->warmupWorldShaders()) {
            initError = "World shader warmup failed";
#if defined(__SWITCH__)
            appendLaunchLog("world shaders: warmup failed");
#endif
            ok = false;
        } else {
            glFinish();
#if defined(__SWITCH__)
            appendLaunchLogTimed("boot phase: world warmup end");
            for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError()) {
                appendLaunchLogf("gl error: 0x%x after world warmup", static_cast<unsigned>(err));
            }
#endif
        }
    }
    reloadAudioAfterSimCompile();
    if (!ok) {
#if defined(__SWITCH__)
        simStartupFailed_ = true;
#endif
        clearPendingShaderCacheSaves();
        const char* diag = lastShaderDiagnostics();
        if (diag && std::strstr(diag, "timed out")) {
#if defined(__SWITCH__)
            toast.show("Shader compile timed out. Wait for compile to finish; do not force power off.",
                       4.f);
#else
            toast.show(
                "Shader compile timed out. Delete nxsand_save/shader_cache/ or set "
                "NXSAND_SHADER_CACHE=0.",
                4.f);
#endif
        } else {
#if defined(__SWITCH__)
            toast.show(
                "Simulation failed to start. Wait for compile to finish; do not force power off.",
                4.f);
#else
            toast.show("Simulation failed to start", 2.5f);
#endif
        }
        if (scene == Scene::Menu) {
            syncScreenMetrics();
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
#if defined(__SWITCH__)
    appendLaunchLog("ensureSimPipeline: done");
#endif
    syncScreenMetrics();
    if (scene == Scene::Menu) {
        presentBootProgress(1.f, "Ready");
    }
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

    render = std::make_unique<RenderPipeline>();
    if (!render->init(shaderDir)) {
        initError = "Render pipeline failed after GL reset";
        return false;
    }
    if (!font.init()) {
        initError = "Font atlas failed after GL reset";
        return false;
    }
    return true;
}

bool App::restoreUiPipelinesAfterSimCompile() {
    bootLogStage("render pipeline restore after sim compile");
#if defined(__SWITCH__)
    appendLaunchLogTimed("boot phase: ui restore begin");
#endif
    if (window) {
        queryDrawableSize(window, screenW, screenH, settings.display.orientation);
        lastScreenW_ = screenW;
        lastScreenH_ = screenH;
    }
    const std::string& shaderPath = shaderDir.empty() ? resolveShaderDir() : shaderDir;
    if (!render) render = std::make_unique<RenderPipeline>();
#if defined(__SWITCH__)
    // Heavy sim link can leave UI programs / blend state inconsistent on Switch GLES.
    setShaderCompileStage("Compiling ui_quad.frag...");
    render->shutdown();
    if (!render->init(shaderPath)) {
        initError = "Render pipeline restore failed";
        return false;
    }
#else
    if (render->uiShader.program == 0) {
        if (!render->init(shaderPath)) {
            initError = "Render pipeline restore failed";
            return false;
        }
    }
#endif
    font.invalidateGlTexture();
    if (!font.init()) {
        initError = "Font atlas restore failed";
#if defined(__SWITCH__)
        appendLaunchLog(initError.c_str());
#endif
        return false;
    }
#if defined(__SWITCH__)
    appendLaunchLogf("font restored: tex=%u lineH=%d", static_cast<unsigned>(font.tex), font.lineH);
#endif
    font.prewarmCommonGlyphs();
    flushPendingShaderCacheSaves();
    if (!menuSim.init()) {
        initError = "Menu backdrop restore failed";
#if defined(__SWITCH__)
        appendLaunchLog(initError.c_str());
#endif
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glBindVertexArray(0);
    syncScreenMetrics();
    applyRuntimeSettingsLight();
#if defined(__SWITCH__)
    appendLaunchLogTimed("boot phase: ui restore end");
#endif
    return true;
}

SimBackend App::resolveSimBackend() const {
    if (getenvEnabled("NXSAND_FORCE_FRAGMENT") || getenvEnabled("NXENGINE_FORCE_FRAGMENT")) {
        return SimBackend::Fragment;
    }
    if (!computeSimSupported_) return SimBackend::Fragment;
    if (forceComputeBackend_) return SimBackend::Compute;
    return settings.performance.simBackend;
}

bool App::initSimPipeline(int w, int h) {
    const SimBackend backend = resolveSimBackend();
#if defined(__SWITCH__)
    appendLaunchLogf("initSimPipeline: trying %s", simBackendName(backend));
#endif
    if (simPipeline->init(w, h, shaderDir, backend)) {
        if (backend != SimBackend::Compute) return true;
        std::string selfTestErr;
        if (simPipeline->runMovementSelfTest(&selfTestErr)) {
#if defined(__SWITCH__)
            appendLaunchLog("compute self-test: pass");
#endif
            return true;
        }
#if defined(__SWITCH__)
        appendLaunchLogf("compute self-test: failed: %s",
                         selfTestErr.empty() ? "?" : selfTestErr.c_str());
#endif
        setShaderDiagnostics(selfTestErr.empty() ? "Compute self-test failed"
                                                 : "Compute self-test failed: " + selfTestErr);
        simPipeline->shutdown();
    }
    if (backend != SimBackend::Compute) return false;
#if defined(__SWITCH__)
    appendLaunchLog("initSimPipeline: compute failed, trying fragment");
#endif
    computeSimSupported_ = false;
    if (!forceComputeBackend_ && settings.performance.simBackend == SimBackend::Compute) {
        settings.performance.simBackend = SimBackend::Fragment;
        markGameSettingsDirty();
    }
    if (forceComputeBackend_) {
#if defined(__SWITCH__)
        appendLaunchLog("initSimPipeline: compute forced, refusing fragment fallback");
#endif
        return false;
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
    toneAudioSetLevel(settings.audio.sound);
    if (scene == Scene::Menu) {
        menuMusicSetActive(settings.audio.menuMusic);
    }
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
#if defined(__SWITCH__)
    if (!simPipeline || !simPipeline->ready()) {
        const auto sz = resolveSimGridSize(screenW, screenH, settings.performance);
        sim.grid_w = sz.first;
        sim.grid_h = sz.second;
        sim.brush_x = std::clamp(sim.brush_x, 0, std::max(0, sim.grid_w - 1));
        sim.brush_y = std::clamp(sim.brush_y, 0, std::max(0, sim.grid_h - 1));
        settingsHeavyApplyPending_ = false;
        heavyFlushScheduled_ = false;
        appendLaunchLogf("heavy settings: staged before sim init %dx%d backend=%s",
                         sim.grid_w, sim.grid_h, simBackendName(resolveSimBackend()));
        return;
    }
#endif
    applyRuntimeSettingsHeavy();
    settingsHeavyApplyPending_ = false;
    heavyFlushScheduled_ = false;
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

void App::ensureAudioReady() {
    if (audioReady_) return;
    audioReady_ = true;
    toneAudioSetLevel(settings.audio.sound);
    if (!toneAudioEnsureReady()) {
#if defined(__SWITCH__)
        appendLaunchLog("audio: output init failed (continuing without audio)");
#endif
        return;
    }
    if (menuMusicInit()) {
        menuMusicSetActive(scene == Scene::Menu && settings.audio.menuMusic);
#if defined(__SWITCH__)
        appendLaunchLog("audio: menu theme loaded");
#endif
    } else {
#if defined(__SWITCH__)
        appendLaunchLog("audio: menu theme missing (continuing without music)");
#endif
    }
}

void App::shutdown() {
#if defined(__SWITCH__)
    appendLaunchLog("stage: shutdown begin");
#endif
    toneAudioSetOutputPaused(true);
    menuMusicShutdown();
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
    if (glCtx) {
        glFinish();
        SDL_GL_DeleteContext(glCtx);
    }
    glCtx = nullptr;
    if (window) SDL_DestroyWindow(window);
    window = nullptr;
    SDL_Quit();
#if defined(__SWITCH__)
    appendLaunchLog("stage: shutdown end");
#endif
}

void App::onEnterPlayFromMenu() {
#if defined(__SWITCH__)
    simStartupFailed_ = false;
#endif
    menuMusicSetActive(false);
    if (!ensureSimPipelineReady()) {
        scene = Scene::Menu;
        menuMusicSetActive(settings.audio.menuMusic);
        return;
    }
#if defined(__SWITCH__)
    if (!playEntryLogged_) {
        playEntryLogged_ = true;
        appendLaunchLogf(
            "play entry: grid %dx%d sleeping=%d matter=%d profiler=%d activeTiles=%d",
            sim.grid_w, sim.grid_h, sim.sleeping ? 1 : 0, sim.gridHasMatter ? 1 : 0,
            static_cast<int>(settings.debug.profilerHud),
            static_cast<int>(settings.performance.activeTiles));
        appendLaunchLog("enter play: sim ready");
    }
#endif
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
    if (!settingsHeavyApplyPending_) return;
#if defined(__SWITCH__)
    appendLaunchLog("heavy settings: deferred until play");
    toast.show("Sim changes apply on play", 1.3f);
#else
    heavyFlushScheduled_ = true;
#endif
}

void App::tickPendingHeavySettings() {
#if defined(__SWITCH__)
    if (scene == Scene::Play && settingsHeavyApplyPending_) {
        toast.show("Applying sim changes...", 1.2f);
        flushPendingHeavySettings();
    }
    return;
#endif
    if (!heavyFlushScheduled_) return;
    heavyFlushScheduled_ = false;
    if (settingsHeavyApplyPending_) toast.show("Applying sim changes...", 1.2f);
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
    pollInput(input, false, true, window, nullptr, 0, 0, settings.controls.cursorSpeed,
              settings.controls.deadzone, settings.controls.invertY, settings.display.orientation,
              settings.accessibility.uiScale);
    if (input.quitRequested) return;
#if defined(__SWITCH__)
    menuSim.tick(menu.tick, false);
#else
    menuSim.tick(menu.tick,
                 settings.visuals.flicker && !settings.accessibility.reduceFlashing);
#endif
    tickMenuBackgroundFx(menu.tick, screenW, screenH);

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

    menuMusicTick();

#if !defined(__SWITCH__)
    const bool warmSimEnabled =
        (getenvEnabled("NXSAND_WARM_SIM") || getenvEnabled("NXENGINE_WARM_SIM")) &&
        !getenvEnabled("NXSAND_NO_WARM_SIM");
    if (!simWarmupTriggered_ && warmSimEnabled && menu.tick >= 2 &&
        menu.screen == MenuScreen::Main && (!simPipeline || !simPipeline->ready())) {
        simWarmupTriggered_ = true;
        ensureSimPipelineReady();
    }
#endif
}

void App::tickPlay(double dtSec) {
    if (!ensureSimPipelineReady()) return;
#if defined(__SWITCH__)
    if (!playTickLogged_) {
        playTickLogged_ = true;
        appendLaunchLog("play tick: begin");
    }
#endif

    const bool profilerOn = settings.debug.profilerHud != ProfilerHud::Off;
    PlayRegion pr = getPlayRegionForScene(screenW, screenH, sim.grid_w, sim.grid_h,
                                          settings.display.fullscreenSim, !sim.paletteHidden,
                                          false, profilerOn, settings.accessibility.uiScale);

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
        resetMenuRepeat();
        menuMusicSetActive(settings.audio.menuMusic);
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

        if (input.togglePaletteHud) {
            sim.paletteHidden = !sim.paletteHidden;
            input.togglePaletteHud = false;
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
#if defined(__SWITCH__)
                if (!playStepLogged_) {
                    playStepLogged_ = true;
                    appendLaunchLogf("sim step ok backend=%s",
                                     simBackendLabel(simPipeline->backend()));
                }
#endif
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
                                  profilerOn, settings.accessibility.uiScale);
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
    glViewport(0, 0, framebufferW, framebufferH);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.03f, 0.04f, 0.06f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (scene == Scene::Play && simPipeline && simPipeline->ready()) {
        ensureUiFontReady();
#if defined(__SWITCH__)
        if (!playRenderLogged_) appendLaunchLog("render play: begin");
#endif
        const bool profilerOn = settings.debug.profilerHud != ProfilerHud::Off;
        PlayRegion pr = getPlayRegionForScene(screenW, screenH, sim.grid_w, sim.grid_h,
                                              settings.display.fullscreenSim, !sim.paletteHidden,
                                              false, profilerOn,
                                              settings.accessibility.uiScale);
        perf_.beginWorldRender();
#if defined(__SWITCH__)
        const bool skipEmptySwitchWorldDraw = sim.sleeping && !sim.gridHasMatter;
        if (skipEmptySwitchWorldDraw) {
            if (!playRenderLogged_) appendLaunchLog("render play: skip empty sim draw");
            render->drawSolidRect(pr.x, pr.y, pr.w, pr.h, 0.058f, 0.073f, 0.105f, 1.f, screenW,
                                  screenH);
        } else
#else
        const bool skipEmptySwitchWorldDraw = false;
        (void)skipEmptySwitchWorldDraw;
#endif
        {
#if defined(__SWITCH__)
            if (!playRenderLogged_) appendLaunchLog("render play: sync sim");
#endif
            simPipeline->syncSimForSampling();
#if defined(__SWITCH__)
            if (!playRenderLogged_) appendLaunchLog("render play: draw sim");
#endif
            render->drawSimulation(simPipeline->readTexture(), sim.grid_w, sim.grid_h, pr, screenH,
                                   sim.tick, perf_.simMs);
#if defined(__SWITCH__)
            if (!playRenderLogged_) appendLaunchLog("render play: draw sim done");
#endif
        }
#if defined(__SWITCH__)
        if (!playRenderLogged_) appendLaunchLog("render play: fx");
#endif
        drawSimExplosionFx(*render, pr, sim.grid_w, sim.grid_h, screenW, screenH);
        perf_.endWorldRender();
#if defined(__SWITCH__)
        if (!playRenderLogged_) appendLaunchLog("render play: prepare ui");
#endif
        render->prepareUiDraw(screenW, screenH, framebufferW, framebufferH);

        perf_.beginUi();
#if defined(__SWITCH__)
        if (!playRenderLogged_) appendLaunchLog("render play: hud");
#endif
        drawHudSolid(*render, *this, pr);
#if defined(__SWITCH__)
        if (!playRenderLogged_) appendLaunchLog("render play: hud done");
        if (!playRenderLogged_) appendLaunchLog("render play: brush cursor");
#endif
        drawBrushCursor(*render, *this, pr);
#if defined(__SWITCH__)
        if (!playRenderLogged_) appendLaunchLog("render play: brush cursor done");
        if (!playRenderLogged_) appendLaunchLog("render play: active overlay enter");
#endif
        drawActiveTilesOverlay(*render, *this, pr);
#if defined(__SWITCH__)
        if (!playRenderLogged_) appendLaunchLog("render play: active overlay exit");
        if (!playRenderLogged_) appendLaunchLog("render play: perf enter");
#endif
        if (!menu.materialWheelOpen) {
            drawPerfOverlay(*render, font, perf_, settings.debug, pr, !sim.paletteHidden, screenW,
                            screenH, settings.accessibility.uiScale);
        }
#if defined(__SWITCH__)
        if (!playRenderLogged_) appendLaunchLog("render play: perf exit");
#endif
        if (menu.materialWheelOpen) {
#if defined(__SWITCH__)
            if (!playRenderLogged_) appendLaunchLog("render play: wheel");
#endif
            drawMaterialWheel(*render, font, *this, pr);
#if defined(__SWITCH__)
            if (!playRenderLogged_) appendLaunchLog("render play: wheel done");
#endif
        }
#if defined(__SWITCH__)
        if (!playRenderLogged_) {
            playRenderLogged_ = true;
            appendLaunchLog("render play: end");
        }
#endif
    } else {
        perf_.beginUi();
        syncScreenMetrics();
        ensureUiFontReady();
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
    ensureAudioReady();
    tickPendingSave(dtSec);
    tickPendingHeavySettings();
    if (window) {
        queryDrawableSize(window, screenW, screenH, settings.display.orientation);
        queryGlFramebufferSize(window, framebufferW, framebufferH);
    }
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
