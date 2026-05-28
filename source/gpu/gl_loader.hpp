#pragma once
// OpenGL ES 3.0+ via Mesa on Switch; GLESv2 on desktop.
#include <cstdio>
#include <string>
#include <SDL2/SDL.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>

namespace nx::gl {

inline bool load_gl_functions() {
    return true;
}

inline bool parse_es_version(int& major, int& minor) {
    major = 0;
    minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major > 0 || minor > 0) {
        return true;
    }
    const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (!ver) return false;
    const std::string v(ver);
    const auto pos = v.find("OpenGL ES ");
    if (pos == std::string::npos) return false;
    if (std::sscanf(v.c_str() + pos, "OpenGL ES %d.%d", &major, &minor) < 2) {
        return false;
    }
    return true;
}

// GLES 3.1 compute capability probe (Switch + desktop).
inline bool check_compute_support(std::string* outError, int* outMaxInvocations = nullptr) {
    int major = 0;
    int minor = 0;
    if (!parse_es_version(major, minor)) {
        if (outError) *outError = "OpenGL ES: could not read GL version";
        return false;
    }
    const bool es31 = (major > 3) || (major == 3 && minor >= 1);
    if (!es31) {
        const char* ver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
        bool glsl31 = false;
        if (glsl) {
            const std::string gs(glsl);
            glsl31 = gs.find("3.10") != std::string::npos || gs.find("3.20") != std::string::npos ||
                     gs.find("3.30") != std::string::npos;
        }
        if (!glsl31) {
            if (outError) {
                *outError = std::string("OpenGL ES 3.1+ required (GL ") +
                                       std::to_string(major) + "." + std::to_string(minor);
                if (ver) {
                    *outError += ", ";
                    *outError += ver;
                }
                *outError += ")";
            }
            return false;
        }
    }

    GLint maxInv = 0;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxInv);
    if (outMaxInvocations) *outMaxInvocations = maxInv;
    if (maxInv < 256) {
        if (outError) {
            *outError = "GPU compute work group too small (MAX_COMPUTE_WORK_GROUP_INVOCATIONS=" +
                        std::to_string(maxInv) + ", need >= 256 for 16x16 groups)";
        }
        return false;
    }
    GLint maxImageUnits = 0;
    glGetIntegerv(GL_MAX_IMAGE_UNITS, &maxImageUnits);
    if (maxImageUnits < 2) {
        if (outError) {
            *outError = "GPU compute image units too small (MAX_IMAGE_UNITS=" +
                        std::to_string(maxImageUnits) + ", need >= 2)";
        }
        return false;
    }
    return true;
}

} // namespace nx::gl
