#include "shader_program.hpp"
#include "shader_cache.hpp"
#include "gl_loader.hpp"
#if defined(__SWITCH__)
#include "../platform/launch_log.hpp"
#endif
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

namespace nx {

static std::string g_shader_diag;

namespace {

#if defined(__SWITCH__)
constexpr uint64_t kCompileTimeoutMs = 600000;
#else
constexpr uint64_t kCompileTimeoutMs = 120000;
#endif
constexpr int kCompilePollSleepMs = 16;
#if defined(__SWITCH__)
constexpr uint64_t kLinkLogIntervalMs = 15000;
constexpr uint64_t kBlockingLinkHeartbeatMs = 15000;

struct SwitchLinkHeartbeat {
    std::atomic<bool> stop{false};
    std::thread worker;
    const char* label = nullptr;

    void start(const char* linkLabel) {
        stop.store(false);
        label = linkLabel;
        worker = std::thread([this]() {
            auto t0 = std::chrono::steady_clock::now();
            uint64_t lastLogSec = 0;
            while (!stop.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                if (stop.load(std::memory_order_relaxed)) break;
                const auto elapsedMs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0)
                        .count());
                const uint64_t elapsedSec = elapsedMs / 1000u;
                if (elapsedMs >= kBlockingLinkHeartbeatMs &&
                    elapsedSec - lastLogSec >= (kBlockingLinkHeartbeatMs / 1000u)) {
                    lastLogSec = elapsedSec;
                    if (label && label[0]) {
                        appendLaunchLogf("link wait: %s %llus", label,
                                         static_cast<unsigned long long>(elapsedSec));
                    }
                }
            }
        });
    }

    void finish() {
        stop.store(true, std::memory_order_relaxed);
        if (worker.joinable()) worker.join();
    }
};
#endif

ShaderCompileProgressFn g_progressFn = nullptr;
void* g_progressUser = nullptr;
std::string g_compileStage = "Compiling shader...";
std::chrono::steady_clock::time_point g_compileStart{};
#if defined(__SWITCH__)
std::string g_activeLinkLabel;
std::vector<std::string> g_lastShaderIncludes;
#endif

uint64_t compileElapsedMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - g_compileStart)
            .count());
}

bool parallelCompileEnabled() {
    return gl::parallel_shader_compile_available();
}

static bool isSimShaderLabel(const std::string& label) {
    return label == "sim.frag" || label == "sim.comp";
}

#if defined(__SWITCH__)
static bool isSimShaderName(const char* name) {
    if (!name || !name[0]) return false;
    return std::strstr(name, "sim.frag") != nullptr || std::strstr(name, "sim.comp") != nullptr;
}

#endif

} // namespace

void setShaderCompileProgress(ShaderCompileProgressFn fn, void* user) {
    g_progressFn = fn;
    g_progressUser = user;
}

void setShaderCompileStage(const char* stage) {
    g_compileStage = (stage && stage[0]) ? stage : "Compiling shader...";
    g_compileStart = std::chrono::steady_clock::now();
}

static void reportCompileProgress() {
    if (!g_progressFn) return;
    g_progressFn(g_compileStage.c_str(), compileElapsedMs(), g_progressUser);
}

static bool compileTimedOut() {
    return compileElapsedMs() > kCompileTimeoutMs;
}

namespace {

static const char* linkLabelFromStage() {
    if (g_compileStage.empty()) return nullptr;
    const char* p = g_compileStage.c_str();
    if (std::strncmp(p, "Compiling ", 10) == 0) {
        p += 10;
    } else if (std::strncmp(p, "Linking ", 8) == 0) {
        p += 8;
    } else {
        return nullptr;
    }
    static char buf[48];
    size_t i = 0;
    while (p[i] && i + 1 < sizeof(buf)) {
        const unsigned char c = static_cast<unsigned char>(p[i]);
        if (c == ' ' || c == '(') break;
        if (c >= 0x80) break;
        buf[i] = static_cast<char>(c);
        ++i;
    }
    buf[i] = '\0';
    return buf[0] ? buf : nullptr;
}

void pollShaderCompile(GLuint shader) {
#if defined(__SWITCH__)
    uint64_t lastLogMs = 0;
    const char* compileLabel =
        !g_activeLinkLabel.empty() ? g_activeLinkLabel.c_str() : linkLabelFromStage();
#endif
    for (;;) {
        GLint done = GL_FALSE;
        if (parallelCompileEnabled()) {
            glGetShaderiv(shader, GL_COMPLETION_STATUS_KHR, &done);
        } else {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &done);
        }
        if (done == GL_TRUE) break;
        if (compileTimedOut()) {
            setShaderDiagnostics(std::string("Shader compile timed out (") + g_compileStage + ")");
#if defined(__SWITCH__)
            if (compileLabel && compileLabel[0]) {
                appendLaunchLogf("compile timed out: %s %llus", compileLabel,
                                 static_cast<unsigned long long>(compileElapsedMs() / 1000u));
            }
#endif
            return;
        }
        reportCompileProgress();
#if defined(__SWITCH__)
        const uint64_t elapsed = compileElapsedMs();
        if (compileLabel && compileLabel[0] && elapsed - lastLogMs >= kLinkLogIntervalMs) {
            lastLogMs = elapsed;
            appendLaunchLogf("compile wait: %s %llus", compileLabel,
                             static_cast<unsigned long long>(elapsed / 1000u));
        }
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(kCompilePollSleepMs));
    }
}

bool shaderCompileTimedOut() {
    return g_shader_diag.find("timed out") != std::string::npos;
}

bool pollProgramLink(GLuint prog, const char* linkLabel) {
#if defined(__SWITCH__)
    uint64_t lastLogMs = 0;
    if (linkLabel && linkLabel[0]) {
        appendLaunchLogf("link start: %s", linkLabel);
    }
    SwitchLinkHeartbeat heartbeat;
    heartbeat.start(linkLabel);
#endif
    reportCompileProgress();
    glLinkProgram(prog);
#if defined(__SWITCH__)
    heartbeat.finish();
#endif
    for (;;) {
        GLint done = GL_FALSE;
        if (parallelCompileEnabled()) {
            glGetProgramiv(prog, GL_COMPLETION_STATUS_KHR, &done);
        } else {
            glGetProgramiv(prog, GL_LINK_STATUS, &done);
        }
        if (done == GL_TRUE) break;
        if (compileTimedOut()) {
            setShaderDiagnostics(std::string("Shader link timed out (") + g_compileStage + ")");
#if defined(__SWITCH__)
            if (linkLabel && linkLabel[0]) {
                appendLaunchLogf("link timed out: %s %llus", linkLabel,
                                 static_cast<unsigned long long>(compileElapsedMs() / 1000u));
            }
#endif
            return false;
        }
        reportCompileProgress();
#if defined(__SWITCH__)
        const uint64_t elapsed = compileElapsedMs();
        if (linkLabel && linkLabel[0] && elapsed - lastLogMs >= kLinkLogIntervalMs) {
            lastLogMs = elapsed;
            appendLaunchLogf("link wait: %s %llus", linkLabel,
                             static_cast<unsigned long long>(elapsed / 1000u));
        }
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(kCompilePollSleepMs));
    }
    return true;
}

} // namespace

void prepareGlContextForShaderLink() {
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

#if defined(__SWITCH__)
static bool isBootShaderDiagLabel(const std::string& label) {
    return label == "sim.frag" || label == "sim.comp" || label == "paint.frag";
}

static size_t countSourceLines(const std::string& src) {
    if (src.empty()) return 0;
    size_t lines = 1;
    for (char c : src) {
        if (c == '\n') ++lines;
    }
    return lines;
}

static void logBootShaderSource(const std::string& label, const std::string& resolved) {
    if (!isBootShaderDiagLabel(label)) return;
    appendLaunchLogf("shader source: %s resolved=%zu bytes lines~%zu", label.c_str(),
                     resolved.size(), countSourceLines(resolved));
}
#endif

static bool shouldUseBinaryRetrievableHint(const std::string& label, ShaderCacheResult cacheResult) {
#if defined(__SWITCH__)
    (void)cacheResult;
    return isSimShaderLabel(label) && shaderCacheEnabled() && shaderProgramBinaryCacheSupported();
#else
    if (!shaderCacheEnabled()) return false;
    if (isSimShaderLabel(label)) {
        (void)cacheResult;
        return false;
    }
    return true;
#endif
}

static bool shouldQueueProgramBinarySave(const std::string& label, bool binaryRetrievable) {
#if defined(__SWITCH__)
    return binaryRetrievable && isSimShaderLabel(label) && shaderCacheEnabled() &&
           shaderProgramBinaryCacheSupported();
#else
    if (binaryRetrievable) return true;
    (void)label;
    return false;
#endif
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

static std::string readShaderResolvedImpl(const std::string& path, int depth,
                                          std::vector<std::string>* includes) {
    if (depth > 8) return {};
    if (depth == 0 && includes) includes->clear();
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
                if (includes) includes->push_back(inc);
                out += readShaderResolvedImpl(dir + inc, depth + 1, includes);
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

static std::string readShaderResolved(const std::string& path) {
    return readShaderResolvedImpl(path, 0, nullptr);
}

#if defined(__SWITCH__)
static void logBootShaderIncludes(const std::string& label, const std::vector<std::string>& inc) {
    if (!isBootShaderDiagLabel(label) || inc.empty()) return;
    std::string msg = "shader includes: " + label;
    for (const std::string& p : inc) {
        msg += " + ";
        msg += p;
    }
    appendLaunchLog(msg.c_str());
}
#endif

std::string ShaderProgram::readFile(const std::string& path) {
    return readShaderResolved(path);
}

GLuint ShaderProgram::compile(GLenum type, const char* src) {
    const char* kind = "shader";
    if (type == GL_VERTEX_SHADER) kind = "vertex";
    else if (type == GL_FRAGMENT_SHADER) kind = "fragment";
    else if (type == GL_COMPUTE_SHADER) kind = "compute";
    const auto t0 = std::chrono::steady_clock::now();
#if defined(__SWITCH__)
    const char* compileLabel =
        !g_activeLinkLabel.empty() ? g_activeLinkLabel.c_str() : linkLabelFromStage();
    if (compileLabel && compileLabel[0] && isBootShaderDiagLabel(std::string(compileLabel))) {
        appendLaunchLogf("compile begin: %s %s", compileLabel, kind);
    }
#endif
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    reportCompileProgress();

    glCompileShader(s);

    pollShaderCompile(s);
    if (shaderCompileTimedOut()) {
        glDeleteShader(s);
        return 0;
    }

    reportCompileProgress();

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    const uint64_t compileMs = elapsedMs(t0);
    bootLog(std::string(kind) + " compile " + std::to_string(compileMs) + "ms " +
            (ok ? "ok" : "failed"));
#if defined(__SWITCH__)
    if (ok) {
        const char* stageLabel = linkLabelFromStage();
        if (stageLabel && isBootShaderDiagLabel(stageLabel)) {
            appendLaunchLogf("compile ok: %s %llums", kind,
                             static_cast<unsigned long long>(compileMs));
        }
    }
#endif
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

#if defined(__SWITCH__)
static void reportLinkProgressIfNeeded() {
    if (!g_progressFn || g_compileStage.empty()) return;
    if (g_compileStage.compare(0, 10, "Compiling ") != 0) return;
    const char* name = g_compileStage.c_str() + 10;
    char buf[96];
    const bool simShader = isSimShaderName(name);
    if (simShader) {
        std::snprintf(buf, sizeof(buf), "Linking %s", name);
    } else {
        std::snprintf(buf, sizeof(buf), "Linking %.*s", 48, name);
    }
    setShaderCompileStage(buf);
    reportCompileProgress();
}
#endif

bool ShaderProgram::linkCompute(GLuint cs, bool binaryRetrievable) {
    const auto t0 = std::chrono::steady_clock::now();
    prepareGlContextForShaderLink();
    program = glCreateProgram();
    if (binaryRetrievable) {
        glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
    }
    glAttachShader(program, cs);
#if defined(__SWITCH__)
    reportLinkProgressIfNeeded();
#endif
    const char* linkLabel = !g_activeLinkLabel.empty() ? g_activeLinkLabel.c_str()
                                                       : linkLabelFromStage();
    bootLog("glLinkProgram (compute) …");
    if (!pollProgramLink(program, linkLabel)) {
        glDeleteProgram(program);
        program = 0;
        return false;
    }
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    bootLog(std::string("compute program link ") + std::to_string(elapsedMs(t0)) + "ms " +
            (ok ? "ok" : "failed"));
#if defined(__SWITCH__)
    if (linkLabel && linkLabel[0]) {
        appendLaunchLogf("link %s: %s %llus", ok ? "ok" : "failed", linkLabel,
                         static_cast<unsigned long long>(elapsedMs(t0) / 1000u));
    }
#endif
    if (!ok) {
        logProgram(program);
        glDeleteProgram(program);
        program = 0;
        return false;
    }
    return true;
}

bool ShaderProgram::link(GLuint vs, GLuint fs, bool binaryRetrievable) {
    const auto t0 = std::chrono::steady_clock::now();
    prepareGlContextForShaderLink();
    program = glCreateProgram();
    if (binaryRetrievable) {
        glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
    }
    glAttachShader(program, vs);
    glAttachShader(program, fs);
#if defined(__SWITCH__)
    reportLinkProgressIfNeeded();
#endif
    const char* linkLabel = !g_activeLinkLabel.empty() ? g_activeLinkLabel.c_str()
                                                       : linkLabelFromStage();
    bootLog("glLinkProgram …");
    if (!pollProgramLink(program, linkLabel)) {
        glDeleteProgram(program);
        program = 0;
        return false;
    }
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    bootLog(std::string("program link ") + std::to_string(elapsedMs(t0)) + "ms " +
            (ok ? "ok" : "failed"));
#if defined(__SWITCH__)
    if (linkLabel && linkLabel[0]) {
        appendLaunchLogf("link %s: %s %llus", ok ? "ok" : "failed", linkLabel,
                         static_cast<unsigned long long>(elapsedMs(t0) / 1000u));
    }
#endif
    if (!ok) {
        logProgram(program);
        glDeleteProgram(program);
        program = 0;
        return false;
    }
    return true;
}

bool ShaderProgram::loadFromFiles(const std::string& vertPath, const std::string& fragPath) {
    g_shader_diag.clear();
    const ShaderCacheKey cacheKeyEarly{cacheLabelFromPath(fragPath), {}, {}};
#if defined(__SWITCH__)
    g_activeLinkLabel = cacheKeyEarly.label;
    {
        char stage[96];
        std::snprintf(stage, sizeof(stage), "Compiling %s...", cacheKeyEarly.label.c_str());
        setShaderCompileStage(stage);
    }
#endif
    std::string vsrc = readFileRaw(vertPath);
#if defined(__SWITCH__)
    g_lastShaderIncludes.clear();
    std::string fsrc = readShaderResolvedImpl(fragPath, 0, &g_lastShaderIncludes);
#else
    std::string fsrc = readShaderResolved(fragPath);
#endif
    if (vsrc.empty() || fsrc.empty()) {
        g_shader_diag = "shader read failed: " + vertPath + " / " + fragPath;
        std::cerr << g_shader_diag << "\n";
#if defined(__SWITCH__)
        g_activeLinkLabel.clear();
#endif
        return false;
    }

    const ShaderCacheKey cacheKey{cacheLabelFromPath(fragPath), vsrc, fsrc};
    ShaderCacheResult cacheResult = ShaderCacheResult::Miss;
#if defined(__SWITCH__)
    logBootShaderSource(cacheKey.label, fsrc);
    logBootShaderIncludes(cacheKey.label, g_lastShaderIncludes);
#endif
#if defined(__SWITCH__)
    const bool allowCacheLoad = isSimShaderLabel(cacheKey.label);
#else
    const bool allowCacheLoad = true;
#endif
    if (allowCacheLoad) {
        if (GLuint cached = tryLoadShaderProgramBinary(cacheKey, &cacheResult)) {
            program = cached;
            return true;
        }
    }
#if !defined(__SWITCH__)
    bootLog(cacheKey.label + " compiling after cache " + cacheResultName(cacheResult));
#else
    if (allowCacheLoad) {
        appendLaunchLogf("%s compiling after cache %s", cacheKey.label.c_str(),
                         cacheResultName(cacheResult));
    }
#endif
    const bool binaryRetrievable = shouldUseBinaryRetrievableHint(cacheKey.label, cacheResult);
    if (!binaryRetrievable && !shouldQueueProgramBinarySave(cacheKey.label, false)) {
        bootLog(cacheKey.label + " binary save skipped for faster first link");
    }
    g_compileStart = std::chrono::steady_clock::now();

    const auto t0 = std::chrono::steady_clock::now();
    GLuint vs = compile(GL_VERTEX_SHADER, vsrc.c_str());
    if (!vs) {
        std::cerr << "shader compile failed (vertex): " << vertPath << "\n";
#if defined(__SWITCH__)
        g_activeLinkLabel.clear();
#endif
        return false;
    }
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc.c_str());
    if (!fs) {
        std::cerr << "shader compile failed (fragment): " << fragPath << "\n";
        glDeleteShader(vs);
        if (g_shader_diag.empty()) {
            g_shader_diag = "shader compile failed: " + fragPath;
        }
#if defined(__SWITCH__)
        g_activeLinkLabel.clear();
#endif
        return false;
    }
    bool ok = link(vs, fs, binaryRetrievable);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok && g_shader_diag.empty()) {
        g_shader_diag = "shader link failed: " + vertPath + " / " + fragPath;
    }
    const uint64_t totalMs = elapsedMs(t0);
    bootLog(cacheKey.label + " load " + std::to_string(totalMs) + "ms " + (ok ? "ok" : "failed"));
#if defined(__SWITCH__)
    if (isBootShaderDiagLabel(cacheKey.label)) {
        appendLaunchLogf("shader pipeline total: %s %llums", cacheKey.label.c_str(),
                         static_cast<unsigned long long>(totalMs));
    }
#endif
    if (ok && shouldQueueProgramBinarySave(cacheKey.label, binaryRetrievable)) {
        queueShaderProgramBinarySave(cacheKey, program);
    }
#if defined(__SWITCH__)
    g_activeLinkLabel.clear();
#endif
    return ok;
}

bool ShaderProgram::loadComputeFromFile(const std::string& compPath) {
    g_shader_diag.clear();
#if defined(__SWITCH__)
    g_activeLinkLabel = cacheLabelFromPath(compPath);
    {
        char stage[96];
        std::snprintf(stage, sizeof(stage), "Compiling %s...", g_activeLinkLabel.c_str());
        setShaderCompileStage(stage);
    }
#endif
#if defined(__SWITCH__)
    g_lastShaderIncludes.clear();
    std::string src = readShaderResolvedImpl(compPath, 0, &g_lastShaderIncludes);
#else
    std::string src = readShaderResolved(compPath);
#endif
    if (src.empty()) {
        g_shader_diag = "shader read failed: " + compPath;
        std::cerr << g_shader_diag << "\n";
        return false;
    }

    const ShaderCacheKey cacheKey{cacheLabelFromPath(compPath), {}, src};
    ShaderCacheResult cacheResult = ShaderCacheResult::Miss;
#if defined(__SWITCH__)
    logBootShaderSource(cacheKey.label, src);
    logBootShaderIncludes(cacheKey.label, g_lastShaderIncludes);
#endif
#if defined(__SWITCH__)
    const bool allowCacheLoad = isSimShaderLabel(cacheKey.label);
#else
    const bool allowCacheLoad = true;
#endif
    if (allowCacheLoad) {
        if (GLuint cached = tryLoadShaderProgramBinary(cacheKey, &cacheResult)) {
            program = cached;
            return true;
        }
    }
#if !defined(__SWITCH__)
    bootLog(cacheKey.label + " compiling after cache " + cacheResultName(cacheResult));
#else
    if (allowCacheLoad) {
        appendLaunchLogf("%s compiling after cache %s", cacheKey.label.c_str(),
                         cacheResultName(cacheResult));
    }
#endif
    const bool binaryRetrievable = shouldUseBinaryRetrievableHint(cacheKey.label, cacheResult);
    if (!binaryRetrievable && !shouldQueueProgramBinarySave(cacheKey.label, false)) {
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
    const uint64_t totalMs = elapsedMs(t0);
    bootLog(cacheKey.label + " load " + std::to_string(totalMs) + "ms " + (ok ? "ok" : "failed"));
#if defined(__SWITCH__)
    if (isBootShaderDiagLabel(cacheKey.label)) {
        appendLaunchLogf("shader pipeline total: %s %llums", cacheKey.label.c_str(),
                         static_cast<unsigned long long>(totalMs));
    }
#endif
    if (ok && shouldQueueProgramBinarySave(cacheKey.label, binaryRetrievable)) {
        queueShaderProgramBinarySave(cacheKey, program);
    }
#if defined(__SWITCH__)
    g_activeLinkLabel.clear();
#endif
    return ok;
}

void ShaderProgram::use() const {
    glUseProgram(program);
}

GLint ShaderProgram::uniformLocation(const char* name) const {
    return glGetUniformLocation(program, name);
}

} // namespace nx
