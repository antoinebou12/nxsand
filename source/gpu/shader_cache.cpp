#include "shader_cache.hpp"
#include "../save/save_paths.hpp"
#if defined(__SWITCH__)
#include "../platform/launch_log.hpp"
#endif
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

namespace nx {

namespace {

constexpr char kMagic[4] = {'N', 'X', 'S', 'C'};
constexpr uint32_t kHeaderVersion = 1;

struct ShaderCacheHeader {
    char magic[4];
    uint32_t version;
    uint64_t keyHash;
    uint32_t binaryFormat;
    uint32_t payloadSize;
};

constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

uint64_t fnv1a64(uint64_t h, const std::string& s) {
    for (unsigned char c : s) {
        h ^= static_cast<uint64_t>(c);
        h *= kFnvPrime;
    }
    return h;
}

uint64_t fnv1a64(uint64_t h, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        h ^= static_cast<uint64_t>((v >> (i * 8)) & 0xffu);
        h *= kFnvPrime;
    }
    return h;
}

#if !defined(__SWITCH__)
bool envEnabledDefaultOn(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !v[0]) return true;
    if (v[0] == '0' && v[1] == '\0') return false;
    if (std::strcmp(v, "false") == 0 || std::strcmp(v, "FALSE") == 0) return false;
    if (std::strcmp(v, "off") == 0 || std::strcmp(v, "OFF") == 0) return false;
    if (std::strcmp(v, "no") == 0 || std::strcmp(v, "NO") == 0) return false;
    return true;
}
#endif

bool bootLogEnabled() {
    const char* v = std::getenv("NXSAND_BOOT_LOG");
    if (!v || !v[0]) v = std::getenv("NXENGINE_BOOT_LOG");
    if (!v || !v[0]) return false;
    if (v[0] == '0' && v[1] == '\0') return false;
    if (std::strcmp(v, "false") == 0 || std::strcmp(v, "FALSE") == 0) return false;
    if (std::strcmp(v, "off") == 0 || std::strcmp(v, "OFF") == 0) return false;
    if (std::strcmp(v, "no") == 0 || std::strcmp(v, "NO") == 0) return false;
    return true;
}

void cacheLog(const char* msg) {
    if (!bootLogEnabled()) return;
    std::cerr << "[shader_cache] " << msg << "\n";
}

struct GpuFingerprint {
    std::string renderer;
    std::string version;
    uint32_t binaryFormat = 0;
    bool supported = false;
};

GpuFingerprint queryGpuFingerprint() {
    GpuFingerprint fp{};
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    fp.renderer = renderer ? renderer : "?";
    fp.version = version ? version : "?";

#if !defined(__SWITCH__)
    if (!glProgramBinary || !glGetProgramBinary) return fp;
#endif

    GLint numFormats = 0;
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &numFormats);
    if (numFormats <= 0) return fp;

    std::vector<GLint> formats(static_cast<size_t>(numFormats));
    glGetIntegerv(GL_PROGRAM_BINARY_FORMATS, formats.data());
    fp.binaryFormat = static_cast<uint32_t>(formats[0]);
    fp.supported = fp.binaryFormat != 0;
    return fp;
}

#if defined(__SWITCH__)
bool isSimCacheLabel(const std::string& label) {
    return label == "sim.frag" || label == "sim.comp";
}

long romfsRulesBodySizeBytes() {
    static const char* kPaths[] = {"shaders/sim_rules_body.glsl", "romfs:/shaders/sim_rules_body.glsl"};
    for (const char* path : kPaths) {
        FILE* f = std::fopen(path, "rb");
        if (!f) continue;
        if (std::fseek(f, 0, SEEK_END) != 0) {
            std::fclose(f);
            continue;
        }
        const long sz = std::ftell(f);
        std::fclose(f);
        if (sz >= 0) return sz;
    }
    return -1;
}

uint32_t simRulesBodyRevision() {
    const long sz = romfsRulesBodySizeBytes();
    if (sz < 0) return 0;
    return static_cast<uint32_t>(sz);
}

void logShaderCacheOffOnce(const char* reason) {
    static bool logged = false;
    if (logged || !reason) return;
    logged = true;
    appendLaunchLogf("shader cache: off (%s)", reason);
}
#endif

uint64_t makeKeyHash(const ShaderCacheKey& key, const GpuFingerprint& fp) {
    uint64_t h = kFnvOffset;
    h = fnv1a64(h, key.label);
    h = fnv1a64(h, key.vertSource);
    h = fnv1a64(h, key.fragOrCompSource);
    h = fnv1a64(h, fp.renderer);
    h = fnv1a64(h, fp.version);
    h = fnv1a64(h, fp.binaryFormat);
#if defined(__SWITCH__)
    if (isSimCacheLabel(key.label)) {
        h = fnv1a64(h, simRulesBodyRevision());
    }
#endif
    return h;
}

std::string cacheDirectory() {
    return saveDirectory() + "shader_cache/";
}

std::string cacheFilePath(uint64_t keyHash) {
    char name[32];
    std::snprintf(name, sizeof(name), "%016llx.bin",
                  static_cast<unsigned long long>(keyHash));
    return cacheDirectory() + name;
}

bool readCacheFile(const std::string& path, ShaderCacheHeader& hdr, std::vector<uint8_t>& payload) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    const size_t hdrRead = std::fread(&hdr, 1, sizeof(hdr), f);
    if (hdrRead != sizeof(hdr)) {
        std::fclose(f);
        return false;
    }
    if (std::memcmp(hdr.magic, kMagic, sizeof(kMagic)) != 0 || hdr.version != kHeaderVersion) {
        std::fclose(f);
        return false;
    }
    if (hdr.payloadSize == 0) {
        std::fclose(f);
        return false;
    }

    payload.resize(hdr.payloadSize);
    const size_t bodyRead = std::fread(payload.data(), 1, payload.size(), f);
    std::fclose(f);
    return bodyRead == payload.size();
}

bool ensureCacheDirReady() {
#if defined(__SWITCH__)
    return false;
#else
    return ensureSaveDirectoryReady() && ensureDirectoryExists(cacheDirectory());
#endif
}

std::vector<std::pair<ShaderCacheKey, GLuint>>& pendingSaves() {
    static std::vector<std::pair<ShaderCacheKey, GLuint>> queue;
    return queue;
}

#if !defined(__SWITCH__)
bool gpuProgramBinaryCacheSupported() {
    return queryGpuFingerprint().supported;
}
#endif

} // namespace

bool shaderProgramBinaryCacheSupported() {
#if defined(__SWITCH__)
    return false;
#else
    return gpuProgramBinaryCacheSupported();
#endif
}

bool shaderCacheEnabled() {
#if defined(__SWITCH__)
    return false;
#else
    return envEnabledDefaultOn("NXSAND_SHADER_CACHE");
#endif
}

GLuint tryLoadShaderProgramBinary(const ShaderCacheKey& key, ShaderCacheResult* outResult) {
    if (outResult) *outResult = ShaderCacheResult::Miss;

    if (!shaderCacheEnabled()) {
#if !defined(__SWITCH__)
        cacheLog((key.label + " cache disabled").c_str());
#endif
        if (outResult) *outResult = ShaderCacheResult::Disabled;
        return 0;
    }

    const GpuFingerprint fp = queryGpuFingerprint();
    if (!fp.supported) {
        cacheLog("binary programs unsupported on this GL context");
#if defined(__SWITCH__)
        if (isSimCacheLabel(key.label)) {
            logShaderCacheOffOnce("program binaries unsupported on this GL context");
        }
#endif
        if (outResult) *outResult = ShaderCacheResult::Disabled;
        return 0;
    }

    const uint64_t keyHash = makeKeyHash(key, fp);
    const std::string path = cacheFilePath(keyHash);

    ShaderCacheHeader hdr{};
    std::vector<uint8_t> payload;
    if (!readCacheFile(path, hdr, payload)) {
        cacheLog((key.label + " miss").c_str());
#if defined(__SWITCH__)
        if (isSimCacheLabel(key.label)) {
            appendLaunchLogf("sim cache miss: %s", key.label.c_str());
        }
#endif
        if (outResult) *outResult = ShaderCacheResult::Miss;
        return 0;
    }
    if (hdr.keyHash != keyHash || hdr.binaryFormat != fp.binaryFormat) {
        cacheLog((key.label + " stale header").c_str());
#if defined(__SWITCH__)
        if (isSimCacheLabel(key.label)) {
            appendLaunchLogf("sim cache load failed: %s (stale header)", key.label.c_str());
        }
#endif
        if (outResult) *outResult = ShaderCacheResult::Failed;
        return 0;
    }

    GLuint program = glCreateProgram();
    glProgramBinary(program, static_cast<GLenum>(hdr.binaryFormat), payload.data(),
                  static_cast<GLsizei>(payload.size()));

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        cacheLog((key.label + " binary load failed, recompiling").c_str());
#if defined(__SWITCH__)
        if (isSimCacheLabel(key.label)) {
            appendLaunchLogf("sim cache load failed: %s (recompiling)", key.label.c_str());
        }
#endif
        glDeleteProgram(program);
        std::remove(path.c_str());
        if (outResult) *outResult = ShaderCacheResult::Failed;
        return 0;
    }

    cacheLog((key.label + " hit").c_str());
#if defined(__SWITCH__)
    if (isSimCacheLabel(key.label)) {
        appendLaunchLogf("sim cache hit: %s rev=%u", key.label.c_str(), simRulesBodyRevision());
    }
#endif
    if (outResult) *outResult = ShaderCacheResult::Hit;
    return program;
}

void saveShaderProgramBinary(const ShaderCacheKey& key, GLuint program) {
    if (!shaderCacheEnabled()) {
        cacheLog((key.label + " not saved: cache disabled").c_str());
        return;
    }
    if (program == 0) {
        cacheLog((key.label + " not saved: no program").c_str());
        return;
    }

    const GpuFingerprint fp = queryGpuFingerprint();
    if (!fp.supported) {
        cacheLog((key.label + " not saved: binary programs unsupported").c_str());
        return;
    }

    GLint binLen = 0;
    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binLen);
    if (binLen <= 0) {
        cacheLog((key.label + " not saved: empty program binary").c_str());
#if defined(__SWITCH__)
        if (isSimCacheLabel(key.label)) {
            appendLaunchLogf("sim cache save failed: %s (empty binary)", key.label.c_str());
        }
#endif
        return;
    }

    const auto t0 = std::chrono::steady_clock::now();
    std::vector<uint8_t> payload(static_cast<size_t>(binLen));
    GLenum format = static_cast<GLenum>(fp.binaryFormat);
    GLsizei written = 0;
    glGetProgramBinary(program, binLen, &written, &format, payload.data());
    if (bootLogEnabled()) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - t0)
                            .count();
        cacheLog((key.label + " glGetProgramBinary " + std::to_string(ms) + "ms").c_str());
    }
    if (written <= 0) {
        cacheLog((key.label + " not saved: glGetProgramBinary returned no data").c_str());
#if defined(__SWITCH__)
        if (isSimCacheLabel(key.label)) {
            appendLaunchLogf("sim cache save failed: %s (empty binary)", key.label.c_str());
        }
#endif
        return;
    }
    payload.resize(static_cast<size_t>(written));

    if (!ensureCacheDirReady()) {
        cacheLog((key.label + " not saved: cache directory unavailable").c_str());
        return;
    }

    const uint64_t keyHash = makeKeyHash(key, fp);
    ShaderCacheHeader hdr{};
    std::memcpy(hdr.magic, kMagic, sizeof(kMagic));
    hdr.version = kHeaderVersion;
    hdr.keyHash = keyHash;
    hdr.binaryFormat = static_cast<uint32_t>(format);
    hdr.payloadSize = static_cast<uint32_t>(payload.size());

    std::string fileData;
    fileData.reserve(sizeof(hdr) + payload.size());
    fileData.append(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    fileData.append(reinterpret_cast<const char*>(payload.data()), payload.size());

    const std::string path = cacheFilePath(keyHash);
    if (atomicWriteFile(path, fileData)) {
        cacheLog((key.label + " saved").c_str());
#if defined(__SWITCH__)
        if (isSimCacheLabel(key.label)) {
            appendLaunchLogf("sim cache saved: %s rev=%u", key.label.c_str(), simRulesBodyRevision());
        }
#endif
    } else {
        cacheLog((key.label + " not saved: write failed").c_str());
#if defined(__SWITCH__)
        if (isSimCacheLabel(key.label)) {
            appendLaunchLogf("sim cache save failed: %s (write failed)", key.label.c_str());
        }
#endif
    }
}

void queueShaderProgramBinarySave(const ShaderCacheKey& key, GLuint program) {
    if (!shaderCacheEnabled() || program == 0) return;
    pendingSaves().emplace_back(key, program);
}

void flushPendingShaderCacheSaves() {
    auto& queue = pendingSaves();
    if (queue.empty()) return;
    if (bootLogEnabled()) {
        cacheLog(("flushing " + std::to_string(queue.size()) + " pending save(s)").c_str());
    }
    for (const auto& entry : queue) {
        saveShaderProgramBinary(entry.first, entry.second);
    }
    queue.clear();
}

void clearPendingShaderCacheSaves() {
    pendingSaves().clear();
}

} // namespace nx
