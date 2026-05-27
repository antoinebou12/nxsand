#include "gpu_test_gl.hpp"
#include <SDL2/SDL.h>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <string>

namespace {

bool fileExists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return static_cast<bool>(f);
}

} // namespace

std::string GpuTestGl::findShaderDir() {
    static const char* kCandidates[] = {"shaders", "../shaders", "../../shaders"};
    for (const char* dir : kCandidates) {
        if (fileExists(std::string(dir) + "/sim.frag") && fileExists(std::string(dir) + "/paint.frag") &&
            fileExists(std::string(dir) + "/fullscreen.vert")) {
            return dir;
        }
    }
    return "shaders";
}

bool GpuTestGl::init(int winW, int winH) {
    if (const char* skip = std::getenv("NXSAND_SKIP_GPU_TESTS")) {
        if (skip[0] == '1' || skip[0] == 'y' || skip[0] == 'Y') {
            std::fprintf(stderr, "gpu tests: skipped (NXSAND_SKIP_GPU_TESTS)\n");
            return false;
        }
    }

#if defined(__linux__)
    if (!std::getenv("SDL_VIDEODRIVER")) {
        SDL_SetHint(SDL_HINT_VIDEODRIVER, "offscreen");
    }
#endif

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "gpu tests: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    window = SDL_CreateWindow("nxsand_gpu_tests", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              winW, winH, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) {
        std::fprintf(stderr, "gpu tests: SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    ctx = SDL_GL_CreateContext(window);
    if (!ctx) {
        std::fprintf(stderr, "gpu tests: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        window = nullptr;
        SDL_Quit();
        return false;
    }

    if (SDL_GL_MakeCurrent(window, ctx) != 0) {
        std::fprintf(stderr, "gpu tests: SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
        shutdown();
        return false;
    }

    shaderDir = findShaderDir();
    if (!fileExists(shaderDir + "/sim.frag")) {
        std::fprintf(stderr, "gpu tests: shaders not found (tried %s)\n", shaderDir.c_str());
        shutdown();
        return false;
    }

    ok = true;
    return true;
}

void GpuTestGl::shutdown() {
    if (ctx) {
        SDL_GL_DeleteContext(ctx);
        ctx = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_Quit();
    }
    ok = false;
}
