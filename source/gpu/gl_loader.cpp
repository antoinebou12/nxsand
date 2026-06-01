#include "gl_loader.hpp"

namespace nx::gl {

bool load_gl_functions() {
#if defined(NX_DESKTOP)
    if (!gladLoadGLES2(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
        return false;
    }
    return true;
#else
    return true;
#endif
}

} // namespace nx::gl
