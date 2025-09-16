#include "shader.h"
#include <sstream>

Shader::Shader(const std::string& vertexSource, const std::string& fragmentSource) {
    GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    programID = glCreateProgram();
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, fragmentShader);
    glLinkProgram(programID);
    checkCompileErrors(programID, "PROGRAM");
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader() {
    if (programID != 0) glDeleteProgram(programID);
}

void Shader::use() const { glUseProgram(programID); }

void Shader::setFloat(const std::string& name, float value) const {
    GLint location = glGetUniformLocation(programID, name.c_str());
    if (location != -1) glUniform1f(location, value);
}

void Shader::setVec2(const std::string& name, float x, float y) const {
    GLint location = glGetUniformLocation(programID, name.c_str());
    if (location != -1) glUniform2f(location, x, y);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const {
    GLint location = glGetUniformLocation(programID, name.c_str());
    if (location != -1) glUniform3f(location, x, y, z);
}

void Shader::setInt(const std::string& name, int value) const {
    GLint location = glGetUniformLocation(programID, name.c_str());
    if (location != -1) glUniform1i(location, value);
}

GLuint Shader::compileShader(const std::string& source, GLenum shaderType) {
    GLuint shader = glCreateShader(shaderType);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    std::string type = (shaderType == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
    checkCompileErrors(shader, type);
    return shader;
}

void Shader::checkCompileErrors(GLuint shader, const std::string& type) {
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "Shader compile error (" << type << "): " << infoLog << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "Program link error: " << infoLog << std::endl;
        }
    }
}
