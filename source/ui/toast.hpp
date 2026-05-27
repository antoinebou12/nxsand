#pragma once
#include <string>

namespace nx {

struct Toast {
    std::string message;
    float secondsLeft = 0.f;

    void show(const std::string& m, float seconds) {
        message = m;
        secondsLeft = seconds;
    }
    void tick(double dt) {
        secondsLeft -= float(dt);
        if (secondsLeft <= 0.f) {
            secondsLeft = 0.f;
            message.clear();
        }
    }
    bool active() const { return secondsLeft > 0.f && !message.empty(); }
};

class App;
class RenderPipeline;
void drawToastSolid(RenderPipeline& r, App& app);

} // namespace nx
