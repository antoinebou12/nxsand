#pragma once
#include <string>

struct SDL_Window;
using SDL_GLContext = void*;

// Headless SDL2 + OpenGL ES 3.0 context for SimPipeline unit tests.
struct GpuTestGl {
    SDL_Window* window = nullptr;
    SDL_GLContext ctx = nullptr;
    bool ok = false;
    std::string shaderDir;

    bool init(int winW = 320, int winH = 240);
    void shutdown();

    static std::string findShaderDir();
};
