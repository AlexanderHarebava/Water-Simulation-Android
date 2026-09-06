
#include "Shader.h"
#include <android/log.h>
#define TAG "FluidShader"

GLuint Shader::compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint success;
    glGetShaderiv(s, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[2048];
        glGetShaderInfoLog(s, 2048, nullptr, log);
        const char* typeName = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        __android_log_print(ANDROID_LOG_ERROR, TAG, "%s shader compile error:\n%s", typeName, log);
        return 0;
    }
    return s;
}

void Shader::init(const char* vertSrc, const char* fragSrc) {
    GLuint v = compile(GL_VERTEX_SHADER, vertSrc);
    if (v == 0) { program = 0; return; }

    GLuint f = compile(GL_FRAGMENT_SHADER, fragSrc);
    if (f == 0) { glDeleteShader(v); program = 0; return; }

    program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[2048];
        glGetProgramInfoLog(program, 2048, nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Program link error:\n%s", log);
        glDeleteProgram(program);
        program = 0;
    } else {
        __android_log_print(ANDROID_LOG_INFO, TAG, "Shader program linked OK: %d", program);
    }

    glDeleteShader(v);
    glDeleteShader(f);
}



void Shader::use() const {
    if (program) glUseProgram(program);
}

void Shader::set1f(const char* name, float v) const {
    glUniform1f(glGetUniformLocation(program, name), v);
}

void Shader::set2f(const char* name, float x, float y) const {
    glUniform2f(glGetUniformLocation(program, name), x, y);
}

void Shader::set3f(const char* name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(program, name), x, y, z);
}

void Shader::set1i(const char* name, int v) const {
    glUniform1i(glGetUniformLocation(program, name), v);
}

void Shader::destroy() {
    if (program) {
        glDeleteProgram(program);
        program = 0;
    }
}
