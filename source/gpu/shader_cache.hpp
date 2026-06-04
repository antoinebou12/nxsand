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

/// Desktop: honors NXSAND_SHADER_CACHE (default on). Switch: opt-in via
/// NXSAND_SWITCH_SHADER_CACHE=1 and only saves target-driver sim binaries.
bool shaderCacheEnabled();

/// GLES program-binary cache supported on the active GL context.
bool shaderProgramBinaryCacheSupported();

/// Log renderer/version/program-binary support once for Switch launch diagnostics.
void logShaderProgramBinarySupport();
void disableShaderCacheLoadsForSession();

/// Pure cache-key helper used by tests to guard source/rules fingerprinting.
uint64_t shaderCacheKeyHashForTest(const ShaderCacheKey& key,
                                   const std::string& renderer,
                                   const std::string& version,
                                   uint32_t binaryFormat,
                                   uint32_t rulesBodySize,
                                   uint64_t rulesBodyHash);

/// Load a linked program from disk cache. Returns program id on hit, 0 otherwise.
GLuint tryLoadShaderProgramBinary(const ShaderCacheKey& key, ShaderCacheResult* outResult);

/// Persist a successfully linked program immediately (used by flush).
void saveShaderProgramBinary(const ShaderCacheKey& key, GLuint program);

/// Queue disk write until flushPendingShaderCacheSaves (avoids blocking sim init on glGetProgramBinary).
void queueShaderProgramBinarySave(const ShaderCacheKey& key, GLuint program);

void flushPendingShaderCacheSaves();
void clearPendingShaderCacheSaves();

} // namespace nx
