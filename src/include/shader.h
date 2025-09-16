#pragma once

#include <GL/glew.h>
#include <string>
#include <iostream>

class Shader {
private:
    GLuint programID;
    
public:
    Shader(const std::string& vertexSource, const std::string& fragmentSource);
    ~Shader();
    
    void use() const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, float x, float y) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setInt(const std::string& name, int value) const;
    
    GLuint getProgram() const { return programID; }
    
private:
    GLuint compileShader(const std::string& source, GLenum shaderType);
    void checkCompileErrors(GLuint shader, const std::string& type);
};
