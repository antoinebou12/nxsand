#pragma once

namespace nx {

class RenderPipeline;

class MenuSim {
public:
    bool init();
    void shutdown();
    void tick(int animTick, bool flickerEnabled);
    void draw(RenderPipeline& r, int screenW, int screenH, float uiScale);

private:
    void seed();
    void step();
    void uploadTexture(int animTick, bool flickerEnabled);

    unsigned char grid_[64 * 36]{};
    unsigned int  pixels_[64 * 36]{};
    unsigned int  tex_ = 0;
    bool          seeded_ = false;
    bool          texAllocated_ = false;
    int           frameAccum_ = 0;
};

} // namespace nx
