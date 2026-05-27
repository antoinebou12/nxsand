#include "game/app.hpp"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <exception>
#include <string>

#if defined(__SWITCH__)
#include <SDL2/SDL.h>
#include <switch.h>
#include <sys/stat.h>
#endif

#if defined(__SWITCH__)
namespace {

// Append a line to sdmc:/switch/nxsand/launch.log. Safe to call at any stage:
// if SDMC isn't mounted yet we try once, and if anything fails we silently no-op.
// This is the only diagnostic the user has when the NRO instant-bounces back to
// hbmenu without ever showing the fatal console screen.
void logStage(const char* msg) {
    static bool s_tried_mount = false;
    if (!s_tried_mount) {
        s_tried_mount = true;
        struct stat st{};
        if (stat("sdmc:/switch", &st) != 0) {
            fsdevMountSdmc();
        }
        mkdir("sdmc:/switch", 0777);
        mkdir("sdmc:/switch/nxsand", 0777);
    }
    FILE* f = std::fopen("sdmc:/switch/nxsand/launch.log", "a");
    if (!f) return;
    std::fprintf(f, "%s\n", msg ? msg : "(null)");
    std::fclose(f);
}

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

bool romfsShaderBundlePresent() {
    return romfsFilePresent("shaders/sim.frag") && romfsFilePresent("shaders/paint.frag");
}

} // namespace
#endif

static int run_app() {
#if defined(__SWITCH__)
    logStage("---- NXSand launch ----");
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
    logStage("stage: shader bundle present?");
    if (!romfsShaderBundlePresent()) {
        romfsExit();
        switchShowFatal("NXSand: shaders missing from romfs",
                        "Run: make (copies shaders/ into the NRO).\n"
                        "Expected shaders/sim.frag and paint.frag inside the NRO.");
        return 1;
    }
    logStage("stage: app.init()");
#endif

    nx::App app;
    if (!app.init()) {
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
    romfsExit();
    logStage("stage: clean exit");
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
