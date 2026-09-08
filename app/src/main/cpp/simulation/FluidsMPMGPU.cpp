#include "FluidsMPMGPU.h"
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <android/log.h>
#include <algorithm>

#define TAG_MPM "FluidMPMGPU"
static void createSSBOZero(GLuint& buf, std::size_t bytes);




static const char* CLEAR_GRID_SRC = R"GLSL(#version 310 es
layout(local_size_x = 64) in;
struct Cell { int vx; int vy; int vz; int mass; };
layout(std430, binding = 0) buffer Cells { Cell cells[]; };
uniform int uGridCount;
void main() {
    int id = int(gl_GlobalInvocationID.x);
    if (id >= uGridCount) return;
    cells[id].vx = 0; cells[id].vy = 0; cells[id].vz = 0; cells[id].mass = 0;
}
)GLSL";

static const char* CLEAR_INT_SRC = R"GLSL(#version 310 es
layout(local_size_x = 64) in;
layout(std430, binding = 0) buffer Grid { int grid[]; };
uniform int uCount;
void main() {
    int id = int(gl_GlobalInvocationID.x);
    if (id >= uCount) return;
    grid[id] = 0;
}
)GLSL";

void FluidsMPMGPU::setParticleCount(int count) {
    count = std::clamp(count, 1000, 100000);
    if (!_initialized) {
        _maxParticles = count;
        _numParticles = std::min(_numParticles, _maxParticles);
        _particlesDirty = true;
        return;
    }
    if (count == _maxParticles) return;

    _maxParticles = count;
    _numParticles = std::min(_numParticles, _maxParticles);

    if (_ssboParticles) { glDeleteBuffers(1, &_ssboParticles); _ssboParticles = 0; }
    if (_ssboDensities) { glDeleteBuffers(1, &_ssboDensities);  _ssboDensities = 0; }


    createSSBOZero(
            _ssboParticles,
            static_cast<std::size_t>(_maxParticles) * 80
    );

    createSSBOZero(
            _ssboDensities,
            static_cast<std::size_t>(_maxParticles) * 4
    );

    _particlesDirty = true;

    __android_log_print(
            ANDROID_LOG_INFO,
            TAG_MPM,
            "setParticleCount: new max=%d",
            _maxParticles
    );
}

static const char* P2G1_SRC = R"GLSL(#version 310 es
layout(local_size_x = 64) in;
struct Particle { vec3 position; vec3 v; mat3 C; };
struct Cell { int vx; int vy; int vz; int mass; };
layout(std430, binding = 0) readonly buffer Particles { Particle particles[]; };
layout(std430, binding = 1) buffer Cells { Cell cells[]; };
uniform int   uNumParticles;
uniform vec3  uBoxSize;
uniform float uFixedM;
uniform int   uGridCount;

int encodeF(float x) { return int(x * uFixedM); }

void main() {
    int id = int(gl_GlobalInvocationID.x);
    if (id >= uNumParticles) return;
    Particle p = particles[id];
    vec3 cellIndexF = floor(p.position);
    vec3 cellDiff   = p.position - (cellIndexF + 0.5);
    vec3 w[3];
    w[0] = 0.5 * (0.5 - cellDiff) * (0.5 - cellDiff);
    w[1] = 0.75 - cellDiff * cellDiff;
    w[2] = 0.5 * (0.5 + cellDiff) * (0.5 + cellDiff);
    mat3 C = p.C;

    for (int gx = 0; gx < 3; gx++)
    for (int gy = 0; gy < 3; gy++)
    for (int gz = 0; gz < 3; gz++) {
        float weight = w[gx].x * w[gy].y * w[gz].z;
        vec3 cellX = cellIndexF + vec3(float(gx), float(gy), float(gz)) - 1.0;
        vec3 cellDist = (cellX + 0.5) - p.position;
        vec3 Q = C * cellDist;
        float massContrib = weight;
        vec3  velContrib  = massContrib * (p.v + Q);

        int cx = int(cellX.x);
        int cy = int(cellX.y);
        int cz = int(cellX.z);
        if (cx >= 0 && cx < int(uBoxSize.x) &&
            cy >= 0 && cy < int(uBoxSize.y) &&
            cz >= 0 && cz < int(uBoxSize.z)) {
            int ci = cx * int(uBoxSize.y) * int(uBoxSize.z)
                   + cy * int(uBoxSize.z) + cz;
            if (ci >= 0 && ci < uGridCount) {
                atomicAdd(cells[ci].mass, encodeF(massContrib));
                atomicAdd(cells[ci].vx,   encodeF(velContrib.x));
                atomicAdd(cells[ci].vy,   encodeF(velContrib.y));
                atomicAdd(cells[ci].vz,   encodeF(velContrib.z));
            }
        }
    }
}
)GLSL";




static const char* P2G2_SRC = R"GLSL(#version 310 es
layout(local_size_x = 64) in;
struct Particle { vec3 position; vec3 v; mat3 C; };
struct Cell { int vx; int vy; int vz; int mass; };
layout(std430, binding = 0) readonly buffer Particles { Particle particles[]; };
layout(std430, binding = 1) buffer Cells { Cell cells[]; };
layout(std430, binding = 2) buffer Densities { float densities[]; };
uniform int   uNumParticles;
uniform vec3  uBoxSize;
uniform float uDt;
uniform float uFixedM;
uniform float uFixedMInv;
uniform float uStiffness;
uniform float uRestDensity;
uniform float uViscosity;
uniform int   uGridCount;

int   encodeF(float x) { return int(x * uFixedM); }
float decodeF(int   x) { return float(x) * uFixedMInv; }

void main() {
    int id = int(gl_GlobalInvocationID.x);
    if (id >= uNumParticles) return;
    Particle p = particles[id];
    vec3 cellIndexF = floor(p.position);
    vec3 cellDiff   = p.position - (cellIndexF + 0.5);
    vec3 w[3];
    w[0] = 0.5 * (0.5 - cellDiff) * (0.5 - cellDiff);
    w[1] = 0.75 - cellDiff * cellDiff;
    w[2] = 0.5 * (0.5 + cellDiff) * (0.5 + cellDiff);

    float density = 0.0;
    for (int gx = 0; gx < 3; gx++)
    for (int gy = 0; gy < 3; gy++)
    for (int gz = 0; gz < 3; gz++) {
        float weight = w[gx].x * w[gy].y * w[gz].z;
        vec3 cellX = cellIndexF + vec3(float(gx), float(gy), float(gz)) - 1.0;
        int cx = int(cellX.x); int cy = int(cellX.y); int cz = int(cellX.z);
        if (cx >= 0 && cx < int(uBoxSize.x) &&
            cy >= 0 && cy < int(uBoxSize.y) &&
            cz >= 0 && cz < int(uBoxSize.z)) {
            int ci = cx * int(uBoxSize.y) * int(uBoxSize.z)
                   + cy * int(uBoxSize.z) + cz;
            if (ci >= 0 && ci < uGridCount)
                density += decodeF(cells[ci].mass) * weight;
        }
    }
    densities[id] = density;

    float volume   = 1.0 / max(density, 0.01);
    float pressure = max(0.0, uStiffness * (pow(max(density / uRestDensity, 0.001), 1.0) - 1.0));
    mat3 stress = mat3(-pressure,0,0, 0,-pressure,0, 0,0,-pressure);
    mat3 strain = p.C + transpose(p.C);
    stress += uViscosity * strain;
    mat3 eq16 = -volume * 4.0 * stress * uDt;

    for (int gx = 0; gx < 3; gx++)
    for (int gy = 0; gy < 3; gy++)
    for (int gz = 0; gz < 3; gz++) {
        float weight = w[gx].x * w[gy].y * w[gz].z;
        vec3 cellX = cellIndexF + vec3(float(gx), float(gy), float(gz)) - 1.0;
        vec3 cellDist = (cellX + 0.5) - p.position;
        vec3 momentum = eq16 * weight * cellDist;
        int cx = int(cellX.x); int cy = int(cellX.y); int cz = int(cellX.z);
        if (cx >= 0 && cx < int(uBoxSize.x) &&
            cy >= 0 && cy < int(uBoxSize.y) &&
            cz >= 0 && cz < int(uBoxSize.z)) {
            int ci = cx * int(uBoxSize.y) * int(uBoxSize.z)
                   + cy * int(uBoxSize.z) + cz;
            if (ci >= 0 && ci < uGridCount) {
                atomicAdd(cells[ci].vx, encodeF(momentum.x));
                atomicAdd(cells[ci].vy, encodeF(momentum.y));
                atomicAdd(cells[ci].vz, encodeF(momentum.z));
            }
        }
    }
}
)GLSL";




static const char* UPDATE_GRID_SRC = R"GLSL(#version 310 es
layout(local_size_x = 64) in;
struct Cell { int vx; int vy; int vz; int mass; };
layout(std430, binding = 0) buffer Cells { Cell cells[]; };
uniform int   uGridCount;
uniform vec3  uBoxSize;
uniform vec3  uRealBoxSize;
uniform vec3  uGravity;
uniform float uDt;
uniform float uFixedM;
uniform float uFixedMInv;
uniform vec3  uPointerCell;
uniform vec3  uPointerForce;
uniform float uPointerRadius;
uniform float uPointerStrength;
uniform int   uPointerActive;
int   encodeF(float x) { return int(x * uFixedM); }
float decodeF(int   x) { return float(x) * uFixedMInv; }
void main() {
    int id = int(gl_GlobalInvocationID.x);
    if (id >= uGridCount) return;
    if (cells[id].mass == 0) return;
    float mass = decodeF(cells[id].mass);
    vec3 vel = vec3(decodeF(cells[id].vx), decodeF(cells[id].vy),
                    decodeF(cells[id].vz)) / max(mass, 0.0001);
    vel += uGravity * uDt;
float maxVel = 20.0;
if (length(vel) > maxVel) {
    vel = normalize(vel) * maxVel;
}
    if (uPointerActive > 0) {
        int sz = int(uBoxSize.z);
        int sy = int(uBoxSize.y);
        int cz = id % sz;
        int cy = (id / sz) % sy;
        int cx = id / (sz * sy);
        vec3 cellPos = vec3(float(cx), float(cy), float(cz));
        vec3 diff = cellPos - uPointerCell;
        float d2 = dot(diff, diff);
        float r2 = uPointerRadius * uPointerRadius;
        if (d2 < r2) {
            float falloff = smoothstep(r2, 0.0, d2);
            vel += uPointerForce * (falloff * uPointerStrength * 1.0);
        }
    }

    int sz = int(uBoxSize.z);
    int sy = int(uBoxSize.y);
    int cz = id % sz;
    int cy = (id / sz) % sy;
    int cx = id / (sz * sy);

    // Boundary: only zero the component moving INTO the wall,
    // and add a small repulsion to prevent sticking
    float wallMargin = 3.0;
    float repulsion = 0.4;

    if (float(cx) < wallMargin) {
        if (vel.x < 0.0) vel.x = 0.0;
        vel.x += repulsion * (wallMargin - float(cx)) / wallMargin;
    }
    if (float(cx) > uRealBoxSize.x - 1.0 - wallMargin) {
        if (vel.x > 0.0) vel.x = 0.0;
        vel.x -= repulsion * (float(cx) - (uRealBoxSize.x - 1.0 - wallMargin)) / wallMargin;
    }
    if (float(cy) < wallMargin) {
        if (vel.y < 0.0) vel.y = 0.0;
        vel.y += repulsion * (wallMargin - float(cy)) / wallMargin;
    }
    if (float(cy) > uRealBoxSize.y - 1.0 - wallMargin) {
        if (vel.y > 0.0) vel.y = 0.0;
        vel.y -= repulsion * (float(cy) - (uRealBoxSize.y - 1.0 - wallMargin)) / wallMargin;
    }
    if (float(cz) < wallMargin) {
        if (vel.z < 0.0) vel.z = 0.0;
        vel.z += repulsion * (wallMargin - float(cz)) / wallMargin;
    }
    if (float(cz) > uRealBoxSize.z - 1.0 - wallMargin) {
        if (vel.z > 0.0) vel.z = 0.0;
        vel.z -= repulsion * (float(cz) - (uRealBoxSize.z - 1.0 - wallMargin)) / wallMargin;
    }

    cells[id].vx = encodeF(vel.x);
    cells[id].vy = encodeF(vel.y);
    cells[id].vz = encodeF(vel.z);
}
)GLSL";



static const char* G2P_SRC = R"GLSL(#version 310 es
layout(local_size_x = 64) in;
struct Particle { vec3 position; vec3 v; mat3 C; };
struct Cell { int vx; int vy; int vz; int mass; };
layout(std430, binding = 0) buffer Particles { Particle particles[]; };
layout(std430, binding = 1) readonly buffer Cells { Cell cells[]; };
uniform int   uNumParticles;
uniform vec3  uBoxSize;
uniform vec3  uRealBoxSize;
uniform float uDt;
uniform float uFixedMInv;
uniform int   uGridCount;
float decodeF(int x) { return float(x) * uFixedMInv; }
void main() {
    int id = int(gl_GlobalInvocationID.x);
    if (id >= uNumParticles) return;
    Particle p = particles[id];
    vec3 cellIndexF = floor(p.position);
    vec3 cellDiff   = p.position - (cellIndexF + 0.5);
    vec3 w[3];
    w[0] = 0.5 * (0.5 - cellDiff) * (0.5 - cellDiff);
    w[1] = 0.75 - cellDiff * cellDiff;
    w[2] = 0.5 * (0.5 + cellDiff) * (0.5 + cellDiff);
    vec3 newVel = vec3(0.0);
    mat3 B = mat3(0.0);
    for (int gx = 0; gx < 3; gx++)
    for (int gy = 0; gy < 3; gy++)
    for (int gz = 0; gz < 3; gz++) {
        float weight = w[gx].x * w[gy].y * w[gz].z;
        vec3 cellX = cellIndexF + vec3(float(gx), float(gy), float(gz)) - 1.0;
        vec3 cellDist = (cellX + 0.5) - p.position;
        int cx = int(cellX.x); int cy = int(cellX.y); int cz = int(cellX.z);
        if (cx < 0 || cx >= int(uBoxSize.x) ||
            cy < 0 || cy >= int(uBoxSize.y) ||
            cz < 0 || cz >= int(uBoxSize.z)) continue;
        int ci = cx * int(uBoxSize.y) * int(uBoxSize.z)
               + cy * int(uBoxSize.z) + cz;
        if (ci < 0 || ci >= uGridCount) continue;
        Cell c = cells[ci];
        vec3 cellVel = vec3(decodeF(c.vx), decodeF(c.vy), decodeF(c.vz));
        vec3 wv = cellVel * weight;
        B += mat3(wv * cellDist.x, wv * cellDist.y, wv * cellDist.z);
        newVel += wv;
    }
    p.v = newVel;
    p.C = B * 4.0;
    p.position += p.v * uDt;

    vec3 lo = vec3(2.0);
    vec3 hi = uRealBoxSize - vec3(3.0);
    p.position = clamp(p.position, lo, hi);

    float wallStiffness = 3.0;
    vec3 x_n = p.position + p.v * (uDt * 2.0);
    vec3 wallMin = vec3(3.5);
    vec3 wallMax = uRealBoxSize - vec3(4.5);

    if (x_n.x < wallMin.x) p.v.x += wallStiffness * (wallMin.x - x_n.x);
    if (x_n.x > wallMax.x) p.v.x += wallStiffness * (wallMax.x - x_n.x);
    if (x_n.y < wallMin.y) p.v.y += wallStiffness * (wallMin.y - x_n.y);
    if (x_n.y > wallMax.y) p.v.y += wallStiffness * (wallMax.y - x_n.y);
    if (x_n.z < wallMin.z) p.v.z += wallStiffness * (wallMin.z - x_n.z);
    if (x_n.z > wallMax.z) p.v.z += wallStiffness * (wallMax.z - x_n.z);

    vec3 distToWall = min(p.position - lo, hi - p.position);
    float minDist = min(distToWall.x, min(distToWall.y, distToWall.z));
    if (minDist < 2.0) {
        float damp = 0.85 + 0.15 * (minDist / 2.0);
        p.v *= damp;
    }

    particles[id] = p;
}
)GLSL";



static const char* P2G_DENSITY_SRC = R"GLSL(#version 310 es
layout(local_size_x = 64) in;
struct Particle { vec3 position; vec3 v; mat3 C; };
layout(std430, binding = 0) readonly buffer Particles { Particle particles[]; };
layout(std430, binding = 1) readonly buffer Densities { float densities[]; };
layout(std430, binding = 2) buffer DensityGrid { int densityGrid[]; };
uniform int   uNumParticles;
uniform vec3  uDensityGridSize;
uniform float uDensityM;
uniform int   uGridCount;

int encodeF(float x) { return int(x * uDensityM); }

void main() {
    int id = int(gl_GlobalInvocationID.x);
    if (id >= uNumParticles) return;
    Particle p = particles[id];
    vec3 cellIndexF = floor(p.position);
    vec3 cellDiff   = p.position - (cellIndexF + 0.5);
    vec3 w[3];
    w[0] = 0.5 * (0.5 - cellDiff) * (0.5 - cellDiff);
    w[1] = 0.75 - cellDiff * cellDiff;
    w[2] = 0.5 * (0.5 + cellDiff) * (0.5 + cellDiff);

    for (int gx = 0; gx < 3; gx++)
    for (int gy = 0; gy < 3; gy++)
    for (int gz = 0; gz < 3; gz++) {
        float weight = w[gx].x * w[gy].y * w[gz].z;
        vec3 cellX = cellIndexF + vec3(float(gx), float(gy), float(gz)) - 1.0;
        int cx = int(cellX.x); int cy = int(cellX.y); int cz = int(cellX.z);
        if (cx >= 0 && cx < int(uDensityGridSize.x) &&
            cy >= 0 && cy < int(uDensityGridSize.y) &&
            cz >= 0 && cz < int(uDensityGridSize.z)) {
            int ci = cx * int(uDensityGridSize.y) * int(uDensityGridSize.z)
                   + cy * int(uDensityGridSize.z) + cz;
            if (ci >= 0 && ci < uGridCount)
                atomicAdd(densityGrid[ci], encodeF(densities[id] * weight));
        }
    }
}
)GLSL";




static const char* CAST_DENSITY_SRC = R"GLSL(#version 310 es
layout(local_size_x = 64) in;
layout(std430, binding = 0) readonly buffer DensityGrid { int  densityGrid[]; };
layout(std430, binding = 1) writeonly buffer CastedGrid { uint packed[]; };
uniform int   uTotal;
uniform float uDensityMInv;
uniform float uDensityScale;
void main() {
    uint base = gl_GlobalInvocationID.x * 4u;
    if (base >= uint(uTotal)) return;
    uint packedValue = 0u;
    for (uint i = 0u; i < 4u; ++i) {
        uint idx = base + i;
        float value = (idx < uint(uTotal))
            ? float(densityGrid[int(idx)]) * uDensityMInv * uDensityScale
            : 0.0;
        value = clamp(value, 0.0, 1.0);
        uint b = uint(value * 255.0 + 0.5);
        packedValue |= (b << (i * 8u));
    }
    packed[int(base >> 2u)] = packedValue;
}
)GLSL";




static void createSSBOZero(GLuint& buf, std::size_t bytes) {
    std::vector<uint8_t> zeros(bytes, 0);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, zeros.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

static void checkGlError(const char* where) {
    GLenum err = glGetError();
    while (err != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, TAG_MPM,
                            "GL error 0x%04x after %s", (unsigned)err, where);
        err = glGetError();
    }
}


void FluidsMPMGPU::init(int gridX, int gridY, int gridZ,
                        int maxParticles, int w, int h) {
    _gx = gridX; _gy = gridY; _gz = gridZ;
    _gridCount    = gridX * gridY * gridZ;
    _maxParticles = maxParticles;
    _numParticles = 0;

    __android_log_print(ANDROID_LOG_INFO, TAG_MPM,
                        "init: grid=%dx%dx%d particles=%d", gridX, gridY, gridZ, maxParticles);

    compileShader(_progClear,   CLEAR_GRID_SRC);
    compileShader(_progP2G1,    P2G1_SRC);
    compileShader(_progP2G2,    P2G2_SRC);
    compileShader(_progUpdate,  UPDATE_GRID_SRC);
    compileShader(_progG2P,     G2P_SRC);
    compileShader(_progP2GD,    P2G_DENSITY_SRC);
    compileShader(_progCast,    CAST_DENSITY_SRC);
    compileShader(_progClearInt, CLEAR_INT_SRC);
    bool ok = _progClear && _progClearInt && _progP2G1 && _progP2G2 && _progUpdate
              && _progG2P && _progP2GD && _progCast;
    if (!ok) {
        __android_log_print(ANDROID_LOG_ERROR, TAG_MPM, "Shader compilation FAILED!");
        return;
    }

    const std::size_t PARTICLE_BYTES = 80;
    createSSBOZero(_ssboParticles,    PARTICLE_BYTES * maxParticles);
    createSSBOZero(_ssboCells,        16 * _gridCount);
    createSSBOZero(_ssboDensities,     4 * maxParticles);
    createSSBOZero(_ssboDensityGrid,   4 * _gridCount);
    createSSBOZero(_ssboCasted, ((_gridCount + 3) / 4) * 4);

    glGenTextures(1, &_texDensity);
    glBindTexture(GL_TEXTURE_3D, _texDensity);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, _gx, _gy, _gz, 0,
                 GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_3D, 0);

    initParticles();

    checkGlError("FluidsMPMGPU::init");

    _initialized = true;
    _particlesDirty = false;
    __android_log_print(ANDROID_LOG_INFO, TAG_MPM,
                        "initialized: particles=%d scenario=%d", _numParticles, _scenario);
}

void FluidsMPMGPU::initParticles() {
    const float spacing = 0.92f;

    std::vector<float> data(
            static_cast<std::size_t>(_maxParticles) * 20,
            0.0f
    );

    _numParticles = 0;

    const float amount = std::clamp(_waterAmount, 0.1f, 2.0f);

    const int target = static_cast<int>(
            std::min(
                    static_cast<float>(_maxParticles),
                    static_cast<float>(_maxParticles) * amount
            )
    );

    auto addParticle = [&](
            float x, float y, float z,
            float vx, float vy, float vz
    ) {
        if (_numParticles >= target) return;

        const int base = _numParticles * 20;

        float jx = 0.5f * ((float)rand() / (float)RAND_MAX - 0.5f) * 0.35f;
        float jy = 0.5f * ((float)rand() / (float)RAND_MAX - 0.5f) * 0.35f;
        float jz = 0.5f * ((float)rand() / (float)RAND_MAX - 0.5f) * 0.35f;

        data[base + 0] = x + jx;
        data[base + 1] = y + jy;
        data[base + 2] = z + jz;
        data[base + 3] = 0.0f;
        data[base + 4] = vx;
        data[base + 5] = vy;
        data[base + 6] = vz;

        ++_numParticles;
    };

    const float amountScale = std::cbrt(amount);

    switch (_scenario) {
        case DAM_BREAK: {
            float startX = 3.5f;
            float endX   = _gx * (0.44f * amountScale);
            float startY = 3.0f;
            float endY   = _gy * std::min(0.82f * amountScale, 0.90f);
            float startZ = 3.5f;
            float endZ   = _gz * std::min(0.85f * amountScale, 0.90f);

            for (float y = startY; y < endY && _numParticles < target; y += spacing) {
                for (float x = startX; x < endX && _numParticles < target; x += spacing) {
                    for (float z = startZ; z < endZ && _numParticles < target; z += spacing) {
                        addParticle(x, y, z, 0.0f, 0.0f, 0.0f);
                    }
                }
            }

            break;
        }

        case WATER_DROP: {
            float size = 20.0f * amountScale;

            float cx = _gx * 0.5f;
            float cy = _gy * 0.70f;
            float cz = _gz * 0.5f;

            float sp = 0.88f;

            for (float y = cy - size * 0.45f; y < cy + size * 0.45f && _numParticles < target; y += sp) {
                for (float x = cx - size * 0.5f; x < cx + size * 0.5f && _numParticles < target; x += sp) {
                    for (float z = cz - size * 0.5f; z < cz + size * 0.5f && _numParticles < target; z += sp) {
                        addParticle(x, y, z, 0.0f, -2.5f, 0.0f);
                    }
                }
            }

            break;
        }

        case DUAL_WAVE: {
            float sp = 0.95f;
            float margin = 3.5f;

            for (float y = 3.0f; y < _gy * 0.75f * amountScale && _numParticles < target; y += sp) {
                for (float x = margin; x < _gx - margin && _numParticles < target; x += sp) {
                    if (x < _gx * 0.32f || x > _gx * 0.68f) {
                        for (float z = margin; z < _gz - margin && _numParticles < target; z += sp) {
                            float vx = (x < _gx * 0.5f) ? 4.0f : -4.0f;
                            addParticle(x, y, z, vx, 0.0f, 0.0f);
                        }
                    }
                }
            }

            break;
        }
    }

    while (_numParticles < target) {
        addParticle(
                5.0f + ((float)rand() / (float)RAND_MAX) * (_gx - 10.0f),
                5.0f + ((float)rand() / (float)RAND_MAX) * (_gy * 0.5f),
                5.0f + ((float)rand() / (float)RAND_MAX) * (_gz - 10.0f),
                0.0f,
                0.0f,
                0.0f
        );
    }

    __android_log_print(
            ANDROID_LOG_INFO,
            TAG_MPM,
            "initParticles: %d / %d (scenario=%d, waterAmount=%.2f)",
            _numParticles,
            _maxParticles,
            _scenario,
            _waterAmount
    );

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _ssboParticles);
    glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            static_cast<std::size_t>(_maxParticles) * 80,
            data.data()
    );
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsMPMGPU::step() {
    if (!_initialized) return;

    if (_particlesDirty) {
        initParticles();
        _particlesDirty = false;
    }

    clearCells();
    dispatchP2G1();
    dispatchP2G2();
    dispatchUpdateGrid();
    dispatchG2P();

    clearDensityGridBuf();
    dispatchP2GDensity();
    dispatchCastDensity();
    uploadDensityTexture();
}

void FluidsMPMGPU::setGravity(float gx, float gy, float gz) {
    _gravity[0] = gx; _gravity[1] = gy; _gravity[2] = gz;
}

void FluidsMPMGPU::setPointer(float cx, float cy, float cz,
                              float fx, float fy, float fz, float radius) {
    _ptrCell[0]=cx; _ptrCell[1]=cy; _ptrCell[2]=cz;
    _ptrForce[0]=fx; _ptrForce[1]=fy; _ptrForce[2]=fz;
    _ptrRadius = radius;
    _ptrActive = true;
}

void FluidsMPMGPU::releasePointer() {
    _ptrActive = false;
    _ptrCell[0] = -1; _ptrCell[1] = -1; _ptrCell[2] = -1;
}

void FluidsMPMGPU::setScenario(int scenario) {
    scenario = std::clamp(scenario, 0, 2);

    if (scenario != _scenario) {
        _scenario = scenario;
        _particlesDirty = true;
    }
}

void FluidsMPMGPU::setPhysicsParams(float stiffness, float restDensity,
                                    float viscosity, float dt) {
    _stiffness = stiffness;
    _restDensity = restDensity;
    _viscosity = viscosity;
    _dt = dt;
}

void FluidsMPMGPU::setTouchStrength(float s) { _ptrStrength = s; }
void FluidsMPMGPU::setWaterAmount(float a) {
    a = std::clamp(a, 0.1f, 2.0f);

    if (std::abs(a - _waterAmount) > 1e-5f) {
        _waterAmount = a;
        _particlesDirty = true;
    }
}

void FluidsMPMGPU::clearCells() {
    glUseProgram(_progClear);

    glUniform1i(
            glGetUniformLocation(_progClear, "uGridCount"),
            _gridCount
    );

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboCells);

    glDispatchCompute((_gridCount + 63) / 64, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsMPMGPU::clearDensityGridBuf() {
    glUseProgram(_progClearInt);
    glUniform1i(glGetUniformLocation(_progClearInt, "uCount"), _gridCount);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboDensityGrid);
    glDispatchCompute((_gridCount + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsMPMGPU::dispatchP2G1() {
    glUseProgram(_progP2G1);
    glUniform1i(glGetUniformLocation(_progP2G1, "uNumParticles"), _numParticles);
    glUniform3f(glGetUniformLocation(_progP2G1, "uBoxSize"), _gx, _gy, _gz);
    glUniform1f(glGetUniformLocation(_progP2G1, "uFixedM"), _fixedM);
    glUniform1i(glGetUniformLocation(_progP2G1, "uGridCount"), _gridCount);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboParticles);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _ssboCells);
    glDispatchCompute((_numParticles + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsMPMGPU::dispatchP2G2() {
    glUseProgram(_progP2G2);
    glUniform1i(glGetUniformLocation(_progP2G2, "uNumParticles"), _numParticles);
    glUniform3f(glGetUniformLocation(_progP2G2, "uBoxSize"), _gx, _gy, _gz);
    glUniform1f(glGetUniformLocation(_progP2G2, "uDt"), _dt * _simSpeed);
    glUniform1f(glGetUniformLocation(_progP2G2, "uFixedM"), _fixedM);
    glUniform1f(glGetUniformLocation(_progP2G2, "uFixedMInv"), 1.0f / _fixedM);
    glUniform1f(glGetUniformLocation(_progP2G2, "uStiffness"), _stiffness);
    glUniform1f(glGetUniformLocation(_progP2G2, "uRestDensity"), _restDensity);
    glUniform1f(glGetUniformLocation(_progP2G2, "uViscosity"), _viscosity);
    glUniform1i(glGetUniformLocation(_progP2G2, "uGridCount"), _gridCount);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboParticles);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _ssboCells);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboDensities);
    glDispatchCompute((_numParticles + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsMPMGPU::dispatchUpdateGrid() {
    glUseProgram(_progUpdate);
    glUniform1i(glGetUniformLocation(_progUpdate, "uGridCount"), _gridCount);
    glUniform3f(glGetUniformLocation(_progUpdate, "uBoxSize"), _gx, _gy, _gz);
    glUniform3f(glGetUniformLocation(_progUpdate, "uRealBoxSize"), _gx, _gy, _gz);
    glUniform3f(glGetUniformLocation(_progUpdate, "uGravity"),
                _gravity[0], _gravity[1], _gravity[2]);
    glUniform1f(glGetUniformLocation(_progUpdate, "uDt"), _dt * _simSpeed);
    glUniform1f(glGetUniformLocation(_progUpdate, "uFixedM"), _fixedM);
    glUniform1f(glGetUniformLocation(_progUpdate, "uFixedMInv"), 1.0f / _fixedM);
    glUniform3f(glGetUniformLocation(_progUpdate, "uPointerCell"),
                _ptrCell[0], _ptrCell[1], _ptrCell[2]);
    glUniform3f(glGetUniformLocation(_progUpdate, "uPointerForce"),
                _ptrForce[0], _ptrForce[1], _ptrForce[2]);
    glUniform1f(glGetUniformLocation(_progUpdate, "uPointerRadius"), _ptrRadius);
    glUniform1f(glGetUniformLocation(_progUpdate, "uPointerStrength"), _ptrStrength);
    glUniform1i(glGetUniformLocation(_progUpdate, "uPointerActive"),
                _ptrActive ? 1 : 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboCells);
    glDispatchCompute((_gridCount + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsMPMGPU::dispatchG2P() {
    glUseProgram(_progG2P);
    glUniform1i(glGetUniformLocation(_progG2P, "uNumParticles"), _numParticles);
    glUniform3f(glGetUniformLocation(_progG2P, "uBoxSize"), _gx, _gy, _gz);
    glUniform3f(glGetUniformLocation(_progG2P, "uRealBoxSize"), _gx, _gy, _gz);
    glUniform1f(glGetUniformLocation(_progG2P, "uDt"), _dt * _simSpeed);
    glUniform1f(glGetUniformLocation(_progG2P, "uFixedMInv"), 1.0f / _fixedM);
    glUniform1i(glGetUniformLocation(_progG2P, "uGridCount"), _gridCount);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboParticles);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _ssboCells);
    glDispatchCompute((_numParticles + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsMPMGPU::dispatchP2GDensity() {
    glUseProgram(_progP2GD);
    glUniform1i(glGetUniformLocation(_progP2GD, "uNumParticles"), _numParticles);
    glUniform3f(glGetUniformLocation(_progP2GD, "uDensityGridSize"), _gx, _gy, _gz);
    glUniform1f(glGetUniformLocation(_progP2GD, "uDensityM"), _fixedM);
    glUniform1i(glGetUniformLocation(_progP2GD, "uGridCount"), _gridCount);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboParticles);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _ssboDensities);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _ssboDensityGrid);
    glDispatchCompute((_numParticles + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsMPMGPU::dispatchCastDensity() {
    int packedWords = (_gridCount + 3) / 4;
    glUseProgram(_progCast);
    glUniform1i(glGetUniformLocation(_progCast, "uTotal"), _gridCount);
    glUniform1f(glGetUniformLocation(_progCast, "uDensityMInv"), 1.0f / _fixedM);
    glUniform1f(glGetUniformLocation(_progCast, "uDensityScale"), 0.33f);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _ssboDensityGrid);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _ssboCasted);
    glDispatchCompute((packedWords + 63) / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidsMPMGPU::uploadDensityTexture() {
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, _texDensity);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _ssboCasted);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, _gx, _gy, _gz,
                    GL_RED, GL_UNSIGNED_BYTE, (const void*)0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
}

void FluidsMPMGPU::compileShader(GLuint& program, const char* source) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[2048];
        glGetShaderInfoLog(shader, 2048, nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, TAG_MPM, "Compile error: %s", log);
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
        __android_log_print(ANDROID_LOG_ERROR, TAG_MPM, "Link error: %s", log);
        glDeleteProgram(program);
        program = 0;
    }
    glDeleteShader(shader);
}

void FluidsMPMGPU::destroy() {
    GLuint progs[] = {_progClear, _progClearInt, _progP2G1, _progP2G2, _progUpdate,
                      _progG2P, _progP2GD, _progCast};
    for (GLuint p : progs) if (p) glDeleteProgram(p);


    _progClear = _progClearInt = _progP2G1 = _progP2G2 = 0;
    _progUpdate = _progG2P = _progP2GD = _progCast = 0;

    GLuint bufs[] = {_ssboParticles, _ssboCells, _ssboDensities,
                     _ssboDensityGrid, _ssboCasted};
    for (GLuint b : bufs) if (b) glDeleteBuffers(1, &b);
    _ssboParticles = _ssboCells = _ssboDensities = 0;
    _ssboDensityGrid = _ssboCasted = 0;

    if (_texDensity) { glDeleteTextures(1, &_texDensity); _texDensity = 0; }

    _initialized = false;
    __android_log_print(ANDROID_LOG_INFO, TAG_MPM, "destroyed");
}
