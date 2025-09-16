#include "shader.h"
#include <sstream>

Shader::Shader(const std::string& vertexSource, const std::string& fragmentSource) {
    // 编译顶点着色器
    GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    
    // 编译片段着色器
    GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    
    // 创建着色器程序
    programID = glCreateProgram();
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, fragmentShader);
    glLinkProgram(programID);
    
    // 检查链接错误
    checkCompileErrors(programID, "PROGRAM");
    
    // 删除着色器对象，它们已经链接到程序中了
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader() {
    if (programID != 0) {
        glDeleteProgram(programID);
    }
}

void Shader::use() const {
    glUseProgram(programID);
}

void Shader::setFloat(const std::string& name, float value) const {
    GLint location = glGetUniformLocation(programID, name.c_str());
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void Shader::setVec2(const std::string& name, float x, float y) const {
    GLint location = glGetUniformLocation(programID, name.c_str());
    if (location != -1) {
        glUniform2f(location, x, y);
    }
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const {
    GLint location = glGetUniformLocation(programID, name.c_str());
    if (location != -1) {
        glUniform3f(location, x, y, z);
    }
}

void Shader::setInt(const std::string& name, int value) const {
    GLint location = glGetUniformLocation(programID, name.c_str());
    if (location != -1) {
        glUniform1i(location, value);
    }
}

GLuint Shader::compileShader(const std::string& source, GLenum shaderType) {
    GLuint shader = glCreateShader(shaderType);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    // 检查编译错误
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
            std::cerr << "着色器编译错误 (" << type << "): " << infoLog << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            std::cerr << "着色器程序链接错误: " << infoLog << std::endl;
        }
    }
}
