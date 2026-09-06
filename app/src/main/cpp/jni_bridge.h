
#pragma once

#include <jni.h>

#ifndef WATERC_JNI_BRIDGE_H
#define WATERC_JNI_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif



JNIEXPORT jlong JNICALL Java_com_example_waterc_FluidSimulation_nativeInit(JNIEnv*, jobject, jint);
JNIEXPORT void  JNICALL Java_com_example_waterc_FluidSimulation_nativeDestroy(JNIEnv*, jobject, jlong);
JNIEXPORT void  JNICALL Java_com_example_waterc_FluidSimulation_nativeStep(JNIEnv*, jobject, jlong, jlong);
JNIEXPORT void  JNICALL Java_com_example_waterc_FluidSimulation_nativeRender(JNIEnv*, jobject, jlong);
JNIEXPORT void  JNICALL Java_com_example_waterc_FluidSimulation_nativeResize(JNIEnv*, jobject, jlong, jint, jint);
JNIEXPORT void  JNICALL Java_com_example_waterc_FluidSimulation_nativeTouch(JNIEnv*, jobject, jlong, jfloat, jfloat, jfloat, jfloat);
JNIEXPORT void  JNICALL Java_com_example_waterc_FluidSimulation_nativeSetGravity(JNIEnv*, jobject, jlong, jfloat, jfloat, jfloat);
JNIEXPORT void  JNICALL Java_com_example_waterc_FluidSimulation_nativeRotateCamera(JNIEnv*, jobject, jlong, jfloat, jfloat);
JNIEXPORT void  JNICALL Java_com_example_waterc_FluidSimulation_nativeSetRenderScale(JNIEnv*, jobject, jlong, jfloat);
JNIEXPORT void JNICALL Java_com_example_waterc_FluidSimulation_nativeSetWaterColor(
        JNIEnv*, jobject, jlong, jfloat, jfloat, jfloat
);

JNIEXPORT void JNICALL Java_com_example_waterc_FluidSimulation_nativeSetAbsorption(
        JNIEnv*, jobject, jlong, jfloat
);
JNIEXPORT void JNICALL Java_com_example_waterc_FluidSimulation_nativeSetWaterAmount(
        JNIEnv*, jobject, jlong, jfloat
);
JNIEXPORT void JNICALL Java_com_example_waterc_FluidSimulation_nativeSetSpecular(
        JNIEnv*, jobject, jlong, jfloat
);

JNIEXPORT void JNICALL Java_com_example_waterc_FluidSimulation_nativeSetRaymarchSteps(
        JNIEnv*, jobject, jlong, jint
);

JNIEXPORT void JNICALL Java_com_example_waterc_FluidSimulation_nativeSetDamping(
        JNIEnv*, jobject, jlong, jfloat
);
#ifdef __cplusplus
}
#endif

#endif
