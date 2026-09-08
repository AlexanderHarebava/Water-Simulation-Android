
#pragma once
#include <GLES3/gl3.h>
#include <string>

class Shader {
public:
    GLuint program = 0;

    void init(const char* vertSrc, const char* fragSrc);
    void use() const;

    void set1f(const char* name, float v) const;
    void set2f(const char* name, float x, float y) const;
    void set3f(const char* name, float x, float y, float z) const;
    void set1i(const char* name, int v) const;

    void destroy();

private:
    GLuint compile(GLenum type, const char* src);
};
