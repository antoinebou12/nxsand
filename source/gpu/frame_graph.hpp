#pragma once

namespace nx {

class App;

struct FrameGraph {
    static void runPlayFrame(App& app, double dtSec);
};

} // namespace nx
