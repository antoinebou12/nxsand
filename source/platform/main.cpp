#include "game/app.hpp"
#include "platform/launch_log.hpp"
#include "save/save_paths.hpp"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <exception>
#include <string>

#if defined(__SWITCH__)
#include <SDL2/SDL.h>
#include <switch.h>
#endif

#if defined(__SWITCH__)
namespace {

void logStage(const char* msg) { appendLaunchLog(msg); }

void logStagef(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    logStage(buf);
}

void switchShowFatal(const char* title, const char* detail) {
    logStagef("FATAL: %s | %s", title ? title : "", detail ? detail : "");
    closeLaunchLog();
    // consoleInit grabs the framebuffer; it crashes silently (returns to hbmenu)
    // if SDL_INIT_VIDEO is still active. Tear SDL down first as a safety net for
    // callers that didn't already shut down (e.g. the uncaught-exception path).
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_Quit();
    }
    consoleInit(nullptr);
    printf("\n%s\n\n", title);
    if (detail && detail[0]) {
        printf("%s\n\n", detail);
    }
    printf("Install the app file here:\n");
    printf("  /switch/NXSand.nro\n\n");
    printf("Saves only (not the app):\n");
    printf("  /switch/nxsand/\n\n");
    printf("Press HOME to exit.\n");
    consoleUpdate(nullptr);

    while (appletMainLoop()) {
        consoleUpdate(nullptr);
    }
    consoleExit(nullptr);
}

bool romfsFilePresent(const char* rel) {
    FILE* f = std::fopen(rel, "rb");
    if (!f) {
        std::string romfsPath = std::string("romfs:/") + rel;
        f = std::fopen(romfsPath.c_str(), "rb");
    }
    if (!f) return false;
    std::fclose(f);
    return true;
}

long romfsFileSizeBytes(const char* rel) {
    const std::string romfsPath = std::string("romfs:/") + rel;
    for (const char* path : {rel, romfsPath.c_str()}) {
        FILE* f = std::fopen(path, "rb");
        if (!f) continue;
        if (std::fseek(f, 0, SEEK_END) != 0) {
            std::fclose(f);
            continue;
        }
        const long sz = std::ftell(f);
        std::fclose(f);
        if (sz >= 0) return sz;
    }
    return -1;
}

bool romfsShaderBundlePresent() {
    static const char* kRequired[] = {
        "shaders/sim.frag",
        "shaders/sim.comp",
#if defined(__SWITCH__)
        "shaders/sim_rules_body.glsl",
#endif
        "shaders/sim_common.glsl",
        "shaders/sim_ids.glsl",
        "shaders/paint.frag",
        "shaders/palette_lookup.frag",
        "shaders/upscale.frag",
        "shaders/fullscreen.vert",
        "shaders/bloom_bright.frag",
        "shaders/bloom_blur.frag",
        "shaders/bloom_composite.frag",
        "shaders/ui_quad.vert",
        "shaders/ui_quad.frag",
    };
    for (const char* path : kRequired) {
        if (!romfsFilePresent(path)) return false;
    }
    return true;
}

} // namespace
#endif

static int run_app() {
#if defined(__SWITCH__)
    resetLaunchLogTimer();
    logStage("---- NXSand launch ----");
    logStagef("build: switch-sim-log-v11 %s %s", __DATE__, __TIME__);
    logStage("switch: default sim backend=fragment");
    logStage("stage: romfsInit");
    if (R_FAILED(romfsInit())) {
        switchShowFatal("NXSand: romfsInit failed",
                        "Rebuild the .nro (make / build-native.ps1).");
        return 1;
    }
    logStage("stage: chdir romfs:/");
    if (chdir("romfs:/") != 0) {
        romfsExit();
        switchShowFatal("NXSand: romfs chdir failed",
                        "Embedded files missing from the NRO.");
        return 1;
    }
    logStagef("romfs: sim.frag=%ld sim_rules_body.glsl=%ld sim_common.glsl=%ld",
              romfsFileSizeBytes("shaders/sim.frag"),
              romfsFileSizeBytes("shaders/sim_rules_body.glsl"),
              romfsFileSizeBytes("shaders/sim_common.glsl"));
    logStage("stage: shader bundle present?");
    if (!romfsShaderBundlePresent()) {
        romfsExit();
        switchShowFatal("NXSand: shaders missing from romfs",
                        "Run: make (copies shaders/ into the NRO).\n"
                        "Expected the full desktop shader bundle (sim, paint, palette, bloom, UI).");
        return 1;
    }
    logStage("stage: save directory");
    if (!nx::ensureSaveStorageAtLaunch()) {
        logStage("save dir: sdmc:/switch/nxsand/ unavailable (check microSD)");
    }
    logStage("stage: app.init()");
#endif

    nx::App app;
    if (!app.init()) {
#if !defined(__SWITCH__)
        if (!app.initError.empty()) {
            std::cerr << "NXSand init failed: " << app.initError << "\n";
        } else {
            std::cerr << "NXSand init failed\n";
        }
#endif
#if defined(__SWITCH__)
        std::string detail = app.initError;
        if (detail.empty()) {
            detail = "Unknown init failure (SDL / OpenGL / shaders).";
        }
        logStagef("app.init failed: %s", detail.c_str());
        app.shutdown();
        romfsExit();
        switchShowFatal("NXSand failed to start", detail.c_str());
#endif
        return 1;
    }
#if defined(__SWITCH__)
    logStage("stage: entering frame loop");
#endif

    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();
#if defined(__SWITCH__)
    while (!app.input.quitRequested && appletMainLoop()) {
#else
    while (!app.input.quitRequested) {
#endif
        auto t1 = clock::now();
        double dt = std::chrono::duration<double>(t1 - t0).count();
        t0 = t1;
        if (dt > 0.25) dt = 0.25;
        app.frame(dt);
    }

    app.shutdown();

#if defined(__SWITCH__)
    logStage("stage: clean exit");
    closeLaunchLog();
    romfsExit();
#endif
    return 0;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
#if defined(__SWITCH__)
    // Wrap run_app so an uncaught exception (e.g. from std::bad_alloc, JSON
    // parsing inside a static init, freetype, etc.) doesn't terminate the
    // process invisibly with a return-to-hbmenu and no fatal screen.
    try {
        return run_app();
    } catch (const std::exception& e) {
        logStagef("uncaught std::exception: %s", e.what());
        switchShowFatal("NXSand: uncaught C++ exception", e.what());
        return 1;
    } catch (...) {
        logStage("uncaught non-std exception");
        switchShowFatal("NXSand: uncaught exception",
                        "Non-std exception escaped main().");
        return 1;
    }
#else
    return run_app();
#endif
}
