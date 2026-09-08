#include "GLRenderer.h"
#include <cmath>
#include <android/log.h>
#include <cstring>
#define TAG "GLRenderer3D"



static void normalize3(float* v) {
    float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 1e-6f) {
        v[0] /= l;
        v[1] /= l;
        v[2] /= l;
    }
}

static void cross3(const float* a, const float* b, float* out) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static float dot3(const float* a, const float* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void perspectiveM(float* m, float fovDeg, float aspect, float zn, float zf) {
    std::memset(m, 0, 16 * sizeof(float));

    float f = 1.0f / std::tan(fovDeg * 3.14159265358979f / 360.0f);

    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1.0f;
    m[14] = (2.0f * zf * zn) / (zn - zf);
}

static void lookAtM(float* m, const float* eye, const float* center, const float* up) {
    float f[3] = {
            center[0] - eye[0],
            center[1] - eye[1],
            center[2] - eye[2]
    };
    normalize3(f);

    float s[3];
    cross3(f, up, s);
    normalize3(s);

    float u[3];
    cross3(s, f, u);

    std::memset(m, 0, 16 * sizeof(float));

    m[0] = s[0];
    m[4] = s[1];
    m[8] = s[2];

    m[1] = u[0];
    m[5] = u[1];
    m[9] = u[2];

    m[2] = -f[0];
    m[6] = -f[1];
    m[10] = -f[2];

    m[15] = 1.0f;

    m[12] = -dot3(s, eye);
    m[13] = -dot3(u, eye);
    m[14] =  dot3(f, eye);
}

void GLRenderer::computeCameraMatrices(float* view, float* proj, int w, int h) {
    const float cp = std::cos(_pitch);
    const float sp = std::sin(_pitch);
    const float cy = std::cos(_yaw);
    const float sy = std::sin(_yaw);

    float eye[3] = {
            _dist * cp * sy,
            _dist * sp,
            _dist * cp * cy
    };

    float target[3] = {0.0f, 0.0f, 0.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};

    lookAtM(view, eye, target, up);

    float aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
    perspectiveM(proj, 45.0f, aspect, 0.1f, 100.0f);
}

static const char* VERT_SRC = R"GLSL(#version 300 es
precision mediump float;
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
out vec2 TexCoord;
void main() {
    TexCoord = aTex;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)GLSL";


static const char* FRAG_SRC = R"GLSL(#version 300 es
precision highp float;
precision highp sampler3D;

in  vec2 TexCoord;
out vec4 FragColor;

uniform sampler3D uTex;
uniform vec2  uResolution;
uniform vec3  uEye;
uniform vec3  uTarget;
uniform float uFov;
uniform float uAbsorption;
uniform vec3  uWaterTint;
uniform float uSpecular;
uniform int   uVolSteps;
const float CUBE        = 1.0;

const int   LIGHT_STEPS = 3;
const int   MAX_VOL_STEPS = 128;
const vec3  LIGHT_POS   = vec3(2.5, 5.0, 2.0);
const vec3  LIGHT_COLOR = vec3(1.0, 0.98, 0.92);
const float IOR         = 1.33;
const float FLOOR_Y     = -1.15;


vec2 boxIntersection(vec3 ro, vec3 rd) {
    vec3 invRd = 1.0 / rd;
    vec3 t0 = (-CUBE - ro) * invRd;
    vec3 t1 = ( CUBE - ro) * invRd;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    return vec2(max(max(tmin.x, tmin.y), tmin.z),
                min(min(tmax.x, tmax.y), tmax.z));
}

vec3 rayDirection(vec2 uv, vec3 ro, vec3 target, float fovDeg) {
    vec3 f = normalize(target - ro);
    vec3 up = abs(f.y) > 0.99 ? vec3(1,0,0) : vec3(0,1,0);
    vec3 r = normalize(cross(f, up));
    vec3 u = cross(r, f);
    float focal = 1.0 / tan(radians(fovDeg) * 0.5);
    return normalize(f * focal + uv.x * r + uv.y * u);
}

float sampleDensity(vec3 p) {
    vec3 uv = (p + CUBE) / (2.0 * CUBE);
    float raw = texture(uTex, uv).r;
    return smoothstep(0.25, 0.60, raw);
}

float sampleRawDensity(vec3 p) {
    vec3 uv = (p + CUBE) / (2.0 * CUBE);
    return texture(uTex, uv).r;
}

vec3 densityNormal(vec3 p) {
    const float e = 0.035;

    vec3 cp = clamp(p, vec3(-CUBE + e * 1.05), vec3(CUBE - e * 1.05));
    float dx = sampleRawDensity(cp + vec3(e, 0.0, 0.0)) - sampleRawDensity(cp - vec3(e, 0.0, 0.0));
    float dy = sampleRawDensity(cp + vec3(0.0, e, 0.0)) - sampleRawDensity(cp - vec3(0.0, e, 0.0));
    float dz = sampleRawDensity(cp + vec3(0.0, 0.0, e)) - sampleRawDensity(cp - vec3(0.0, 0.0, e));
    return vec3(dx, dy, dz);
}



vec3 boxNormal(vec3 p) {
    vec3 ap = abs(p);
    if (ap.x > ap.y && ap.x > ap.z) return vec3(sign(p.x), 0.0, 0.0);
    if (ap.y > ap.z) return vec3(0.0, sign(p.y), 0.0);
    return vec3(0.0, 0.0, sign(p.z));
}

vec3 background(vec3 ro, vec3 rd, float caustic) {
  vec3 skyTop = vec3(0.16, 0.38, 0.80);
    vec3 skyHor = vec3(0.62, 0.78, 0.95);

    vec3 sunDir = normalize(LIGHT_POS);

    if (rd.y < -1e-4) {
        float t = (FLOOR_Y - ro.y) / rd.y;
        vec3 p = ro + rd * t;
        vec2 q = floor(p.xz * 2.0);
        float chk = mod(q.x + q.y, 2.0);
        vec3 col = mix(vec3(0.34), vec3(0.13), chk);

        vec2 msk = vec2(1.0) - smoothstep(vec2(1.0), vec2(1.4), abs(p.xz));
        float glow = caustic * msk.x * msk.y * exp(-0.9 * dot(p.xz, p.xz));
        col += LIGHT_COLOR * vec3(0.75, 0.95, 1.0) * 1.6 * glow;

        float fog = clamp(1.0 - exp(-0.002 * t * t), 0.0, 1.0);
        return mix(col, skyHor, fog);
    }
      vec3 col = mix(skyHor, skyTop, pow(clamp(rd.y, 0.0, 1.0), 0.6));


    float sunDot = clamp(dot(rd, sunDir), 0.0, 1.0);
    col += LIGHT_COLOR * 0.30 * pow(sunDot, 32.0);
    col += LIGHT_COLOR * 0.90 * pow(sunDot, 256.0);
    col += LIGHT_COLOR * 3.0 * smoothstep(0.9992, 0.9997, sunDot);

    return col;
}

void main() {
    vec2 uv = (gl_FragCoord.xy * 2.0 - uResolution) / uResolution.y;
    vec3 rd = rayDirection(uv, uEye, uTarget, uFov);
    vec3 ro = uEye;

    vec3 col = background(ro, rd, 0.0);
    vec2 hit = boxIntersection(ro, rd);
    float t0 = max(hit.x, 0.0);
    float t1 = hit.y;

if (t1 > t0) {
    int steps = uVolSteps;

    if (steps < 8) steps = 8;
    if (steps > MAX_VOL_STEPS) steps = MAX_VOL_STEPS;

    float stepLen = (t1 - t0) / float(steps);
    vec3 step = rd * stepLen;
    vec3 pos = ro + rd * (t0 + stepLen * 0.5);

    vec3 T = vec3(1.0);
    vec3 acc = vec3(0.0);
    vec3 refrDir = rd;
    vec3 exitPos = ro + rd * t1;

    bool inWater = false;
    bool haveExit = false;

    vec3 absCoeff = vec3(2.68, 1.71, 1.16) * (uAbsorption / 4.1) * 1.8;

for (int i = 0; i < MAX_VOL_STEPS; ++i) {
    if (i >= steps) break;

    float d = sampleDensity(pos);


            if (!inWater && d > 0.001) {
                inWater = true;


                vec3 pA = pos - step;
                vec3 pB = pos;
                for (int b = 0; b < 4; ++b) {
                    vec3 pMid = 0.5 * (pA + pB);
                    if (sampleDensity(pMid) > 0.001) pB = pMid;
                    else pA = pMid;
                }
                pos = 0.5 * (pA + pB);

                vec3 n;
                vec3 g = densityNormal(pos);
                if (pos.x <= -0.98 || pos.x >= 0.98 || pos.z <= -0.98 || pos.z >= 0.98 || pos.y <= -0.98) {
                    n = -boxNormal(pos);
                } else if (length(g) > 1e-4) {
                    n = normalize(g);
                } else {
                    n = vec3(0.0, 1.0, 0.0);
                }

                if (dot(n, rd) > 0.0) n = -n;


                float cosTheta = clamp(dot(-rd, n), 0.0, 1.0);
                float fres = 0.02 + 0.98 * pow(1.0 - cosTheta, 5.0);


                vec3 reflCol = background(pos, reflect(rd, n), 0.0);
                acc += T * fres * reflCol;


                vec3 ldir = normalize(LIGHT_POS - pos);
                vec3 hv   = normalize(ldir - rd);
                float spec = pow(clamp(dot(n, hv), 0.0, 1.0), 180.0);
              acc += T * spec * LIGHT_COLOR * uSpecular;


                T *= (1.0 - fres);


                vec3 rf = refract(rd, n, 1.0 / IOR);
                refrDir = (dot(rf, rf) > 1e-6) ? normalize(rf) : reflect(rd, n);
            }
          else if (inWater && d <= 0.001) {
                vec3 pA = pos - step;
                vec3 pB = pos;
                for (int b = 0; b < 3; ++b) {
                    vec3 pMid = 0.5 * (pA + pB);
                    if (sampleDensity(pMid) <= 0.001) pB = pMid;
                    else pA = pMid;
                }
                pos = 0.5 * (pA + pB);
                exitPos = pos;
                haveExit = true;
                vec3 g = densityNormal(pos);
                if (length(g) > 1e-4) {
                    vec3 n = normalize(g);
                    if (dot(n, refrDir) > 0.0) n = -n;
                    vec3 outDir = refract(refrDir, n, IOR);

                    refrDir = (dot(outDir, outDir) > 1e-6) ? normalize(outDir) : reflect(refrDir, n);
                }
                break;
            }

            if (inWater) {
           vec3 scatter = uWaterTint * 0.05;
                if (d > 0.02) {
                    vec3  ldir  = normalize(LIGHT_POS - pos);
                    float lstep = 2.5 / float(LIGHT_STEPS);
                    vec3  lpos  = pos + ldir * lstep;
                    float Tl    = 1.0;
                    for (int s = 0; s < LIGHT_STEPS; ++s) {
                        float ld = sampleDensity(lpos);
                        Tl *= exp(-uAbsorption * 0.12 * ld * lstep);
                        if (Tl < 0.05) break;
                        lpos += ldir * lstep;
                    }
                    scatter += LIGHT_COLOR * Tl * 0.05;
                } else {
                    scatter += LIGHT_COLOR * 0.02;
                }

                float alpha = 1.0 - exp(-uAbsorption * 0.8 * d * stepLen);
                acc += T * alpha * scatter;
                T *= exp(-absCoeff * d * stepLen);
                if (all(lessThan(T, vec3(0.01)))) break;
            }
            pos += step;
        }

        if (inWater) {
            if (!haveExit) {
                exitPos = ro + rd * t1;
                vec3 n = -boxNormal(exitPos);
                vec3 outDir = refract(refrDir, n, IOR);
                if (dot(outDir, outDir) > 1e-6) refrDir = normalize(outDir);
            }
            vec3 bg = background(exitPos, refrDir, 0.35);
            float lum = clamp(dot(bg, vec3(0.299, 0.587, 0.114)), 0.0, 1.0);
      vec3 flatBlue = uWaterTint * (0.5 + 2.0 * lum);
            bg = mix(bg, flatBlue, 0.65);
            col = acc + T * bg;
        }
    }


    col = clamp((col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14), 0.0, 1.0);
    col = pow(col, vec3(1.0 / 2.2));


    float bayer = mod(gl_FragCoord.x + gl_FragCoord.y, 2.0) * 0.5 - 0.25;
    col += bayer * (0.5 / 255.0);

    FragColor = vec4(col, 1.0);
}
)GLSL";


static const char* SIMPLE_FRAG_SRC = R"GLSL(#version 300 es
precision mediump float;
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTex;
void main() {
    FragColor = texture(uTex, TexCoord);
}
)GLSL";

static const char* BG_FRAG_SRC = R"GLSL(#version 300 es
precision highp float;

in  vec2 TexCoord;
out vec4 FragColor;

uniform vec2  uResolution;
uniform vec3  uEye;
uniform vec3  uTarget;
uniform float uFov;

const vec3  LIGHT_POS   = vec3(2.5, 5.0, 2.0);
const vec3  LIGHT_COLOR = vec3(1.0, 0.98, 0.92);
const float FLOOR_Y     = -1.15;

vec3 rayDirection(vec2 uv, vec3 ro, vec3 target, float fovDeg) {
    vec3 f = normalize(target - ro);
    vec3 up = abs(f.y) > 0.99 ? vec3(1,0,0) : vec3(0,1,0);
    vec3 r = normalize(cross(f, up));
    vec3 u = cross(r, f);
    float focal = 1.0 / tan(radians(fovDeg) * 0.5);
    return normalize(f * focal + uv.x * r + uv.y * u);
}

vec3 background(vec3 ro, vec3 rd) {
    vec3 skyTop = vec3(0.16, 0.38, 0.80);
    vec3 skyHor = vec3(0.62, 0.78, 0.95);
    vec3 sunDir = normalize(LIGHT_POS);

    if (rd.y < -1e-4) {
        float t = (FLOOR_Y - ro.y) / rd.y;
        vec3 p = ro + rd * t;
        vec2 q = floor(p.xz * 2.0);
        float chk = mod(q.x + q.y, 2.0);
        vec3 col = mix(vec3(0.34), vec3(0.13), chk);

        float fog = clamp(1.0 - exp(-0.002 * t * t), 0.0, 1.0);
        return mix(col, skyHor, fog);
    }

    vec3 col = mix(skyHor, skyTop, pow(clamp(rd.y, 0.0, 1.0), 0.6));

    float sunDot = clamp(dot(rd, sunDir), 0.0, 1.0);
    col += LIGHT_COLOR * 0.30 * pow(sunDot, 32.0);
    col += LIGHT_COLOR * 0.90 * pow(sunDot, 256.0);
    col += LIGHT_COLOR * 3.0 * smoothstep(0.9992, 0.9997, sunDot);

    return col;
}

void main() {
    vec2 uv = (gl_FragCoord.xy * 2.0 - uResolution) / uResolution.y;
    vec3 rd = rayDirection(uv, uEye, uTarget, uFov);
    vec3 ro = uEye;

    vec3 col = background(ro, rd);

    col = clamp((col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14), 0.0, 1.0);
    col = pow(col, vec3(1.0 / 2.2));

    float bayer = mod(gl_FragCoord.x + gl_FragCoord.y, 2.0) * 0.5 - 0.25;
    col += bayer * (0.5 / 255.0);

    FragColor = vec4(col, 1.0);
}
)GLSL";

static const char* SPHERE_VERT_SRC = R"GLSL(#version 310 es
precision highp float;
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec3 aPos;
layout(location = 2) in vec3 aVel;

uniform mat4 uViewMatrix;
uniform mat4 uProjMatrix;
uniform float uGridN;
uniform float uWorldRadius;

out vec2 vUV;
out vec3 vViewPos;
out vec3 vVelocity;

void main() {
    vUV = aCorner;
    vec3 rawPos = aPos;
    vec3 rawVel = aVel;

    vec3 pPos = (rawPos / max(uGridN, 1.0) - 0.5) * 2.0;
    vVelocity = rawVel;

    vec4 viewCenter = uViewMatrix * vec4(pPos, 1.0);
    vViewPos = viewCenter.xyz;

    vec4 pos = viewCenter + vec4(aCorner * uWorldRadius, 0.0, 0.0);
    gl_Position = uProjMatrix * pos;
}
)GLSL";

static const char* SPHERE_FRAG_SRC = R"GLSL(#version 310 es
precision highp float;

in vec2 vUV;
in vec3 vViewPos;
in vec3 vVelocity;

uniform vec3  uWaterColor;
uniform float uWaterOpacity;
uniform int   uRenderStyle; // 0 = realistic, 1 = crystal, 2 = velocity heatmap

out vec4 FragColor;

void main() {
    vec2 uv = vUV;
    float r2 = dot(uv, uv);
    if (r2 > 1.0) discard;

    float z = sqrt(1.0 - r2);
    vec3 N = normalize(vec3(uv.x, uv.y, z));

    vec3 L = normalize(vec3(0.35, 0.85, 0.40));
    vec3 V = vec3(0.0, 0.0, 1.0);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 48.0);
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);

    vec3 baseCol = uWaterColor;

    if (uRenderStyle == 2) {
        float speed = length(vVelocity);
        float s = clamp(speed * 0.02, 0.0, 1.0);
        baseCol = mix(vec3(0.10, 0.45, 0.95), vec3(1.0, 0.28, 0.12), s);
    } else if (uRenderStyle == 1) {
        baseCol = mix(uWaterColor, vec3(0.85, 0.95, 1.0), fresnel * 0.6);
    }

    vec3 ambient  = baseCol * 0.40;
    vec3 diffuse  = baseCol * diff * 0.65;
    vec3 specular = vec3(1.0, 0.98, 0.95) * spec * 0.90;
    vec3 rim      = vec3(0.65, 0.88, 1.0) * fresnel * 0.50;

    vec3 finalCol = ambient + diffuse + specular + rim;

    float edgeSoft = smoothstep(1.0, 0.85, r2);
    float alpha = clamp(uWaterOpacity * (0.6 + 0.4 * fresnel) * edgeSoft, 0.05, 1.0);

    FragColor = vec4(finalCol, alpha);
}
)GLSL";

void GLRenderer::setRenderScale(float scale) {
    _renderScale = scale;
    if (_width > 0 && _height > 0) {
        _renderW = static_cast<int>(_width * _renderScale);
        _renderH = static_cast<int>(_height * _renderScale);
        setupFBO();
    }
}

void GLRenderer::drawScene(int targetW, int targetH) {
    _shader.use();

    glUniform1i(glGetUniformLocation(_shader.program, "uTex"), 0);

    glUniform2f(
            glGetUniformLocation(_shader.program, "uResolution"),
            static_cast<float>(targetW),
            static_cast<float>(targetH)
    );

    const float cp = std::cos(_pitch);
    const float sp = std::sin(_pitch);
    const float cy = std::cos(_yaw);
    const float sy = std::sin(_yaw);

    const float eyeX = _dist * cp * sy;
    const float eyeY = _dist * sp;
    const float eyeZ = _dist * cp * cy;

    glUniform3f(glGetUniformLocation(_shader.program, "uEye"), eyeX, eyeY, eyeZ);
    glUniform3f(glGetUniformLocation(_shader.program, "uTarget"), 0.0f, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(_shader.program, "uFov"), 45.0f);

    glUniform1f(glGetUniformLocation(_shader.program, "uAbsorption"), _absorption);
    glUniform3f(glGetUniformLocation(_shader.program, "uWaterTint"), _waterR, _waterG, _waterB);
    glUniform1f(glGetUniformLocation(_shader.program, "uSpecular"), _specular);
    glUniform1i(glGetUniformLocation(_shader.program, "uVolSteps"), _volSteps);

    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void GLRenderer::setupFBO() {
    if (_renderW <= 0 || _renderH <= 0) return;

    if (_fbo == 0) glGenFramebuffers(1, &_fbo);
    if (_fboTexture == 0) glGenTextures(1, &_fboTexture);

    glBindTexture(GL_TEXTURE_2D, _fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, _renderW, _renderH, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, _fboTexture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "FBO incomplete: 0x%x", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "GLRenderer3D init complete. VAO=%d VBO=%d TEX3D=%d FBO=%d",
                        _vao, _vbo, _texture3D, _fbo);
    _initialized = true;
}



void GLRenderer::renderTexture3D(GLuint texture, int texW, int texH, int texD) {
    if (_shader.program == 0 || _vao == 0) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, texture);

    GLuint targetFbo = (_fbo != 0 && _renderScale < 1.0f) ? _fbo : 0;
    int targetW = (targetFbo != 0) ? _renderW : _width;
    int targetH = (targetFbo != 0) ? _renderH : _height;

    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glViewport(0, 0, targetW, targetH);

    drawScene(targetW, targetH);

    if (targetFbo != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, _width, _height);

        _simpleShader.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _fboTexture);

        glUniform1i(glGetUniformLocation(_simpleShader.program, "uTex"), 0);

        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
}

void GLRenderer::init(int width, int height) {
    _width = width;
    _height = height;
    _renderW = static_cast<int>(width * _renderScale);
    _renderH = static_cast<int>(height * _renderScale);

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Initializing GLRenderer3D %dx%d (render scale: %.2f)",
                        width, height, _renderScale);

    _shader.init(VERT_SRC, FRAG_SRC);
    if (_shader.program == 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Shader compilation FAILED!");
        return;
    }
    _bgShader.init(VERT_SRC, BG_FRAG_SRC);
    if (_bgShader.program == 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "BG shader compilation FAILED!");
        return;
    }

    _sphereShader.init(SPHERE_VERT_SRC, SPHERE_FRAG_SRC);
    if (_sphereShader.program == 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Sphere shader compilation FAILED!");
        return;
    }
    _simpleShader.init(VERT_SRC, SIMPLE_FRAG_SRC);
    if (_simpleShader.program == 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Simple shader compilation FAILED!");
        return;
    }

    float quad[] = {
            // x,    y,    u,   v
            -1.0f, -1.0f,  0.0f, 0.0f,
            1.0f, -1.0f,  1.0f, 0.0f,
            1.0f,  1.0f,  1.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
            1.0f,  1.0f,  1.0f, 1.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glBindVertexArray(_vao);

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glGenVertexArrays(1, &_particleVao);
    glBindVertexArray(_particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glBindVertexArray(0);
    glGenTextures(1, &_texture3D);
    glBindTexture(GL_TEXTURE_3D, _texture3D);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_3D, 0);

    if (_renderScale < 1.0f) {
        setupFBO();
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "GLRenderer3D init complete. VAO=%d VBO=%d TEX3D=%d FBO=%d",
                        _vao, _vbo, _texture3D, _fbo);
}

void GLRenderer::setCamera(float yaw, float pitch) {
    _yaw = yaw;
    if (pitch >  1.2f) pitch =  1.2f;
    if (pitch < -1.2f) pitch = -1.2f;
    _pitch = pitch;
}

void GLRenderer::render3D(
        const std::vector<std::uint8_t>& volume,
        int texW,
        int texH,
        int texD
) {
    if (_shader.program == 0 || _vao == 0 || _texture3D == 0) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, _texture3D);

    if (texW != _texW || texH != _texH || texD != _texD) {
        glTexImage3D(
                GL_TEXTURE_3D,
                0,
                GL_R8,
                texW,
                texH,
                texD,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                volume.data()
        );

        _texW = texW;
        _texH = texH;
        _texD = texD;
    } else {
        glTexSubImage3D(
                GL_TEXTURE_3D,
                0,
                0, 0, 0,
                texW,
                texH,
                texD,
                GL_RED,
                GL_UNSIGNED_BYTE,
                volume.data()
        );
    }

    GLuint targetFbo = (_fbo != 0 && _renderScale < 1.0f) ? _fbo : 0;
    int targetW = (targetFbo != 0) ? _renderW : _width;
    int targetH = (targetFbo != 0) ? _renderH : _height;

    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glViewport(0, 0, targetW, targetH);

    drawScene(targetW, targetH);

    if (targetFbo != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, _width, _height);

        _simpleShader.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _fboTexture);

        glUniform1i(glGetUniformLocation(_simpleShader.program, "uTex"), 0);

        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
}
void GLRenderer::renderParticles(GLuint particleSSBO, int numParticles, int gridN) {
    if (_sphereShader.program == 0 || _bgShader.program == 0 || _vao == 0) {
        return;
    }

    if (numParticles <= 0 || gridN <= 0) {
        return;
    }

    GLuint targetFbo = (_fbo != 0 && _renderScale < 1.0f) ? _fbo : 0;
    int targetW = (targetFbo != 0) ? _renderW : _width;
    int targetH = (targetFbo != 0) ? _renderH : _height;

    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glViewport(0, 0, targetW, targetH);

    float view[16];
    float proj[16];
    computeCameraMatrices(view, proj, targetW, targetH);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    _bgShader.use();

    glUniform2f(
            glGetUniformLocation(_bgShader.program, "uResolution"),
            static_cast<float>(targetW),
            static_cast<float>(targetH)
    );

    const float cp = std::cos(_pitch);
    const float sp = std::sin(_pitch);
    const float cy = std::cos(_yaw);
    const float sy = std::sin(_yaw);

    const float eyeX = _dist * cp * sy;
    const float eyeY = _dist * sp;
    const float eyeZ = _dist * cp * cy;

    glUniform3f(glGetUniformLocation(_bgShader.program, "uEye"), eyeX, eyeY, eyeZ);
    glUniform3f(glGetUniformLocation(_bgShader.program, "uTarget"), 0.0f, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(_bgShader.program, "uFov"), 45.0f);

    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);


    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    _sphereShader.use();

    glUniformMatrix4fv(
            glGetUniformLocation(_sphereShader.program, "uViewMatrix"),
            1,
            GL_FALSE,
            view
    );

    glUniformMatrix4fv(
            glGetUniformLocation(_sphereShader.program, "uProjMatrix"),
            1,
            GL_FALSE,
            proj
    );

    glUniform1f(glGetUniformLocation(_sphereShader.program, "uGridN"), static_cast<float>(gridN));

    float worldRadius = _mpmParticleRadius * 2.0f / static_cast<float>(gridN);
    glUniform1f(glGetUniformLocation(_sphereShader.program, "uWorldRadius"), worldRadius);

    glUniform3f(
            glGetUniformLocation(_sphereShader.program, "uWaterColor"),
            _mpmWaterR,
            _mpmWaterG,
            _mpmWaterB
    );

    glUniform1f(glGetUniformLocation(_sphereShader.program, "uWaterOpacity"), _mpmWaterOpacity);

    int styleCode = _mpmRenderStyle - 1;

    glUniform1i(glGetUniformLocation(_sphereShader.program, "uRenderStyle"), styleCode);


    glBindVertexArray(_particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, particleSSBO);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 80, (void*)0);
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 80, (void*)16);
    glVertexAttribDivisor(2, 1);

    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, numParticles);

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


    glDisable(GL_BLEND);


    if (targetFbo != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, _width, _height);

        _simpleShader.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _fboTexture);
        glUniform1i(glGetUniformLocation(_simpleShader.program, "uTex"), 0);

        glBindVertexArray(_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
}
void GLRenderer::resize(int width, int height) {
    _width = width;
    _height = height;
    _renderW = static_cast<int>(width * _renderScale);
    _renderH = static_cast<int>(height * _renderScale);

    if (_renderScale < 1.0f) {
        setupFBO();
    }
}

void GLRenderer::destroy() {
    _shader.destroy();
    _simpleShader.destroy();
    _bgShader.destroy();
    _sphereShader.destroy();
    if (_vao) {
        glDeleteVertexArrays(1, &_vao);
        _vao = 0;
    }
    if (_particleVao) {
        glDeleteVertexArrays(1, &_particleVao);
        _particleVao = 0;
    }
    if (_vbo) {
        glDeleteBuffers(1, &_vbo);
        _vbo = 0;
    }
    if (_texture3D) {
        glDeleteTextures(1, &_texture3D);
        _texture3D = 0;
    }
    if (_fbo) {
        glDeleteFramebuffers(1, &_fbo);
        _fbo = 0;
    }
    if (_fboTexture) {
        glDeleteTextures(1, &_fboTexture);
        _fboTexture = 0;
    }

    _initialized = false;
    __android_log_print(ANDROID_LOG_INFO, TAG, "GLRenderer3D destroyed");
}