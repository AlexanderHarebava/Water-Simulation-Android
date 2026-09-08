#include "jni_bridge.h"
#include "simulation/Fluids.h"
#include "simulation/FluidsGPU.h"
#include "simulation/config.h"
#include "rendering/GLRenderer.h"
#include <android/log.h>
#include <memory>
#include <chrono>
#include <cmath>
#include "simulation/FluidsMPMGPU.h"

#define TAG "FluidJNI3D"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct FluidContext {
    int simMode = 1;
    int particleCount = 40000;

    std::unique_ptr<Fluids>         fluidCpu;
    std::unique_ptr<FluidsGPU>      fluidGpu;
    std::unique_ptr<FluidsMPMGPU>   fluidMpm;
    std::unique_ptr<GLRenderer>     renderer;
    int mpmRenderStyle = 0;
    int gridSize = 32;
    std::uint64_t iteration = 0;
    bool rendererInitialized = false;
    std::uint64_t frameCount = 0;
    double accSimMs  = 0.0;
    double accDrawMs = 0.0;
    float waterAmount = 1.0f;
    float gravityX = 0.0f;
    float gravityY = -2.0f;
    float gravityZ = 0.0f;
    float yaw   = 0.7f;
    float pitch = 0.35f;
    float dist  = 8.0f;
    float renderScale = 0.5f;
};

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_example_waterc_FluidSimulation_nativeInit(
        JNIEnv*, jobject, jint gridSize, jint simMode)
{
    auto* ctx = new FluidContext();
    initConfig(static_cast<std::uint16_t>(gridSize));
    ctx->gridSize = gridSize;
    ctx->simMode = simMode;

    if (ctx->simMode == 1) {
        ctx->fluidMpm = std::make_unique<FluidsMPMGPU>();
    } else if (ctx->simMode == 0) {
        ctx->fluidGpu = std::make_unique<FluidsGPU>();
    } else {
        ctx->fluidCpu = std::make_unique<Fluids>();
    }
    ctx->renderer = std::make_unique<GLRenderer>();
    return reinterpret_cast<jlong>(ctx);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeDestroy(JNIEnv*, jobject, jlong handle)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;
    if (ctx->fluidMpm)  { ctx->fluidMpm->destroy();  ctx->fluidMpm.reset(); }
    if (ctx->fluidGpu)  { ctx->fluidGpu->destroy();  ctx->fluidGpu.reset(); }
    if (ctx->fluidCpu)  { ctx->fluidCpu.reset(); }
    if (ctx->renderer)  { ctx->renderer->destroy();  ctx->renderer.reset(); }
    ctx->rendererInitialized = false;
    delete ctx;
    __android_log_print(ANDROID_LOG_INFO, TAG, "Context destroyed");
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeResize(
        JNIEnv*, jobject, jlong handle, jint w, jint h)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;

    if (!ctx->rendererInitialized) {


        if (!ctx->renderer->isInitialized()) {
            ctx->renderer->setRenderScale(ctx->renderScale);
            ctx->renderer->init(w, h);
            ctx->renderer->setCamera(ctx->yaw, ctx->pitch);
        } else {
            ctx->renderer->resize(w, h);
        }

        const int g = ctx->gridSize;
        if (ctx->simMode == 1) {
            if (!ctx->fluidMpm) ctx->fluidMpm = std::make_unique<FluidsMPMGPU>();
            ctx->fluidMpm->setParticleCount(ctx->particleCount);
            ctx->fluidMpm->setWaterAmount(ctx->waterAmount);
            ctx->fluidMpm->init(g, g, g, ctx->particleCount, w, h);
        } else if (ctx->simMode == 0) {
            if (!ctx->fluidGpu) ctx->fluidGpu = std::make_unique<FluidsGPU>();
            ctx->fluidGpu->setWaterAmount(ctx->waterAmount);
            ctx->fluidGpu->init(g, w, h);
        } else {
            if (!ctx->fluidCpu) ctx->fluidCpu = std::make_unique<Fluids>();
            ctx->fluidCpu->setWaterAmount(ctx->waterAmount);
        }

        ctx->rendererInitialized = true;
        __android_log_print(ANDROID_LOG_INFO, TAG,
                            "GLRenderer3D initialized (mode=%d, grid=%d, particles=%d)",
                            ctx->simMode, g, ctx->particleCount);
    } else {
        ctx->renderer->resize(w, h);
    }
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeStep(JNIEnv*, jobject, jlong handle, jlong)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;
    const auto t0 = std::chrono::steady_clock::now();

    if (ctx->simMode == 1 && ctx->fluidMpm) {
        ctx->fluidMpm->setGravity(ctx->gravityX, ctx->gravityY, ctx->gravityZ);
        ctx->fluidMpm->step();
    } else if (ctx->simMode == 0 && ctx->fluidGpu) {
        ctx->fluidGpu->setGravity(ctx->gravityX, ctx->gravityY, ctx->gravityZ);
        ctx->fluidGpu->step();
    } else if (ctx->fluidCpu) {
        ctx->fluidCpu->setGravity(ctx->gravityX, ctx->gravityY, ctx->gravityZ);
        ctx->fluidCpu->update(ctx->iteration);
    }
    ++ctx->iteration;

    const auto t1 = std::chrono::steady_clock::now();
    ctx->accSimMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeRender(JNIEnv*, jobject, jlong handle)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->rendererInitialized || !ctx->renderer) return;

    const auto t0 = std::chrono::steady_clock::now();

    if (ctx->simMode == 1 && ctx->fluidMpm) {
        if (ctx->mpmRenderStyle == 0) {

            ctx->renderer->renderTexture3D(
                    ctx->fluidMpm->densityTexture(),
                    ctx->fluidMpm->textureWidth(),
                    ctx->fluidMpm->textureHeight(),
                    ctx->fluidMpm->textureDepth());
        } else {

            ctx->renderer->renderParticles(
                    ctx->fluidMpm->particleBuffer(),
                    ctx->fluidMpm->particleCount(),
                    ctx->fluidMpm->textureWidth());
        }
    } else if (ctx->simMode == 0 && ctx->fluidGpu) {
        ctx->renderer->renderTexture3D(
                ctx->fluidGpu->densityTexture(),
                ctx->fluidGpu->textureWidth(),
                ctx->fluidGpu->textureHeight(),
                ctx->fluidGpu->textureDepth());
    } else if (ctx->fluidCpu) {
        ctx->renderer->render3D(
                ctx->fluidCpu->texture(),
                ctx->fluidCpu->textureWidth(),
                ctx->fluidCpu->textureHeight(),
                ctx->fluidCpu->textureDepth());
    }

    const auto t1 = std::chrono::steady_clock::now();
    ctx->accDrawMs += std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (++ctx->frameCount % 120 == 0) {
        __android_log_print(ANDROID_LOG_INFO, TAG,
                            "perf [%s]: sim=%.2f ms  draw=%.2f ms",
                            ctx->simMode == 1 ? "MPM" : (ctx->simMode == 0 ? "GPU" : "CPU"),
                            ctx->accSimMs / 120.0, ctx->accDrawMs / 120.0);
        ctx->accSimMs = 0.0;
        ctx->accDrawMs = 0.0;
    }
}
JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetMpmSimSpeed(
        JNIEnv*, jobject, jlong handle, jfloat value)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->fluidMpm) return;
    ctx->fluidMpm->setSimulationSpeed(value);
}
JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetMpmRenderStyle(
        JNIEnv*, jobject, jlong handle, jint style)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;

    ctx->mpmRenderStyle = style;
    if (ctx->renderer) ctx->renderer->setMpmRenderStyle(style);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetMpmWaterColor(
        JNIEnv*, jobject, jlong handle, jfloat r, jfloat g, jfloat b)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;
    ctx->renderer->setMpmWaterColor(r, g, b);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetMpmWaterOpacity(
        JNIEnv*, jobject, jlong handle, jfloat value)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;
    ctx->renderer->setMpmWaterOpacity(value);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetMpmParticleRadius(
        JNIEnv*, jobject, jlong handle, jfloat value)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;
    ctx->renderer->setMpmParticleRadius(value);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetMpmTouchStrength(
        JNIEnv*, jobject, jlong handle, jfloat value)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->fluidMpm) return;
    ctx->fluidMpm->setTouchStrength(value);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeTouch(
        JNIEnv*, jobject, jlong handle, jfloat, jfloat, jfloat dx, jfloat dy)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;
    ctx->yaw   += dx * 8.0f;
    ctx->pitch -= dy * 8.0f;
    if (ctx->pitch >  1.2f) ctx->pitch =  1.2f;
    if (ctx->pitch < -1.2f) ctx->pitch = -1.2f;
    if (ctx->yaw >  static_cast<float>(M_PI))  ctx->yaw -= 2.0f * static_cast<float>(M_PI);
    if (ctx->yaw < -static_cast<float>(M_PI))  ctx->yaw += 2.0f * static_cast<float>(M_PI);
    if (ctx->renderer) ctx->renderer->setCamera(ctx->yaw, ctx->pitch);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetGravity(
        JNIEnv*, jobject, jlong handle, jfloat gx, jfloat gy, jfloat gz)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;
    ctx->gravityX = gx;
    ctx->gravityY = gy;
    ctx->gravityZ = gz;
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetWaterColor(
        JNIEnv*, jobject, jlong handle, jfloat r, jfloat g, jfloat b)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;
    ctx->renderer->setWaterColor(r, g, b);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetAbsorption(
        JNIEnv*, jobject, jlong handle, jfloat value)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;
    ctx->renderer->setAbsorption(value);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetSpecular(
        JNIEnv*, jobject, jlong handle, jfloat value)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;
    ctx->renderer->setSpecular(value);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetRaymarchSteps(
        JNIEnv*, jobject, jlong handle, jint steps)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;
    ctx->renderer->setVolSteps(steps);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetDamping(
        JNIEnv*, jobject, jlong handle, jfloat value)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;
    if (ctx->fluidGpu) ctx->fluidGpu->setDamping(value);
    if (ctx->fluidCpu) ctx->fluidCpu->setDamping(value);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeRotateCamera(
        JNIEnv*, jobject, jlong handle, jfloat yawDelta, jfloat pitchDelta)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;
    ctx->yaw   += yawDelta;
    ctx->pitch += pitchDelta;
    if (ctx->pitch >  1.2f) ctx->pitch =  1.2f;
    if (ctx->pitch < -1.2f) ctx->pitch = -1.2f;
    if (ctx->yaw >  static_cast<float>(M_PI))  ctx->yaw -= 2.0f * static_cast<float>(M_PI);
    if (ctx->yaw < -static_cast<float>(M_PI))  ctx->yaw += 2.0f * static_cast<float>(M_PI);
    if (ctx->renderer) ctx->renderer->setCamera(ctx->yaw, ctx->pitch);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetRenderScale(
        JNIEnv*, jobject, jlong handle, jfloat scale)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;
    ctx->renderScale = scale;
    if (ctx->rendererInitialized) {
        ctx->renderer->setRenderScale(scale);
        __android_log_print(ANDROID_LOG_INFO, TAG, "Render scale set to %.2f", scale);
    }
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetWaterAmount(
        JNIEnv*, jobject, jlong handle, jfloat value)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;
    ctx->waterAmount = value;
    if (ctx->fluidCpu) ctx->fluidCpu->setWaterAmount(value);
    if (ctx->fluidGpu) ctx->fluidGpu->setWaterAmount(value);
    if (ctx->fluidMpm) ctx->fluidMpm->setWaterAmount(value);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetSimMode(
        JNIEnv*, jobject, jlong handle, jint mode)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || ctx->simMode == mode) return;

    __android_log_print(
            ANDROID_LOG_INFO,
            TAG,
            "Switching sim mode: %d -> %d",
            ctx->simMode,
            mode
    );

    if (ctx->fluidMpm) {
        ctx->fluidMpm->destroy();
        ctx->fluidMpm.reset();
    }

    if (ctx->fluidGpu) {
        ctx->fluidGpu->destroy();
        ctx->fluidGpu.reset();
    }

    if (ctx->fluidCpu) {
        ctx->fluidCpu.reset();
    }

    ctx->simMode = mode;
    ctx->iteration = 0;

    if (mode == 1) {
        ctx->fluidMpm = std::make_unique<FluidsMPMGPU>();
        ctx->fluidMpm->setParticleCount(ctx->particleCount);
        ctx->fluidMpm->setWaterAmount(ctx->waterAmount);
    } else if (mode == 0) {
        ctx->fluidGpu = std::make_unique<FluidsGPU>();
        ctx->fluidGpu->setWaterAmount(ctx->waterAmount);
    } else {
        ctx->fluidCpu = std::make_unique<Fluids>();
        ctx->fluidCpu->setWaterAmount(ctx->waterAmount);
    }

    ctx->rendererInitialized = false;
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetParticleCount(
        JNIEnv*, jobject, jlong handle, jint count)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;
    ctx->particleCount = count;
    if (ctx->fluidMpm) ctx->fluidMpm->setParticleCount(count);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetScenario(
        JNIEnv*, jobject, jlong handle, jint scenario)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->fluidMpm) return;
    ctx->fluidMpm->setScenario(scenario);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetPointer(
        JNIEnv*, jobject, jlong handle,
        jfloat cx, jfloat cy, jfloat cz,
        jfloat fx, jfloat fy, jfloat fz, jfloat radius)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->fluidMpm) return;
    ctx->fluidMpm->setPointer(cx, cy, cz, fx, fy, fz, radius);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeReleasePointer(
        JNIEnv*, jobject, jlong handle)
{
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->fluidMpm) return;
    ctx->fluidMpm->releasePointer();
}

}
