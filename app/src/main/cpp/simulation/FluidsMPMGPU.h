/* MIT License

Copyright (c) 2026 Alexander Harebava

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/

#pragma once
#include <GLES3/gl31.h>
#include <cstdint>
#include <algorithm>
#include <cmath>


enum MpmScenario : int {
    DAM_BREAK   = 0,
    WATER_DROP  = 1,
    DUAL_WAVE   = 2
};

class FluidsMPMGPU {
public:
    void init(int gridX, int gridY, int gridZ, int maxParticles, int w, int h);
    void step();
    void setGravity(float gx, float gy, float gz);
    void setPointer(float cx, float cy, float cz,
                    float fx, float fy, float fz, float radius);
    void releasePointer();
    void setSimulationSpeed(float speed) {
        _simSpeed = std::clamp(speed, 0.3f, 1.0f);
    }
    void setWaterAmount(float a);
    void setScenario(int scenario);
    void setPhysicsParams(float stiffness, float restDensity, float viscosity, float dt);
    void setTouchStrength(float s);
    void setParticleCount(int count);
    GLuint densityTexture() const { return _texDensity; }
    int textureWidth()  const { return _gx; }
    int textureHeight() const { return _gy; }
    int textureDepth()  const { return _gz; }
    GLuint particleBuffer() const { return _ssboParticles; }
    int particleCount() const { return _numParticles; }
    void destroy();

private:
    void compileShader(GLuint& program, const char* source);
    void initParticles();
    void clearCells();
    bool _particlesDirty = false;
    void clearDensityGridBuf();
    void dispatchP2G1();
    void dispatchP2G2();
    void dispatchUpdateGrid();
    void dispatchG2P();
    void dispatchP2GDensity();
    void dispatchCastDensity();
    void uploadDensityTexture();
    float _simSpeed = 1.0f;
    int _gx = 0, _gy = 0, _gz = 0;
    int _gridCount = 0;
    int _numParticles = 0, _maxParticles = 0;

    GLuint _progClear = 0, _progClearInt = 0, _progP2G1 = 0, _progP2G2 = 0,
            _progUpdate = 0, _progG2P = 0, _progP2GD = 0, _progCast = 0;
    GLuint _ssboParticles = 0, _ssboCells = 0, _ssboDensities = 0,
            _ssboDensityGrid = 0, _ssboCasted = 0;
    GLuint _texDensity = 0;

    float _gravity[3] = {0.0f, -9.8f, 0.0f};


    float _fixedM     = 1e7f;
    float _stiffness  = 50.0f;
    float _restDensity = 3.0f;
    float _viscosity  = 0.1f;
    float _dt         = 0.35f;
    float _waterAmount = 1.0f;
    bool  _initialized = false;

    float _ptrCell[3]   = {-1, -1, -1};
    float _ptrForce[3]  = {0, 0, 0};
    float _ptrRadius    = 14.0f;
    float _ptrStrength  = 1.2f;
    bool  _ptrActive    = false;

    int   _scenario     = DAM_BREAK;
};
