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


#include "FluidsGPU.h"
#include "config.h"
#include <vector>
#include <algorithm>
#include <utility>
#include <android/log.h>

#include <cmath>
#define TAG_GPU "FluidGPU"


static const int PRESSURE_ITERS = 40;




static const char* INIT_SPHERE_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
uniform vec3 uCenter;
uniform float uRadius;

layout(std430, binding = 0) buffer PhiBuffer { float phi[]; };
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    if (p.x >= uN || p.y >= uN || p.z >= uN) return;
    vec3 d = vec3(p) - uCenter;
    float value = (length(d) < uRadius) ? -2.0 : 2.0;
    phi[p.x + uN * (p.y + uN * p.z)] = value;
}
)GLSL";

static const char* FACE_LABELS_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
layout(std430, binding = 0) buffer PhiBuffer { float phi[]; };
layout(std430, binding = 9) buffer LabelsU { uint lu[]; };
layout(std430, binding = 10) buffer LabelsV { uint lv[]; };
layout(std430, binding = 11) buffer LabelsW { uint lw[]; };
const uint EMPTY  = 1u;
const uint LIQUID = 2u;
const uint SOLID  = 4u;
int idxPhi(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
uint liquidBit(ivec3 p) {
    p = clamp(p, ivec3(0), ivec3(uN - 1));
    return (phi[idxPhi(p)] < 0.0) ? LIQUID : 0u;
}
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    if (p.x <= uN && p.y < uN && p.z < uN) {
        uint l = (liquidBit(p) | liquidBit(p - ivec3(1, 0, 0)));
        if (l == 0u) l = EMPTY;
        if (p.x == 0 || p.x == uN) l = SOLID;
        lu[p.x + (uN + 1) * (p.y + uN * p.z)] = l;
    }
    if (p.x < uN && p.y <= uN && p.z < uN) {
        uint l = (liquidBit(p) | liquidBit(p - ivec3(0, 1, 0)));
        if (l == 0u) l = EMPTY;
        if (p.y == 0 || p.y == uN) l = SOLID;
        lv[p.x + uN * (p.y + (uN + 1) * p.z)] = l;
    }
    if (p.x < uN && p.y < uN && p.z <= uN) {
        uint l = (liquidBit(p) | liquidBit(p - ivec3(0, 0, 1)));
        if (l == 0u) l = EMPTY;
        if (p.z == 0 || p.z == uN) l = SOLID;
        lw[p.x + uN * (p.y + uN * p.z)] = l;
    }
}
)GLSL";

static void clearSSBOFloat(GLuint buf, std::size_t count) {
    if (buf == 0 || count == 0) return;

    std::vector<float> zeros(count, 0.0f);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            count * sizeof(float),
            zeros.data()
    );
}

void FluidsGPU::clearVelocityAndPressure() {
    const std::size_t phiCount =
            static_cast<std::size_t>(_N) * _N * _N;

    const std::size_t uCount =
            (static_cast<std::size_t>(_N) + 1) * _N * _N;

    const std::size_t vCount =
            static_cast<std::size_t>(_N) * (static_cast<std::size_t>(_N) + 1) * _N;

    const std::size_t wCount =
            static_cast<std::size_t>(_N) * _N * (static_cast<std::size_t>(_N) + 1);

    clearSSBOFloat(_ssboU, uCount);
    clearSSBOFloat(_ssboV, vCount);
    clearSSBOFloat(_ssboW, wCount);
    clearSSBOFloat(_ssboPressure, phiCount);
    clearSSBOFloat(_ssboDivergence, phiCount);

    glMemoryBarrier(
            GL_BUFFER_UPDATE_BARRIER_BIT |
            GL_SHADER_STORAGE_BARRIER_BIT
    );
}

void FluidsGPU::resetVolumeTarget() {
    float volInit[2] = { -1.0f, 0.0f };

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboVolume);
    glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            sizeof(volInit),
            volInit
    );

    glMemoryBarrier(
            GL_BUFFER_UPDATE_BARRIER_BIT |
            GL_SHADER_STORAGE_BARRIER_BIT
    );
}

static const char* ADD_FORCES_SRC = R"GLSL(#version 310 es
precision highp float;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;

uniform int uN;
uniform vec3 uGravity;
uniform float uDamping;

layout(std430, binding = 1) buffer UBuffer { float U[]; };
layout(std430, binding = 2) buffer VBuffer { float V[]; };
layout(std430, binding = 3) buffer WBuffer { float W[]; };

layout(std430, binding = 9)  buffer LabelsU { uint lu[]; };
layout(std430, binding = 10) buffer LabelsV { uint lv[]; };
layout(std430, binding = 11) buffer LabelsW { uint lw[]; };

const uint LIQUID = 2u;

int idxU(ivec3 p) { return p.x + (uN + 1) * (p.y + uN * p.z); }
int idxV(ivec3 p) { return p.x + uN * (p.y + (uN + 1) * p.z); }
int idxW(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }

void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);

    const float forceScale = 75.0;
    const float maxVel = 5000.0;

    float damping = clamp(uDamping, 0.90, 1.0);

    if (p.x <= uN && p.y < uN && p.z < uN) {
        int id = idxU(p);

        if ((lu[id] & LIQUID) != 0u) {
            float v = U[id] + uGravity.x * forceScale;
            U[id] = clamp(v * damping, -maxVel, maxVel);
        }
    }

    if (p.x < uN && p.y <= uN && p.z < uN) {
        int id = idxV(p);

        if ((lv[id] & LIQUID) != 0u) {
            float v = V[id] + uGravity.y * forceScale;
            V[id] = clamp(v * damping, -maxVel, maxVel);
        }
    }

    if (p.x < uN && p.y < uN && p.z <= uN) {
        int id = idxW(p);

        if ((lw[id] & LIQUID) != 0u) {
            float v = W[id] + uGravity.z * forceScale;
            W[id] = clamp(v * damping, -maxVel, maxVel);
        }
    }
}
)GLSL";

static const char* ADVECT_COPY_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
layout(std430, binding = 0) buffer PhiBuffer { float phi[]; };
layout(std430, binding = 4) buffer PhiPrevBuffer { float phiPrev[]; };
int idxPhi(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    if (p.x >= uN || p.y >= uN || p.z >= uN) return;
    phiPrev[idxPhi(p)] = phi[idxPhi(p)];
}
)GLSL";

static const char* ADVECT_SURFACE_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
uniform float uDt;
layout(std430, binding = 0) buffer PhiBuffer { float phi[]; };
layout(std430, binding = 4) buffer PhiPrevBuffer { float phiPrev[]; };
layout(std430, binding = 1) buffer UBuffer { float U[]; };
layout(std430, binding = 2) buffer VBuffer { float V[]; };
layout(std430, binding = 3) buffer WBuffer { float W[]; };
int idxPhi(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
int idxU(ivec3 p) { return p.x + (uN + 1) * (p.y + uN * p.z); }
int idxV(ivec3 p) { return p.x + uN * (p.y + (uN + 1) * p.z); }
int idxW(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
float interp(vec3 pos) {
    pos = clamp(pos, vec3(0.0), vec3(float(uN - 1)));
    ivec3 i0 = ivec3(pos);
    ivec3 i1 = min(i0 + 1, ivec3(uN - 1));
    vec3 t = fract(pos);
    float c000 = phiPrev[idxPhi(ivec3(i0.x, i0.y, i0.z))];
    float c100 = phiPrev[idxPhi(ivec3(i1.x, i0.y, i0.z))];
    float c010 = phiPrev[idxPhi(ivec3(i0.x, i1.y, i0.z))];
    float c110 = phiPrev[idxPhi(ivec3(i1.x, i1.y, i0.z))];
    float c001 = phiPrev[idxPhi(ivec3(i0.x, i0.y, i1.z))];
    float c101 = phiPrev[idxPhi(ivec3(i1.x, i0.y, i1.z))];
    float c011 = phiPrev[idxPhi(ivec3(i0.x, i1.y, i1.z))];
    float c111 = phiPrev[idxPhi(ivec3(i1.x, i1.y, i1.z))];
    float c00 = mix(c000, c100, t.x);
    float c10 = mix(c010, c110, t.x);
    float c01 = mix(c001, c101, t.x);
    float c11 = mix(c011, c111, t.x);
    float c0 = mix(c00, c10, t.y);
    float c1 = mix(c01, c11, t.y);
    return mix(c0, c1, t.z);
}
vec3 getVelocity(ivec3 p) {
    float u = 0.5 * (U[idxU(p)] + U[idxU(p + ivec3(1, 0, 0))]);
    float v = 0.5 * (V[idxV(p)] + V[idxV(p + ivec3(0, 1, 0))]);
    float w = 0.5 * (W[idxW(p)] + W[idxW(p + ivec3(0, 0, 1))]);
    return vec3(u, v, w);
}
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    if (p.x >= uN || p.y >= uN || p.z >= uN) return;
    vec3 vel = getVelocity(p);
    vec3 pos = vec3(p) - vel * uDt * float(uN);
    float newValue = interp(pos);
    if (isnan(newValue)) newValue = 2.0;
    phi[idxPhi(p)] = newValue;
}
)GLSL";



static const char* RD_INIT_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
layout(std430, binding = 0) buffer PhiBuffer { float phi[]; };
layout(std430, binding = 4) buffer PhiOutBuffer { float phiOut[]; };
int idxPhi(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    if (p.x >= uN || p.y >= uN || p.z >= uN) return;
    float c = phi[idxPhi(p)];
    if (isnan(c) || isinf(c)) c = 2.0;
    c = clamp(c, -2.0, 2.0);
    bool nearInterface = false;
    if (p.x + 1 < uN) nearInterface = nearInterface || ((c < 0.0) != (phi[idxPhi(p + ivec3(1,0,0))] < 0.0));
    if (p.x > 0)      nearInterface = nearInterface || ((c < 0.0) != (phi[idxPhi(p - ivec3(1,0,0))] < 0.0));
    if (p.y + 1 < uN) nearInterface = nearInterface || ((c < 0.0) != (phi[idxPhi(p + ivec3(0,1,0))] < 0.0));
    if (p.y > 0)      nearInterface = nearInterface || ((c < 0.0) != (phi[idxPhi(p - ivec3(0,1,0))] < 0.0));
    if (p.z + 1 < uN) nearInterface = nearInterface || ((c < 0.0) != (phi[idxPhi(p + ivec3(0,0,1))] < 0.0));
    if (p.z > 0)      nearInterface = nearInterface || ((c < 0.0) != (phi[idxPhi(p - ivec3(0,0,1))] < 0.0));
    float sgn = (c <= 0.0) ? -1.0 : 1.0;
    phiOut[idxPhi(p)] = nearInterface ? c : 2.0 * sgn;
}
)GLSL";

static const char* RD_RELAX_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
layout(std430, binding = 0) buffer PhiBuffer { float phi[]; };
layout(std430, binding = 4) buffer PhiOutBuffer { float phiOut[]; };
int idxPhi(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
float getPhi(ivec3 p) {
    p = clamp(p, ivec3(0), ivec3(uN - 1));
    return phi[idxPhi(p)];
}
float gradLength(ivec3 p) {
    float c = getPhi(p);
    float gI, gJ, gK;
    if (p.x == 0)         gI = getPhi(ivec3(0, p.y, p.z)) - getPhi(ivec3(1, p.y, p.z));
    else if (p.x == uN-1) gI = getPhi(ivec3(uN-2, p.y, p.z)) - getPhi(ivec3(uN-1, p.y, p.z));
    else {
        float pr = getPhi(p + ivec3(1,0,0));
        float pl = getPhi(p - ivec3(1,0,0));
        gI = (abs(pr) < abs(pl)) ? (c - pr) : (pl - c);
    }
    if (p.y == 0)         gJ = getPhi(ivec3(p.x, 0, p.z)) - getPhi(ivec3(p.x, 1, p.z));
    else if (p.y == uN-1) gJ = getPhi(ivec3(p.x, uN-2, p.z)) - getPhi(ivec3(p.x, uN-1, p.z));
    else {
        float pr = getPhi(p + ivec3(0,1,0));
        float pl = getPhi(p - ivec3(0,1,0));
        gJ = (abs(pr) < abs(pl)) ? (c - pr) : (pl - c);
    }
    if (p.z == 0)         gK = getPhi(ivec3(p.x, p.y, 0)) - getPhi(ivec3(p.x, p.y, 1));
    else if (p.z == uN-1) gK = getPhi(ivec3(p.x, p.y, uN-2)) - getPhi(ivec3(p.x, p.y, uN-1));
    else {
        float pr = getPhi(p + ivec3(0,0,1));
        float pl = getPhi(p - ivec3(0,0,1));
        gK = (abs(pr) < abs(pl)) ? (c - pr) : (pl - c);
    }
    return sqrt(gI*gI + gJ*gJ + gK*gK);
}
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    if (p.x >= uN || p.y >= uN || p.z >= uN) return;
    int id = idxPhi(p);
    float c = phi[id];
    float dx = 1.0 / float(uN);
    float s = c / sqrt(c * c + 0.25);
    float g = gradLength(p);
    phiOut[id] = c + 0.5 * dx * (-s * (g - 1.0));
}
)GLSL";




static const char* VOLUME_SUM_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
layout(std430, binding = 0) buffer PhiBuffer { float phi[]; };
layout(std430, binding = 7) buffer PartialSums { float sums[]; };
shared float sdata[256];
int idxPhi(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    float v = 0.0;
    if (p.x < uN && p.y < uN && p.z < uN) {
        float ph = phi[idxPhi(p)];
        v = clamp(0.5 - ph * 0.5, 0.0, 1.0);
    }
    uint lid = gl_LocalInvocationIndex;
    sdata[lid] = v;
    barrier();
    for (uint s = 128u; s > 0u; s >>= 1u) {
        if (lid < s) sdata[lid] += sdata[lid + s];
        barrier();
    }
    if (lid == 0u) {
        uint gid = gl_WorkGroupID.x
                 + gl_NumWorkGroups.x * (gl_WorkGroupID.y
                 + gl_NumWorkGroups.y * gl_WorkGroupID.z);
        sums[gid] = sdata[0];
    }
}
)GLSL";


static const char* VOLUME_FINAL_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 256) in;
uniform int uNumGroups;
layout(std430, binding = 7) buffer PartialSums { float sums[]; };
layout(std430, binding = 8) buffer VolumeBuffer { float vol[]; };
shared float sdata[256];
void main() {
    uint lid = gl_LocalInvocationIndex;
    float v = 0.0;
    for (int i = int(lid); i < uNumGroups; i += 256) v += sums[i];
    sdata[lid] = v;
    barrier();
    for (uint s = 128u; s > 0u; s >>= 1u) {
        if (lid < s) sdata[lid] += sdata[lid + s];
        barrier();
    }
    if (lid == 0u) {
        float total = sdata[0];
        float target = vol[0];
        if (target < 0.0) { target = total; vol[0] = target; }
        float shift = 0.0;
        if (target > 1e-6) shift = clamp((target - total) / target * 0.5, -0.03, 0.03);
        vol[1] = shift;
    }
}
)GLSL";

static const char* VOLUME_APPLY_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
layout(std430, binding = 0) buffer PhiBuffer { float phi[]; };
layout(std430, binding = 8) buffer VolumeBuffer { float vol[]; };
int idxPhi(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    if (p.x >= uN || p.y >= uN || p.z >= uN) return;
    float shift = vol[1];
    if (abs(shift) > 1e-5) {
        int id = idxPhi(p);
        phi[id] = clamp(phi[id] - shift, -2.0, 2.0);
    }
}
)GLSL";

static const char* DIVERGENCE_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
layout(std430, binding = 1) buffer UBuffer { float U[]; };
layout(std430, binding = 2) buffer VBuffer { float V[]; };
layout(std430, binding = 3) buffer WBuffer { float W[]; };
layout(std430, binding = 12) buffer DivBuffer { float divB[]; };
int idxU(ivec3 p) { return p.x + (uN + 1) * (p.y + uN * p.z); }
int idxV(ivec3 p) { return p.x + uN * (p.y + (uN + 1) * p.z); }
int idxW(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
int idxPhi(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    if (p.x >= uN || p.y >= uN || p.z >= uN) return;
    float h = 1.0 / float(uN);
    float d = (U[idxU(p + ivec3(1, 0, 0))] - U[idxU(p)])
            + (V[idxV(p + ivec3(0, 1, 0))] - V[idxV(p)])
            + (W[idxW(p + ivec3(0, 0, 1))] - W[idxW(p)]);
    divB[idxPhi(p)] = -h * d;
}
)GLSL";



static const char* PRESSURE_GS_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
uniform int uParity;  // 0 = red, 1 = black
layout(std430, binding = 0)  buffer PhiBuffer { float phi[]; };
layout(std430, binding = 12) buffer DivBuffer { float divB[]; };
layout(std430, binding = 14) buffer PressureBuffer { float press[]; };
int idxPhi(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    if (p.x >= uN || p.y >= uN || p.z >= uN) return;
    if (((p.x + p.y + p.z) & 1) != uParity) return;
    int id = idxPhi(p);
    if (phi[id] >= 0.0) { press[id] = 0.0; return; }

    float diag = 6.0;
    if (p.x == 0)      diag -= 1.0;
    if (p.y == 0)      diag -= 1.0;
    if (p.z == 0)      diag -= 1.0;
    if (p.x == uN - 1) diag -= 1.0;
    if (p.y == uN - 1) diag -= 1.0;
    if (p.z == uN - 1) diag -= 1.0;
    float sum = 0.0;
    if (p.x > 0)      { int n = id - 1;     if (phi[n] < 0.0) sum += press[n]; }
    if (p.x < uN - 1) { int n = id + 1;     if (phi[n] < 0.0) sum += press[n]; }
    if (p.y > 0)      { int n = id - uN;    if (phi[n] < 0.0) sum += press[n]; }
    if (p.y < uN - 1) { int n = id + uN;    if (phi[n] < 0.0) sum += press[n]; }
    if (p.z > 0)      { int n = id - uN*uN; if (phi[n] < 0.0) sum += press[n]; }
    if (p.z < uN - 1) { int n = id + uN*uN; if (phi[n] < 0.0) sum += press[n]; }
    float pNew = (divB[id] + sum) / diag;
    if (isnan(pNew) || isinf(pNew)) pNew = 0.0;
    press[id] = pNew;
}
)GLSL";


static const char* APPLY_PRESSURE_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
layout(std430, binding = 1) buffer UBuffer { float U[]; };
layout(std430, binding = 2) buffer VBuffer { float V[]; };
layout(std430, binding = 3) buffer WBuffer { float W[]; };
layout(std430, binding = 14) buffer PressureBuffer { float press[]; };
layout(std430, binding = 9)  buffer LabelsU { uint lu[]; };
layout(std430, binding = 10) buffer LabelsV { uint lv[]; };
layout(std430, binding = 11) buffer LabelsW { uint lw[]; };
const uint LIQUID = 2u;
const float maxVel = 5000.0;
int idxU(ivec3 p) { return p.x + (uN + 1) * (p.y + uN * p.z); }
int idxV(ivec3 p) { return p.x + uN * (p.y + (uN + 1) * p.z); }
int idxW(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
int idxPhi(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    float N = float(uN);
    if (p.x >= 1 && p.x <= uN - 1 && p.y < uN && p.z < uN) {
        int id = idxU(p);
        if ((lu[id] & LIQUID) != 0u) {
            U[id] = clamp(U[id] - N * (press[idxPhi(p)] - press[idxPhi(p - ivec3(1,0,0))]), -maxVel, maxVel);
        }
    }
    if (p.x < uN && p.y >= 1 && p.y <= uN - 1 && p.z < uN) {
        int id = idxV(p);
        if ((lv[id] & LIQUID) != 0u) {
            V[id] = clamp(V[id] - N * (press[idxPhi(p)] - press[idxPhi(p - ivec3(0,1,0))]), -maxVel, maxVel);
        }
    }
    if (p.x < uN && p.y < uN && p.z >= 1 && p.z <= uN - 1) {
        int id = idxW(p);
        if ((lw[id] & LIQUID) != 0u) {
            W[id] = clamp(W[id] - N * (press[idxPhi(p)] - press[idxPhi(p - ivec3(0,0,1))]), -maxVel, maxVel);
        }
    }
}
)GLSL";

static const char* EXTRAPOLATE_VEL_SRC = R"GLSL(#version 310 es
precision highp float;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;
uniform int uN;
layout(std430, binding = 1) buffer UBuffer { float U[]; };
layout(std430, binding = 2) buffer VBuffer { float V[]; };
layout(std430, binding = 3) buffer WBuffer { float W[]; };
layout(std430, binding = 9)  buffer LabelsU { uint lu[]; };
layout(std430, binding = 10) buffer LabelsV { uint lv[]; };
layout(std430, binding = 11) buffer LabelsW { uint lw[]; };
const uint EMPTY  = 1u;
const uint LIQUID = 2u;
const uint SOLID  = 4u;
int idxU(ivec3 p) { return p.x + (uN + 1) * (p.y + uN * p.z); }
int idxV(ivec3 p) { return p.x + uN * (p.y + (uN + 1) * p.z); }
int idxW(ivec3 p) { return p.x + uN * (p.y + uN * p.z); }
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);


    if (p.x <= uN && p.y < uN && p.z < uN) {
        int id = idxU(p);

        if (lu[id] == EMPTY) {
            float s = 0.0; float c = 0.0;
            if (p.x + 1 <= uN) { int n=idxU(p+ivec3(1,0,0)); if((lu[n]&LIQUID)!=0u){s+=U[n];c+=1.0;} }
            if (p.x > 0)       { int n=idxU(p-ivec3(1,0,0)); if((lu[n]&LIQUID)!=0u){s+=U[n];c+=1.0;} }
            if (p.y + 1 < uN)  { int n=idxU(p+ivec3(0,1,0)); if((lu[n]&LIQUID)!=0u){s+=U[n];c+=1.0;} }
            if (p.y > 0)       { int n=idxU(p-ivec3(0,1,0)); if((lu[n]&LIQUID)!=0u){s+=U[n];c+=1.0;} }
            if (p.z + 1 < uN)  { int n=idxU(p+ivec3(0,0,1)); if((lu[n]&LIQUID)!=0u){s+=U[n];c+=1.0;} }
            if (p.z > 0)       { int n=idxU(p-ivec3(0,0,1)); if((lu[n]&LIQUID)!=0u){s+=U[n];c+=1.0;} }
            if (c > 0.0) U[id] = s / c;
        }
    }


    if (p.x < uN && p.y <= uN && p.z < uN) {
        int id = idxV(p);
        if (lv[id] == EMPTY) {
            float s = 0.0; float c = 0.0;
            if (p.x + 1 < uN)  { int n=idxV(p+ivec3(1,0,0)); if((lv[n]&LIQUID)!=0u){s+=V[n];c+=1.0;} }
            if (p.x > 0)       { int n=idxV(p-ivec3(1,0,0)); if((lv[n]&LIQUID)!=0u){s+=V[n];c+=1.0;} }
            if (p.y + 1 <= uN) { int n=idxV(p+ivec3(0,1,0)); if((lv[n]&LIQUID)!=0u){s+=V[n];c+=1.0;} }
            if (p.y > 0)       { int n=idxV(p-ivec3(0,1,0)); if((lv[n]&LIQUID)!=0u){s+=V[n];c+=1.0;} }
            if (p.z + 1 < uN)  { int n=idxV(p+ivec3(0,0,1)); if((lv[n]&LIQUID)!=0u){s+=V[n];c+=1.0;} }
            if (p.z > 0)       { int n=idxV(p-ivec3(0,0,1)); if((lv[n]&LIQUID)!=0u){s+=V[n];c+=1.0;} }
            if (c > 0.0) V[id] = s / c;
        }
    }


    if (p.x < uN && p.y < uN && p.z <= uN) {
        int id = idxW(p);
        if (lw[id] == EMPTY) {
            float s = 0.0; float c = 0.0;
            if (p.x + 1 < uN)  { int n=idxW(p+ivec3(1,0,0)); if((lw[n]&LIQUID)!=0u){s+=W[n];c+=1.0;} }
            if (p.x > 0)       { int n=idxW(p-ivec3(1,0,0)); if((lw[n]&LIQUID)!=0u){s+=W[n];c+=1.0;} }
            if (p.y + 1 < uN)  { int n=idxW(p+ivec3(0,1,0)); if((lw[n]&LIQUID)!=0u){s+=W[n];c+=1.0;} }
            if (p.y > 0)       { int n=idxW(p-ivec3(0,1,0)); if((lw[n]&LIQUID)!=0u){s+=W[n];c+=1.0;} }
            if (p.z + 1 <= uN) { int n=idxW(p+ivec3(0,0,1)); if((lw[n]&LIQUID)!=0u){s+=W[n];c+=1.0;} }
            if (p.z > 0)       { int n=idxW(p-ivec3(0,0,1)); if((lw[n]&LIQUID)!=0u){s+=W[n];c+=1.0;} }
            if (c > 0.0) W[id] = s / c;
        }
    }
}
)GLSL";

static const char* EXPORT_DENSITY_SRC = R"GLSL(#version 310 es
precision highp float;

layout(local_size_x = 256) in;

uniform int uTotal;

layout(std430, binding = 0)  buffer PhiBuffer   { float phi[]; };
layout(std430, binding = 13) buffer DensityOut  { uint packed[]; };

void main() {
    uint base = gl_GlobalInvocationID.x * 4u;

    if (base >= uint(uTotal)) return;

    uint packedValue = 0u;

    for (uint i = 0u; i < 4u; ++i) {
        uint idx = base + i;

        float value = (idx < uint(uTotal)) ? phi[int(idx)] : 2.0;
        float d = clamp(0.5 - value * 0.5, 0.0, 1.0);

        uint b = uint(d * 255.0 + 0.5);
        packedValue |= (b << (i * 8u));
    }

    packed[int(base >> 2u)] = packedValue;
}
)GLSL";





static void createSSBOZero(GLuint& buf, std::size_t bytes) {
    std::vector<std::uint8_t> zeros(bytes, 0);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, zeros.data(), GL_DYNAMIC_DRAW);
}



static void checkGlError(const char* where) {
    GLenum err = glGetError();
    if (err == GL_NO_ERROR) return;
    do {
        __android_log_print(ANDROID_LOG_ERROR, TAG_GPU,
                            "GL error 0x%04x after %s", (unsigned)err, where);
        err = glGetError();
    } while (err != GL_NO_ERROR);
}

void FluidsGPU::init(int N, int width, int height) {

    _N = N;
    _width = width;
    _height = height;
    _groups = (N + 7) / 8;
    const int zGroups = (N + 3) / 4;

    GLint maxInvocations = 0;

    GLint maxSize[3] = {0, 0, 0};

    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxInvocations);
    for (int i = 0; i < 3; ++i)
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &maxSize[i]);
    __android_log_print(ANDROID_LOG_INFO, TAG_GPU, "GL_VERSION=%s | GL_RENDERER=%s",
                        (const char*)glGetString(GL_VERSION),
                        (const char*)glGetString(GL_RENDERER));

    __android_log_print(ANDROID_LOG_INFO, TAG_GPU,
                        "GL limits: max invocations/group=%d, max group size=%dx%dx%d",
                        maxInvocations, maxSize[0], maxSize[1], maxSize[2]);
    if (maxInvocations > 0 && maxInvocations < 256) {
        __android_log_print(ANDROID_LOG_WARN, TAG_GPU,
                            "Device supports only %d invocations/group (< 256)! "
                            "Reduce local_size in compute shaders.", maxInvocations);
    }

    __android_log_print(ANDROID_LOG_INFO, TAG_GPU,
                        "Initializing FluidsGPU %dx%dx%d (groups=%d, zGroups=%d)",
                        N, N, N, _groups, zGroups);

    compileShader(_programInit, INIT_SPHERE_SRC);
    compileShader(_programFaceLabels, FACE_LABELS_SRC);
    compileShader(_programAddForces, ADD_FORCES_SRC);
    compileShader(_programAdvectCopy, ADVECT_COPY_SRC);
    compileShader(_programAdvectSurface, ADVECT_SURFACE_SRC);
    compileShader(_programRdInit, RD_INIT_SRC);
    compileShader(_programRdRelax, RD_RELAX_SRC);
    compileShader(_programVolumeSum, VOLUME_SUM_SRC);
    compileShader(_programVolumeFinal, VOLUME_FINAL_SRC);
    compileShader(_programVolumeApply, VOLUME_APPLY_SRC);
    compileShader(_programDivergence, DIVERGENCE_SRC);
    compileShader(_programPressureGS, PRESSURE_GS_SRC);
    compileShader(_programApplyPressure, APPLY_PRESSURE_SRC);
    compileShader(_programExtrapolateVel, EXTRAPOLATE_VEL_SRC);
    compileShader(_programExport, EXPORT_DENSITY_SRC);

    bool ok = _programInit && _programFaceLabels && _programAddForces
              && _programAdvectCopy && _programAdvectSurface
              && _programRdInit && _programRdRelax
              && _programVolumeSum && _programVolumeFinal && _programVolumeApply
              && _programDivergence && _programPressureGS && _programApplyPressure
              && _programExtrapolateVel && _programExport;
    if (!ok) {
        __android_log_print(ANDROID_LOG_ERROR, TAG_GPU, "Compute shader compilation FAILED!");
        return;
    }

    const std::size_t phiSize = static_cast<std::size_t>(N) * N * N * sizeof(float);

    createSSBOZero(_ssboPhi, phiSize);
    createSSBOZero(_ssboPhiPrev, phiSize);
    createSSBOZero(_ssboU, static_cast<std::size_t>(N + 1) * N * N * sizeof(float));
    createSSBOZero(_ssboV, static_cast<std::size_t>(N) * (N + 1) * N * sizeof(float));
    createSSBOZero(_ssboW, static_cast<std::size_t>(N) * N * (N + 1) * sizeof(float));
    createSSBOZero(_ssboLabelsU, static_cast<std::size_t>(N + 1) * N * N * sizeof(std::uint32_t));
    createSSBOZero(_ssboLabelsV, static_cast<std::size_t>(N) * (N + 1) * N * sizeof(std::uint32_t));
    createSSBOZero(_ssboLabelsW, static_cast<std::size_t>(N) * N * (N + 1) * sizeof(std::uint32_t));
    createSSBOZero(_ssboDivergence, phiSize);
    createSSBOZero(_ssboPressure, phiSize);
    const std::size_t texelCount = static_cast<std::size_t>(N) * N * N;
    const std::size_t packedWords = (texelCount + 3) / 4;

    createSSBOZero(_ssboDensityOut, packedWords * sizeof(std::uint32_t));



    const std::size_t totalGroups =
            static_cast<std::size_t>(_groups) * _groups * zGroups;
    createSSBOZero(_ssboPartialSums, totalGroups * sizeof(float));

    float volInit[2] = { -1.0f, 0.0f };
    glGenBuffers(1, &_ssboVolume);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboVolume);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(volInit), volInit, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glGenTextures(1, &_texDensity);
    glBindTexture(GL_TEXTURE_3D, _texDensity);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, N, N, N, 0,
                 GL_RED, GL_UNSIGNED_BYTE, nullptr);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_3D, 0);

    checkGlError("FluidsGPU::init");

    __android_log_print(ANDROID_LOG_INFO, TAG_GPU,
                        "FluidsGPU initialized: SSBO=%d, TEX=%d", _ssboPhi, _texDensity);
}

void FluidsGPU::compileShader(GLuint& program, const char* source) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[2048];
        glGetShaderInfoLog(shader, 2048, nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG_GPU, "Compute shader compile error: %s", log);
        glDeleteShader(shader);
        program = 0;
        return;
    }

    program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[2048];
        glGetProgramInfoLog(program, 2048, nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG_GPU, "Compute shader link error: %s", log);
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(shader);
}

void FluidsGPU::dispatchInitSphere() {
    const int zGroups = (_N + 3) / 4;
    glUseProgram(_programInit);
    glUniform1i(glGetUniformLocation(_programInit, "uN"), _N);
    float center = _N / 2.0f;
    glUniform3f(glGetUniformLocation(_programInit, "uCenter"), center, center, center);
    const float baseRadius = _N / 2.5f;
    const float amountRadius =
            baseRadius * std::cbrt(_waterAmount);

    const float maxRadius = _N * 0.49f;
    const float radius = std::min(amountRadius, maxRadius);

    glUniform1f(
            glGetUniformLocation(_programInit, "uRadius"),
            radius
    );
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboPhi);
    glDispatchCompute(_groups, _groups, zGroups);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsGPU::dispatchBuildFaceLabels() {
    const int zGroups = (_N + 3) / 4;
    glUseProgram(_programFaceLabels);
    glUniform1i(glGetUniformLocation(_programFaceLabels, "uN"), _N);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboPhi);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, _ssboLabelsU);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, _ssboLabelsV);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, _ssboLabelsW);
    glDispatchCompute(_groups, _groups, zGroups);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsGPU::dispatchAddForces() {
    const int zGroups = (_N + 3) / 4;

    glUseProgram(_programAddForces);

    glUniform1i(glGetUniformLocation(_programAddForces, "uN"), _N);

    glUniform3f(
            glGetUniformLocation(_programAddForces, "uGravity"),
            _gravityX,
            _gravityY,
            _gravityZ
    );

    glUniform1f(
            glGetUniformLocation(_programAddForces, "uDamping"),
            _damping
    );

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _ssboU);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboV);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssboW);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, _ssboLabelsU);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, _ssboLabelsV);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, _ssboLabelsW);

    glDispatchCompute(_groups, _groups, zGroups);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsGPU::dispatchDivergence() {
    const int zGroups = (_N + 3) / 4;
    glUseProgram(_programDivergence);
    glUniform1i(glGetUniformLocation(_programDivergence, "uN"), _N);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _ssboU);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboV);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssboW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, _ssboDivergence);
    glDispatchCompute(_groups, _groups, zGroups);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsGPU::dispatchPressure() {
    const int zGroups = (_N + 3) / 4;
    glUseProgram(_programPressureGS);
    glUniform1i(glGetUniformLocation(_programPressureGS, "uN"), _N);
    GLint locParity = glGetUniformLocation(_programPressureGS, "uParity");
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboPhi);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, _ssboDivergence);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, _ssboPressure);

    for (int it = 0; it < PRESSURE_ITERS; ++it) {
        glUniform1i(locParity, 0);
        glDispatchCompute(_groups, _groups, zGroups);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glUniform1i(locParity, 1);
        glDispatchCompute(_groups, _groups, zGroups);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
}

void FluidsGPU::dispatchApplyPressure() {
    const int zGroups = (_N + 3) / 4;
    glUseProgram(_programApplyPressure);
    glUniform1i(glGetUniformLocation(_programApplyPressure, "uN"), _N);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _ssboU);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboV);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssboW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, _ssboPressure);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, _ssboLabelsU);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, _ssboLabelsV);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, _ssboLabelsW);
    glDispatchCompute(_groups, _groups, zGroups);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsGPU::dispatchExtrapolateVel() {
    const int zGroups = (_N + 3) / 4;
    glUseProgram(_programExtrapolateVel);
    glUniform1i(glGetUniformLocation(_programExtrapolateVel, "uN"), _N);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _ssboU);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboV);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssboW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, _ssboLabelsU);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, _ssboLabelsV);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, _ssboLabelsW);
    glDispatchCompute(_groups, _groups, zGroups);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsGPU::dispatchAdvectSurface() {
    const int zGroups = (_N + 3) / 4;
    glUseProgram(_programAdvectCopy);
    glUniform1i(glGetUniformLocation(_programAdvectCopy, "uN"), _N);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboPhi);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboPhiPrev);
    glDispatchCompute(_groups, _groups, zGroups);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(_programAdvectSurface);
    glUniform1i(glGetUniformLocation(_programAdvectSurface, "uN"), _N);
    glUniform1f(glGetUniformLocation(_programAdvectSurface, "uDt"),  static_cast<float>(Config::dt));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboPhi);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboPhiPrev);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _ssboU);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboV);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _ssboW);
    glDispatchCompute(_groups, _groups, zGroups);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsGPU::dispatchRedistancing() {
    const int zGroups = (_N + 3) / 4;
    glUseProgram(_programRdInit);
    glUniform1i(glGetUniformLocation(_programRdInit, "uN"), _N);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboPhi);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _ssboPhiPrev);
    glDispatchCompute(_groups, _groups, zGroups);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    GLuint readBuf = _ssboPhiPrev;
    GLuint writeBuf = _ssboPhi;
    for (int it = 0; it < 3; ++it) {
        glUseProgram(_programRdRelax);
        glUniform1i(glGetUniformLocation(_programRdRelax, "uN"), _N);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, readBuf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, writeBuf);
        glDispatchCompute(_groups, _groups, zGroups);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        std::swap(readBuf, writeBuf);
    }
}

void FluidsGPU::dispatchVolume() {



    const int zGroups = (_N + 3) / 4;
    const int totalGroups = _groups * _groups * zGroups;

    glUseProgram(_programVolumeSum);
    glUniform1i(glGetUniformLocation(_programVolumeSum, "uN"), _N);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboPhi);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _ssboPartialSums);
    glDispatchCompute(_groups, _groups, zGroups);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(_programVolumeFinal);
    glUniform1i(glGetUniformLocation(_programVolumeFinal, "uNumGroups"), totalGroups);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, _ssboPartialSums);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, _ssboVolume);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glUseProgram(_programVolumeApply);
    glUniform1i(glGetUniformLocation(_programVolumeApply, "uN"), _N);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboPhi);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, _ssboVolume);
    glDispatchCompute(_groups, _groups, zGroups);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsGPU::dispatchExportDensity() {
    const int totalTexels = _N * _N * _N;
    const int packedWords = (totalTexels + 3) / 4;
    const int groups = (packedWords + 255) / 256;

    glUseProgram(_programExport);

    glUniform1i(glGetUniformLocation(_programExport, "uTotal"), totalTexels);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboPhi);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, _ssboDensityOut);

    glDispatchCompute(groups, 1, 1);

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, _texDensity);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _ssboDensityOut);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexSubImage3D(
            GL_TEXTURE_3D,
            0,
            0, 0, 0,
            _N, _N, _N,
            GL_RED,
            GL_UNSIGNED_BYTE,
            (const void*)0
    );

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
}

void FluidsGPU::step() {
    static std::uint64_t s_stepsSinceCheck = 0;

    if (!_initialized || _waterAmountDirty) {
        if (_programInit != 0) {
            dispatchInitSphere();
            clearVelocityAndPressure();
            resetVolumeTarget();

            _initialized = true;
            _waterAmountDirty = false;
        }
    }
    static std::uint64_t s_totalSteps = 0;
    const bool trace = (s_totalSteps < 2);
    if (_initialized) {
        dispatchBuildFaceLabels();   if (trace) checkGlError("faceLabels");
        dispatchAddForces();         if (trace) checkGlError("addForces");
        dispatchDivergence();        if (trace) checkGlError("divergence");
        dispatchPressure();          if (trace) checkGlError("pressure");
        dispatchApplyPressure();     if (trace) checkGlError("applyPressure");
        dispatchExtrapolateVel();    if (trace) checkGlError("extrapolateVel");
        dispatchAdvectSurface();     if (trace) checkGlError("advectSurface");
        dispatchRedistancing();      if (trace) checkGlError("redistancing");
        dispatchVolume();            if (trace) checkGlError("volume");
        dispatchExportDensity();     if (trace) checkGlError("exportDensity");
        ++s_totalSteps;
    }



    if ((++s_stepsSinceCheck % 120) == 0) {
        checkGlError("FluidsGPU::step (periodic)");
    }
}

void FluidsGPU::setGravity(float gx, float gy, float gz) {
    _gravityX = gx;
    _gravityY = gy;
    _gravityZ = gz;
}

void FluidsGPU::destroy() {
    GLuint programs[] = {
            _programInit, _programFaceLabels, _programAddForces,
            _programAdvectCopy, _programAdvectSurface,
            _programRdInit, _programRdRelax,
            _programVolumeSum, _programVolumeFinal, _programVolumeApply,
            _programDivergence, _programPressureGS, _programApplyPressure,
            _programExtrapolateVel, _programExport
    };
    for (GLuint p : programs) if (p) glDeleteProgram(p);

    GLuint buffers[] = {
            _ssboPhi, _ssboPhiPrev, _ssboU, _ssboV, _ssboW,
            _ssboLabelsU, _ssboLabelsV, _ssboLabelsW,
            _ssboDivergence, _ssboPressure,
            _ssboPartialSums, _ssboVolume,
            _ssboDensityOut
    };


    for (GLuint b : buffers) if (b) glDeleteBuffers(1, &b);

    if (_texDensity) {
        glDeleteTextures(1, &_texDensity);
        _texDensity = 0;
    }
    __android_log_print(ANDROID_LOG_INFO, TAG_GPU, "FluidsGPU destroyed");
}
