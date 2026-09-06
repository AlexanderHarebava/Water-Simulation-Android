/* Copyright 2026 Alexander Harebava

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
        limitations under the License. */


#pragma once

#include <GLES3/gl31.h>
#include <cstdint>
#include <algorithm>
#include <cmath>
#define TAG_GPU "FluidGPU"

class FluidsGPU {
public:
    void init(int N, int width, int height);
    void step();
    void setGravity(float gx, float gy, float gz);
    void setWaterAmount(float amount) {
        amount = std::clamp(amount, 0.05f, 2.0f);

        if (std::abs(amount - _waterAmount) > 1e-5f) {
            _waterAmount = amount;
            _waterAmountDirty = true;
        }
    }
    void setDamping(float damping) {
        _damping = damping;
    }

    GLuint densityTexture() const { return _texDensity; }

    int textureWidth() const { return _N; }
    int textureHeight() const { return _N; }
    int textureDepth() const { return _N; }

    void destroy();

private:
    void compileShader(GLuint& program, const char* source);
    void dispatchInitSphere();
    void dispatchBuildFaceLabels();
    void dispatchAddForces();
    void dispatchDivergence();
    void clearVelocityAndPressure();
    void resetVolumeTarget();

    float _waterAmount = 1.0f;
    bool _waterAmountDirty = false;
    void dispatchPressure();
    void dispatchApplyPressure();
    void dispatchExtrapolateVel();
    void dispatchAdvectSurface();
    void dispatchRedistancing();
    void dispatchVolume();
    void dispatchExportDensity();
    float _damping = 0.998f;

    int _N = 0;
    int _width = 0;
    int _height = 0;
    int _groups = 1;

    GLuint _programInit = 0;
    GLuint _programFaceLabels = 0;
    GLuint _programAddForces = 0;
    GLuint _programAdvectCopy = 0;
    GLuint _programAdvectSurface = 0;
    GLuint _programRdInit = 0;
    GLuint _programRdRelax = 0;
    GLuint _programVolumeSum = 0;
    GLuint _programVolumeFinal = 0;
    GLuint _programVolumeApply = 0;
    GLuint _programDivergence = 0;
    GLuint _programPressureGS = 0;
    GLuint _programApplyPressure = 0;
    GLuint _programExtrapolateVel = 0;
    GLuint _programExport = 0;

    GLuint _ssboPhi = 0;
    GLuint _ssboPhiPrev = 0;
    GLuint _ssboU = 0;
    GLuint _ssboV = 0;
    GLuint _ssboW = 0;
    GLuint _ssboLabelsU = 0;
    GLuint _ssboLabelsV = 0;
    GLuint _ssboLabelsW = 0;
    GLuint _ssboDivergence = 0;
    GLuint _ssboPressure = 0;
    GLuint _ssboPartialSums = 0;
    GLuint _ssboVolume = 0;
    GLuint _ssboDensityOut = 0;
    GLuint _texDensity = 0;

    float _gravityX = 0.0f;
    float _gravityY = -2.0f;
    float _gravityZ = 0.0f;

    bool _initialized = false;
};
