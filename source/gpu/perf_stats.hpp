#pragma once
#include <chrono>
#include <cstdint>

namespace nx {

// CPU frame timing for on-screen overlay (Switch + desktop).
struct PerfStats {
    double frameMs = 0.0;
    double simMs = 0.0;
    double worldRenderMs = 0.0;
    double uiMs = 0.0;
    double paintMs = 0.0;
    double presentMs = 0.0;
    float fps = 0.0f;
    int simW = 0;
    int simH = 0;
    int substeps = 1;
    int phasesPerStep = 4;
    int fragmentPasses = 0;
    int paintDirtyW = 0;
    int paintDirtyH = 0;
    int brushCommandCount = 0;
    bool paintHeld = false;
    bool eraseHeld = false;
    int brushMaterial = 0;
    int brushRadius = 0;
    int activeTileMode = 0;
    int activeTileCount = 0;
    bool activeTileFallback = false;
    bool simSleeping = false;
    bool gridHasMatter = false;
    const char* presetLabel = "Balanced";
    const char* simBackendLabel = "Fragment";
    double lastUnaccountedMs = 0.0;
    double lastFrameMs = 0.0;
    double lastSimMs = 0.0;
    double lastPaintMs = 0.0;
    double lastWorldRenderMs = 0.0;
    double lastUiMs = 0.0;
    double lastPresentMs = 0.0;
    double lastOtherMs = 0.0;
    int lastFragmentPasses = 0;
    int lastBrushCommandCount = 0;
    int lastPaintDirtyW = 0;
    int lastPaintDirtyH = 0;
    bool lastPaintHeld = false;
    bool lastEraseHeld = false;
    int lastBrushMaterial = 0;
    int lastBrushRadius = 0;
    int lastActiveTileMode = 0;
    int lastActiveTileCount = 0;
    bool lastActiveTileFallback = false;

    void beginFrame() {
        frameStart_ = Clock::now();
        simMs = 0.0;
        paintMs = 0.0;
        presentMs = 0.0;
        uiMs = 0.0;
        worldRenderMs = 0.0;
        fragmentPasses = 0;
        brushCommandCount = 0;
        paintDirtyW = 0;
        paintDirtyH = 0;
        paintHeld = false;
        eraseHeld = false;
        activeTileCount = 0;
        activeTileFallback = false;
    }
    void beginSim() { simStart_ = Clock::now(); }
    void endSim() {
        simMs += std::chrono::duration<double, std::milli>(Clock::now() - simStart_).count();
    }
    void beginPaint() { paintStart_ = Clock::now(); }
    void endPaint() {
        paintMs += std::chrono::duration<double, std::milli>(Clock::now() - paintStart_).count();
    }
    void beginWorldRender() { worldRenderStart_ = Clock::now(); }
    void endWorldRender() {
        worldRenderMs =
            std::chrono::duration<double, std::milli>(Clock::now() - worldRenderStart_).count();
    }
    void beginUi() { uiStart_ = Clock::now(); }
    void endUi() {
        uiMs += std::chrono::duration<double, std::milli>(Clock::now() - uiStart_).count();
    }
    void beginPresent() { presentStart_ = Clock::now(); }
    void endPresent() {
        presentMs += std::chrono::duration<double, std::milli>(Clock::now() - presentStart_).count();
    }
    void endFrame() {
        frameMs = std::chrono::duration<double, std::milli>(Clock::now() - frameStart_).count();
        ++framesAccum_;
        fpsAccum_ += static_cast<float>(1000.0 / (frameMs > 0.001 ? frameMs : 0.001));
        if (framesAccum_ >= 30) {
            fps = fpsAccum_ / static_cast<float>(framesAccum_);
            framesAccum_ = 0;
            fpsAccum_ = 0.f;
        }
        const double accounted = simMs + paintMs + worldRenderMs + uiMs + presentMs;
        lastUnaccountedMs = frameMs > accounted ? frameMs - accounted : 0.0;
        lastFrameMs = frameMs;
        lastSimMs = simMs;
        lastPaintMs = paintMs;
        lastWorldRenderMs = worldRenderMs;
        lastUiMs = uiMs;
        lastPresentMs = presentMs;
        lastOtherMs = lastUnaccountedMs;
        lastFragmentPasses = fragmentPasses;
        lastBrushCommandCount = brushCommandCount;
        lastPaintDirtyW = paintDirtyW;
        lastPaintDirtyH = paintDirtyH;
        lastPaintHeld = paintHeld;
        lastEraseHeld = eraseHeld;
        lastBrushMaterial = brushMaterial;
        lastBrushRadius = brushRadius;
        lastActiveTileMode = activeTileMode;
        lastActiveTileCount = activeTileCount;
        lastActiveTileFallback = activeTileFallback;
    }

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    TimePoint frameStart_{};
    TimePoint simStart_{};
    TimePoint paintStart_{};
    TimePoint worldRenderStart_{};
    TimePoint uiStart_{};
    TimePoint presentStart_{};
    int framesAccum_ = 0;
    float fpsAccum_ = 0.f;
};

} // namespace nx
