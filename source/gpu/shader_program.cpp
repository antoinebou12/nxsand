#include "shader_program.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

namespace nx {

static std::string g_shader_diag;

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

std::string ShaderProgram::readFile(const std::string& path) {
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

GLuint ShaderProgram::compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        const char* kind = "shader";
        if (type == GL_VERTEX_SHADER) kind = "vertex";
        else if (type == GL_FRAGMENT_SHADER) kind = "fragment";
        else if (type == GL_COMPUTE_SHADER) kind = "compute";
        logShader(s, kind);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool ShaderProgram::linkCompute(GLuint cs) {
    program = glCreateProgram();
    glAttachShader(program, cs);
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        logProgram(program);
        glDeleteProgram(program);
        program = 0;
        return false;
    }
    return true;
}

bool ShaderProgram::link(GLuint vs, GLuint fs) {
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
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
    std::string vsrc = readFile(vertPath);
    std::string fsrc = readFile(fragPath);
    if (vsrc.empty() || fsrc.empty()) {
        g_shader_diag = "shader read failed: " + vertPath + " / " + fragPath;
        std::cerr << g_shader_diag << "\n";
        return false;
    }
    GLuint vs = compile(GL_VERTEX_SHADER, vsrc.c_str());
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc.c_str());
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        if (g_shader_diag.empty()) {
            g_shader_diag = "shader compile failed: " + vertPath + " / " + fragPath;
        }
        return false;
    }
    bool ok = link(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok && g_shader_diag.empty()) {
        g_shader_diag = "shader link failed: " + vertPath + " / " + fragPath;
    }
    return ok;
}

bool ShaderProgram::loadComputeFromFile(const std::string& compPath) {
    g_shader_diag.clear();
    std::string src = readFile(compPath);
    if (src.empty()) {
        g_shader_diag = "shader read failed: " + compPath;
        std::cerr << g_shader_diag << "\n";
        return false;
    }
    GLuint cs = compile(GL_COMPUTE_SHADER, src.c_str());
    if (!cs) {
        if (g_shader_diag.empty()) {
            g_shader_diag = "compute compile failed: " + compPath;
        }
        return false;
    }
    bool ok = linkCompute(cs);
    glDeleteShader(cs);
    if (!ok && g_shader_diag.empty()) {
        g_shader_diag = "compute link failed: " + compPath;
    }
    return ok;
}

void ShaderProgram::use() const {
    glUseProgram(program);
}

GLint ShaderProgram::uniformLocation(const char* name) const {
    return glGetUniformLocation(program, name);
}

} // namespace nx
