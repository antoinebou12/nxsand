#include "shader_program.hpp"
#include "shader_cache.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <vector>

namespace nx {

static std::string g_shader_diag;

namespace {

constexpr uint64_t kCompileTimeoutMs = 120000;
constexpr int kCompilePollSleepMs = 16;

ShaderCompileProgressFn g_progressFn = nullptr;
void* g_progressUser = nullptr;
const char* g_compileStage = "Compiling shader…";
std::chrono::steady_clock::time_point g_compileStart{};

uint64_t compileElapsedMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - g_compileStart)
            .count());
}

} // namespace

void setShaderCompileProgress(ShaderCompileProgressFn fn, void* user) {
    g_progressFn = fn;
    g_progressUser = user;
}

void setShaderCompileStage(const char* stage) {
    g_compileStage = (stage && stage[0]) ? stage : "Compiling shader…";
    g_compileStart = std::chrono::steady_clock::now();
}

static void reportCompileProgress() {
    if (!g_progressFn) return;
    g_progressFn(g_compileStage, compileElapsedMs(), g_progressUser);
}

static bool compileTimedOut() {
    return compileElapsedMs() > kCompileTimeoutMs;
}

/// Drain UI draws and reset bindings before glLinkProgram (ANGLE can hang if UI program/FBO
/// are still active — gpu_unit_tests never repaints during compile).
static void prepareGlContextForShaderLink() {
    glFlush();
    glFinish();
    glUseProgram(0);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
}

static bool bootLogEnabled() {
    const char* v = std::getenv("NXSAND_BOOT_LOG");
    if (!v || !v[0]) v = std::getenv("NXENGINE_BOOT_LOG");
    if (!v || !v[0]) return false;
    if (v[0] == '0' && v[1] == '\0') return false;
    if (std::strcmp(v, "false") == 0 || std::strcmp(v, "FALSE") == 0) return false;
    if (std::strcmp(v, "off") == 0 || std::strcmp(v, "OFF") == 0) return false;
    if (std::strcmp(v, "no") == 0 || std::strcmp(v, "NO") == 0) return false;
    return true;
}

static uint64_t elapsedMs(std::chrono::steady_clock::time_point start) {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
            .count());
}

static void bootLog(const std::string& msg) {
    if (!bootLogEnabled()) return;
    std::cerr << "[shader] " << msg << "\n";
}

static const char* cacheResultName(ShaderCacheResult result) {
    switch (result) {
        case ShaderCacheResult::Disabled: return "disabled";
        case ShaderCacheResult::Miss: return "miss";
        case ShaderCacheResult::Failed: return "failed";
        case ShaderCacheResult::Hit: return "hit";
    }
    return "unknown";
}

static bool shouldSaveProgramBinary(const std::string& label) {
    if (!shaderCacheEnabled()) return false;
    // On ANGLE, asking for retrievable binaries can make the large sim shaders spend
    // minutes in glLinkProgram. Existing binary hits are still loaded before compile.
    return label != "sim.frag" && label != "sim.comp";
}

void setShaderDiagnostics(const std::string& msg) {
    g_shader_diag = msg;
}

const char* lastShaderDiagnostics() {
    return g_shader_diag.c_str();
}

static void logShader(GLuint id, const char* kind) {
    GLint len = 0;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
    if (len <= 1) return;
    std::vector<char> buf(static_cast<size_t>(len));
    glGetShaderInfoLog(id, len, nullptr, buf.data());
    std::cerr << kind << " shader log:\n" << buf.data() << "\n";
    g_shader_diag += std::string(kind) + ": " + buf.data();
}

static void logProgram(GLuint id) {
    GLint len = 0;
    glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
    if (len <= 1) return;
    std::vector<char> buf(static_cast<size_t>(len));
    glGetProgramInfoLog(id, len, nullptr, buf.data());
    std::cerr << "program log:\n" << buf.data() << "\n";
    g_shader_diag += std::string("link: ") + buf.data();
}

ShaderProgram::~ShaderProgram() {
    if (program) glDeleteProgram(program);
}

ShaderProgram::ShaderProgram(ShaderProgram&& o) noexcept : program(o.program) {
    o.program = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& o) noexcept {
    if (this != &o) {
        if (program) glDeleteProgram(program);
        program = o.program;
        o.program = 0;
    }
    return *this;
}

static std::string readFileRaw(const std::string& path) {
    // fopen works reliably with romfs:/ on Switch (ifstream can fail).
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
        std::fseek(f, 0, SEEK_END);
        long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            std::string out(static_cast<size_t>(sz), '\0');
            if (std::fread(out.data(), 1, static_cast<size_t>(sz), f) == static_cast<size_t>(sz)) {
                std::fclose(f);
                return out;
            }
        }
        std::fclose(f);
    }
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::string readShaderResolved(const std::string& path, int depth = 0) {
    if (depth > 8) return {};
    std::string src = readFileRaw(path);
    if (src.empty()) return src;

    const size_t slash = path.find_last_of("/\\");
    const std::string dir = slash != std::string::npos ? path.substr(0, slash + 1) : "";

    std::string out;
    out.reserve(src.size() + 256);
    size_t lineStart = 0;
    while (lineStart <= src.size()) {
        size_t lineEnd = src.find('\n', lineStart);
        if (lineEnd == std::string::npos) lineEnd = src.size();
        const std::string line = src.substr(lineStart, lineEnd - lineStart);
        const auto incPos = line.find("#include");
        if (incPos != std::string::npos) {
            const auto q1 = line.find('"', incPos);
            const auto q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1 + 1) {
                const std::string inc = line.substr(q1 + 1, q2 - q1 - 1);
                out += readShaderResolved(dir + inc, depth + 1);
                out.push_back('\n');
                if (lineEnd < src.size()) {
                    lineStart = lineEnd + 1;
                    continue;
                }
                break;
            }
        }
        out.append(line);
        out.push_back('\n');
        if (lineEnd >= src.size()) break;
        lineStart = lineEnd + 1;
    }
    return out;
}

std::string ShaderProgram::readFile(const std::string& path) {
    return readShaderResolved(path);
}

GLuint ShaderProgram::compile(GLenum type, const char* src) {
    const char* kind = "shader";
    if (type == GL_VERTEX_SHADER) kind = "vertex";
    else if (type == GL_FRAGMENT_SHADER) kind = "fragment";
    else if (type == GL_COMPUTE_SHADER) kind = "compute";
    const auto t0 = std::chrono::steady_clock::now();
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    if (GLAD_GL_KHR_parallel_shader_compile) {
        for (;;) {
            GLint done = GL_FALSE;
            glGetShaderiv(s, GL_COMPLETION_STATUS_KHR, &done);
            if (done == GL_TRUE) break;
            if (compileTimedOut()) {
                setShaderDiagnostics(std::string("Shader compile timed out (") + g_compileStage +
                                     ")");
                glDeleteShader(s);
                return 0;
            }
            reportCompileProgress();
            std::this_thread::sleep_for(std::chrono::milliseconds(kCompilePollSleepMs));
        }
    }

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    bootLog(std::string(kind) + " compile " + std::to_string(elapsedMs(t0)) + "ms " +
            (ok ? "ok" : "failed"));
    if (!ok) {
        logShader(s, kind);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static std::string cacheLabelFromPath(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash != std::string::npos ? path.substr(slash + 1) : path;
}

bool ShaderProgram::linkCompute(GLuint cs, bool binaryRetrievable) {
    const auto t0 = std::chrono::steady_clock::now();
    ShaderCompileProgressFn savedProgress = g_progressFn;
    void* savedProgressUser = g_progressUser;
    g_progressFn = nullptr;
    g_progressUser = nullptr;
    prepareGlContextForShaderLink();
    program = glCreateProgram();
    if (binaryRetrievable) {
        glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
    }
    glAttachShader(program, cs);
    bootLog("glLinkProgram (compute) …");
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    bootLog(std::string("compute program link ") + std::to_string(elapsedMs(t0)) + "ms " +
            (ok ? "ok" : "failed"));
    if (!ok) {
        logProgram(program);
        glDeleteProgram(program);
        program = 0;
        g_progressFn = savedProgress;
        g_progressUser = savedProgressUser;
        return false;
    }
    g_progressFn = savedProgress;
    g_progressUser = savedProgressUser;
    return true;
}

bool ShaderProgram::link(GLuint vs, GLuint fs, bool binaryRetrievable) {
    const auto t0 = std::chrono::steady_clock::now();
    ShaderCompileProgressFn savedProgress = g_progressFn;
    void* savedProgressUser = g_progressUser;
    g_progressFn = nullptr;
    g_progressUser = nullptr;
    prepareGlContextForShaderLink();
    program = glCreateProgram();
    if (binaryRetrievable) {
        glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
    }
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    bootLog("glLinkProgram …");
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    bootLog(std::string("program link ") + std::to_string(elapsedMs(t0)) + "ms " +
            (ok ? "ok" : "failed"));
    if (!ok) {
        logProgram(program);
        glDeleteProgram(program);
        program = 0;
        g_progressFn = savedProgress;
        g_progressUser = savedProgressUser;
        return false;
    }
    g_progressFn = savedProgress;
    g_progressUser = savedProgressUser;
    return true;
}

bool ShaderProgram::loadFromFiles(const std::string& vertPath, const std::string& fragPath) {
    g_shader_diag.clear();
    std::string vsrc = readFileRaw(vertPath);
    std::string fsrc = readShaderResolved(fragPath);
    if (vsrc.empty() || fsrc.empty()) {
        g_shader_diag = "shader read failed: " + vertPath + " / " + fragPath;
        std::cerr << g_shader_diag << "\n";
        return false;
    }

    const ShaderCacheKey cacheKey{cacheLabelFromPath(fragPath), vsrc, fsrc};
    ShaderCacheResult cacheResult = ShaderCacheResult::Miss;
    if (GLuint cached = tryLoadShaderProgramBinary(cacheKey, &cacheResult)) {
        program = cached;
        return true;
    }
    bootLog(cacheKey.label + " compiling after cache " + cacheResultName(cacheResult));
    const bool binaryRetrievable = shouldSaveProgramBinary(cacheKey.label);
    if (!binaryRetrievable) {
        bootLog(cacheKey.label + " binary save skipped for faster first link");
    }
    g_compileStart = std::chrono::steady_clock::now();

    const auto t0 = std::chrono::steady_clock::now();
    GLuint vs = compile(GL_VERTEX_SHADER, vsrc.c_str());
    if (!vs) {
        std::cerr << "shader compile failed (vertex): " << vertPath << "\n";
        return false;
    }
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc.c_str());
    if (!fs) {
        std::cerr << "shader compile failed (fragment): " << fragPath << "\n";
        glDeleteShader(vs);
        if (g_shader_diag.empty()) {
            g_shader_diag = "shader compile failed: " + fragPath;
        }
        return false;
    }
    bool ok = link(vs, fs, binaryRetrievable);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok && g_shader_diag.empty()) {
        g_shader_diag = "shader link failed: " + vertPath + " / " + fragPath;
    }
    bootLog(cacheKey.label + " load " + std::to_string(elapsedMs(t0)) + "ms " +
            (ok ? "ok" : "failed"));
    if (ok && binaryRetrievable) queueShaderProgramBinarySave(cacheKey, program);
    return ok;
}

bool ShaderProgram::loadComputeFromFile(const std::string& compPath) {
    g_shader_diag.clear();
    std::string src = readShaderResolved(compPath);
    if (src.empty()) {
        g_shader_diag = "shader read failed: " + compPath;
        std::cerr << g_shader_diag << "\n";
        return false;
    }

    const ShaderCacheKey cacheKey{cacheLabelFromPath(compPath), {}, src};
    ShaderCacheResult cacheResult = ShaderCacheResult::Miss;
    if (GLuint cached = tryLoadShaderProgramBinary(cacheKey, &cacheResult)) {
        program = cached;
        return true;
    }
    bootLog(cacheKey.label + " compiling after cache " + cacheResultName(cacheResult));
    const bool binaryRetrievable = shouldSaveProgramBinary(cacheKey.label);
    if (!binaryRetrievable) {
        bootLog(cacheKey.label + " binary save skipped for faster first link");
    }
    g_compileStart = std::chrono::steady_clock::now();

    const auto t0 = std::chrono::steady_clock::now();
    GLuint cs = compile(GL_COMPUTE_SHADER, src.c_str());
    if (!cs) {
        if (g_shader_diag.empty()) {
            g_shader_diag = "compute compile failed: " + compPath;
        }
        return false;
    }
    bool ok = linkCompute(cs, binaryRetrievable);
    glDeleteShader(cs);
    if (!ok && g_shader_diag.empty()) {
        g_shader_diag = "compute link failed: " + compPath;
    }
    bootLog(cacheKey.label + " load " + std::to_string(elapsedMs(t0)) + "ms " +
            (ok ? "ok" : "failed"));
    if (ok && binaryRetrievable) queueShaderProgramBinarySave(cacheKey, program);
    return ok;
}

void ShaderProgram::use() const {
    glUseProgram(program);
}

GLint ShaderProgram::uniformLocation(const char* name) const {
    return glGetUniformLocation(program, name);
}

} // namespace nx
