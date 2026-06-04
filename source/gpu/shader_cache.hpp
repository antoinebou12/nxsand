#pragma once
#include "gl_loader.hpp"
#include <cstdint>
#include <string>

namespace nx {

struct ShaderCacheKey {
    std::string label;
    std::string vertSource;
    std::string fragOrCompSource;
};

enum class ShaderCacheResult {
    Disabled,
    Miss,
    Failed,
    Hit,
};

/// Desktop: honors NXSAND_SHADER_CACHE (default on). Switch: always false (no SD shader_cache/).
bool shaderCacheEnabled();

/// GLES program-binary cache supported on this platform (false on Switch).
bool shaderProgramBinaryCacheSupported();

/// Load a linked program from disk cache. Returns program id on hit, 0 otherwise.
GLuint tryLoadShaderProgramBinary(const ShaderCacheKey& key, ShaderCacheResult* outResult);

/// Persist a successfully linked program immediately (used by flush).
void saveShaderProgramBinary(const ShaderCacheKey& key, GLuint program);

/// Queue disk write until flushPendingShaderCacheSaves (avoids blocking sim init on glGetProgramBinary).
void queueShaderProgramBinarySave(const ShaderCacheKey& key, GLuint program);

void flushPendingShaderCacheSaves();
void clearPendingShaderCacheSaves();

} // namespace nx
