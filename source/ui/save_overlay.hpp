#pragma once
#include <string>

namespace nx {

class FontAtlas;
class RenderPipeline;

class SaveOverlay {
public:
    void begin(const char* label);
    void end();
    void tick(float dtSec);
    void draw(RenderPipeline& r, FontAtlas& font, int screenW, int screenH, float uiScale);
    bool active() const;
    bool readyForIo() const;

private:
    std::string label_;
    float animTime_ = 0.f;
    int showFrames_ = 0;
    bool active_ = false;
};

} // namespace nx
