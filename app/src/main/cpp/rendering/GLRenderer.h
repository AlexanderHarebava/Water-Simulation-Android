#pragma once

#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <vector>
#include <cstdint>
#include "Shader.h"

class GLRenderer {
public:
    void init(int width, int height);

    void render3D(const std::vector<std::uint8_t>& volume, int texW, int texH, int texD);
    void renderTexture3D(GLuint texture, int texW, int texH, int texD);

    // Новый рендер частиц MPM
    void renderParticles(GLuint particleSSBO, int numParticles, int gridN);
    bool isInitialized() const { return _initialized; }

    void resize(int width, int height);
    void setCamera(float yaw, float pitch);
    void setRenderScale(float scale);
    void destroy();

    // Euler settings
    void setWaterColor(float r, float g, float b) {
        _waterR = r;
        _waterG = g;
        _waterB = b;
    }

    void setAbsorption(float value) {
        _absorption = value;
    }

    void setSpecular(float value) {
        _specular = value;
    }

    void setVolSteps(int steps) {
        _volSteps = steps;
    }

    void setMpmRenderStyle(int style) {
        _mpmRenderStyle = style;
    }

    void setMpmWaterColor(float r, float g, float b) {
        _mpmWaterR = r;
        _mpmWaterG = g;
        _mpmWaterB = b;
    }

    void setMpmWaterOpacity(float value) {
        _mpmWaterOpacity = value;
    }

    void setMpmParticleRadius(float value) {
        _mpmParticleRadius = value;
    }

private:
    Shader _shader;
    Shader _simpleShader;
    bool _initialized = false;

    Shader _bgShader;
    Shader _sphereShader;

    GLuint _vao = 0;
    GLuint _vbo = 0;
    GLuint _texture3D = 0;
    GLuint _fbo = 0;
    GLuint _fboTexture = 0;
    GLuint _particleVao = 0;

    int _width = 0;
    int _height = 0;
    int _renderW = 0;
    int _renderH = 0;

    float _renderScale = 0.5f;

    int _texW = 0;
    int _texH = 0;
    int _texD = 0;

    float _yaw   = -1.0f;
    float _pitch = -0.54f;
    float _dist  = 8.0f;

    // Euler params
    float _waterR = 0.069f;
    float _waterG = 0.181f;
    float _waterB = 0.314f;
    float _absorption = 3.4f;
    float _specular = 0.8f;
    int _volSteps = 32;

    // MPM params
    int _mpmRenderStyle = 0;
    float _mpmWaterR = 0.12f;
    float _mpmWaterG = 0.60f;
    float _mpmWaterB = 0.95f;
    float _mpmWaterOpacity = 0.85f;
    float _mpmParticleRadius = 0.62f;

    void setupFBO();
    void drawScene(int targetW, int targetH);
    void computeCameraMatrices(float* view, float* proj, int w, int h);
};