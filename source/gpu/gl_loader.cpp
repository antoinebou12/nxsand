#include "gl_loader.hpp"
#include <cstring>

namespace nx::gl {

namespace {

#if defined(__SWITCH__)
bool g_parallel_shader_compile = false;

bool extensionListed(const char* name) {
    if (!name || !name[0]) return false;
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    if (count > 0) {
        for (GLint i = 0; i < count; ++i) {
            const char* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
            if (ext && std::strcmp(ext, name) == 0) return true;
        }
        return false;
    }
    const char* extStr = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    if (!extStr) return false;
    const size_t nameLen = std::strlen(name);
    const char* p = extStr;
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;
        const char* start = p;
        while (*p && *p != ' ') ++p;
        const size_t len = static_cast<size_t>(p - start);
        if (len == nameLen && std::strncmp(start, name, nameLen) == 0) return true;
    }
    return false;
}
#endif

} // namespace

bool load_gl_functions() {
#if defined(NX_DESKTOP)
    if (!gladLoadGLES2(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
        return false;
    }
    return true;
#elif defined(__SWITCH__)
    g_parallel_shader_compile = extensionListed("GL_KHR_parallel_shader_compile");
    return true;
#else
    return true;
#endif
}

bool parallel_shader_compile_available() {
#if defined(NX_DESKTOP)
    return GLAD_GL_KHR_parallel_shader_compile != 0;
#elif defined(__SWITCH__)
    return g_parallel_shader_compile;
#else
    return false;
#endif
}

} // namespace nx::gl
