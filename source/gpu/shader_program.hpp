#pragma once
#include <cstdint>
#include <string>
#include "gl_loader.hpp"

namespace nx {

// Last load/compile/link failure (for Switch fatal screen).
void setShaderDiagnostics(const std::string& msg);
const char* lastShaderDiagnostics();

using ShaderCompileProgressFn = void (*)(const char* stage, uint64_t elapsedMs, void* user);

void setShaderCompileProgress(ShaderCompileProgressFn fn, void* user);
void setShaderCompileStage(const char* stage);

class ShaderProgram {
public:
    GLuint program = 0;

    ShaderProgram() = default;
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    ShaderProgram(ShaderProgram&& o) noexcept;
    ShaderProgram& operator=(ShaderProgram&& o) noexcept;

    bool loadFromFiles(const std::string& vertPath, const std::string& fragPath);
    bool loadComputeFromFile(const std::string& compPath);
    void use() const;
    GLint uniformLocation(const char* name) const;

private:
    static std::string readFile(const std::string& path);
    static GLuint compile(GLenum type, const char* src);
    bool link(GLuint vs, GLuint fs, bool binaryRetrievable);
    bool linkCompute(GLuint cs, bool binaryRetrievable);
};

} // namespace nx
