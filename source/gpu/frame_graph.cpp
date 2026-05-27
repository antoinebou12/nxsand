#include "frame_graph.hpp"
#include "../game/app.hpp"

namespace nx {

void FrameGraph::runPlayFrame(App& app, double /*dtSec*/) {
    // Reserved hook: explicit pass ordering lives in App::tickPlay / renderFrame.
    (void)app;
}

} // namespace nx
