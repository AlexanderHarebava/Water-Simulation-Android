#include "jni_bridge.h"
#include "simulation/Fluids.h"
#include "simulation/FluidsGPU.h"
#include "simulation/config.h"
#include "rendering/GLRenderer.h"
#include <android/log.h>
#include <memory>
#include <chrono>
#include <cmath>

#define TAG "FluidJNI3D"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


struct FluidContext {
    bool useGpu = true;

    std::unique_ptr<Fluids>    fluidCpu;
    std::unique_ptr<FluidsGPU> fluidGpu;
    std::unique_ptr<GLRenderer> renderer;

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

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetWaterAmount(
        JNIEnv*, jobject, jlong handle, jfloat value
) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;

    ctx->waterAmount = value;

    if (ctx->fluidCpu) {
        ctx->fluidCpu->setWaterAmount(value);
    }

    if (ctx->fluidGpu) {
        ctx->fluidGpu->setWaterAmount(value);
    }
}



JNIEXPORT jlong JNICALL
Java_com_example_waterc_FluidSimulation_nativeInit(JNIEnv* env, jobject, jint gridSize) {
    auto* ctx = new FluidContext();
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "Init fluid sim 3D, grid=%d, GPU=%s",
                        gridSize, ctx->useGpu ? "true" : "false");
    initConfig(static_cast<std::uint16_t>(gridSize));


    ctx->gridSize = gridSize;

    if (ctx->useGpu) {
        ctx->fluidGpu = std::make_unique<FluidsGPU>();
    } else {
        ctx->fluidCpu = std::make_unique<Fluids>();
    }

    ctx->renderer = std::make_unique<GLRenderer>();
    ctx->iteration = 0;
    ctx->rendererInitialized = false;
    return reinterpret_cast<jlong>(ctx);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeDestroy(JNIEnv*, jobject, jlong handle) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;

    if (ctx->renderer)  ctx->renderer->destroy();
    if (ctx->fluidCpu)  ctx->fluidCpu.reset();
    if (ctx->fluidGpu)  { ctx->fluidGpu->destroy(); ctx->fluidGpu.reset(); }
    ctx->renderer.reset();
    ctx->rendererInitialized = false;

    delete ctx;
    __android_log_print(ANDROID_LOG_INFO, TAG, "Context destroyed");
}


JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeResize(
        JNIEnv*, jobject, jlong handle, jint w, jint h
) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;

    __android_log_print(ANDROID_LOG_INFO, TAG, "Resize[%p]: %d x %d",
                        static_cast<void*>(ctx), w, h);

    if (!ctx->rendererInitialized) {

        ctx->renderer->setRenderScale(ctx->renderScale);

        ctx->renderer->init(w, h);
        ctx->renderer->setCamera(ctx->yaw, ctx->pitch);

        if (ctx->useGpu && ctx->fluidGpu) {
            ctx->fluidGpu->init(ctx->gridSize, w, h);
        }

        ctx->rendererInitialized = true;

        __android_log_print(ANDROID_LOG_INFO, TAG, "GLRenderer3D initialized");
    } else {
        ctx->renderer->resize(w, h);
    }
}


JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeStep(JNIEnv*, jobject, jlong handle, jlong) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;

    if (ctx->useGpu) {
        if (!ctx->fluidGpu) return;
        const auto t0 = std::chrono::steady_clock::now();
        ctx->fluidGpu->setGravity(ctx->gravityX, ctx->gravityY, ctx->gravityZ);
        ctx->fluidGpu->step();
        ++ctx->iteration;
        const auto t1 = std::chrono::steady_clock::now();
        ctx->accSimMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    } else {
        if (!ctx->fluidCpu) return;
        const auto t0 = std::chrono::steady_clock::now();
        ctx->fluidCpu->setGravity(ctx->gravityX, ctx->gravityY, ctx->gravityZ);
        ctx->fluidCpu->update(ctx->iteration);
        ++ctx->iteration;
        const auto t1 = std::chrono::steady_clock::now();
        ctx->accSimMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
}







JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeRender(JNIEnv*, jobject, jlong handle) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->rendererInitialized || !ctx->renderer) return;

    const auto t0 = std::chrono::steady_clock::now();

    if (ctx->useGpu && ctx->fluidGpu) {
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
                            "perf [%s]: sim=%.2f ms  draw(cpu submit)=%.2f ms  (avg/120)",
                            ctx->useGpu ? "GPU" : "CPU",
                            ctx->accSimMs / 120.0, ctx->accDrawMs / 120.0);
        ctx->accSimMs  = 0.0;
        ctx->accDrawMs = 0.0;
    }
}





JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeTouch(
        JNIEnv*, jobject, jlong handle,
        jfloat, jfloat,
        jfloat dx, jfloat dy
) {
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
        JNIEnv*, jobject, jlong handle,
        jfloat gx, jfloat gy, jfloat gz
) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;
    ctx->gravityX = gx;
    ctx->gravityY = gy;
    ctx->gravityZ = gz;
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetWaterColor(
        JNIEnv*, jobject, jlong handle, jfloat r, jfloat g, jfloat b
) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;

    ctx->renderer->setWaterColor(r, g, b);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetAbsorption(
        JNIEnv*, jobject, jlong handle, jfloat value
) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;

    ctx->renderer->setAbsorption(value);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetSpecular(
        JNIEnv*, jobject, jlong handle, jfloat value
) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;

    ctx->renderer->setSpecular(value);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetRaymarchSteps(
        JNIEnv*, jobject, jlong handle, jint steps
) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;

    ctx->renderer->setVolSteps(steps);
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeSetDamping(
        JNIEnv*, jobject, jlong handle, jfloat value
) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx) return;

    if (ctx->fluidGpu) {
        ctx->fluidGpu->setDamping(value);
    }

    if (ctx->fluidCpu) {
        ctx->fluidCpu->setDamping(value);
    }
}

JNIEXPORT void JNICALL
Java_com_example_waterc_FluidSimulation_nativeRotateCamera(
        JNIEnv*, jobject, jlong handle,
        jfloat yawDelta, jfloat pitchDelta
) {
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
        JNIEnv*, jobject, jlong handle, jfloat scale
) {
    auto* ctx = reinterpret_cast<FluidContext*>(handle);
    if (!ctx || !ctx->renderer) return;

    ctx->renderScale = scale;

    if (ctx->rendererInitialized) {
        ctx->renderer->setRenderScale(scale);
        __android_log_print(ANDROID_LOG_INFO, TAG, "Render scale set to %.2f", scale);
    }
}
}
