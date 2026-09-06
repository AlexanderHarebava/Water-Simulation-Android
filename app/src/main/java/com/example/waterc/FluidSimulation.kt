package com.example.waterc


class FluidSimulation {

    companion object {
        init {
            System.loadLibrary("waterc-fluid")
        }
    }

    private var handle: Long = 0L
    private var initialized: Boolean = false

    private external fun nativeSetWaterAmount(handle: Long, value: Float)

    fun init(gridSize: Int = 32) {

        if (initialized) destroy()
        handle = nativeInit(gridSize)
        initialized = handle != 0L
    }

    fun step() {
        if (initialized) nativeStep(handle, System.nanoTime())
    }

    fun render() {
        if (initialized) nativeRender(handle)
    }

    fun resize(width: Int, height: Int) {
        if (initialized) nativeResize(handle, width, height)
    }


    fun touch(x: Float, y: Float, dx: Float, dy: Float) {
        if (initialized) nativeTouch(handle, x, y, dx, dy)
    }


    fun setGravity(gx: Float, gy: Float, gz: Float) {
        if (initialized) nativeSetGravity(handle, gx, gy, gz)
    }


    fun rotateCamera(yawDelta: Float, pitchDelta: Float) {
        if (initialized) nativeRotateCamera(handle, yawDelta, pitchDelta)
    }

    fun setRenderScale(scale: Float) {
        if (initialized) nativeSetRenderScale(handle, scale)
    }

    private external fun nativeSetRenderScale(handle: Long, scale: Float)
    fun destroy() {
        if (initialized) {
            nativeDestroy(handle)
            handle = 0L
            initialized = false
        }
    }


    fun applyVisualSettings(settings: WatercSettings) {
        setWaterColor(settings.waterR, settings.waterG, settings.waterB)
        setAbsorption(settings.absorption)
        setSpecular(settings.specular)
        setRaymarchSteps(settings.volSteps)
        setDamping(settings.damping)
        setRenderScale(settings.renderScale)
        setWaterAmount(settings.waterAmount)
    }

    fun setWaterColor(r: Float, g: Float, b: Float) {
        if (initialized) nativeSetWaterColor(handle, r, g, b)
    }
    fun setWaterAmount(value: Float) {
        if (initialized) nativeSetWaterAmount(handle, value)
    }
    fun setAbsorption(value: Float) {
        if (initialized) nativeSetAbsorption(handle, value)
    }

    fun setSpecular(value: Float) {
        if (initialized) nativeSetSpecular(handle, value)
    }

    fun setRaymarchSteps(steps: Int) {
        if (initialized) nativeSetRaymarchSteps(handle, steps)
    }

    fun setDamping(value: Float) {
        if (initialized) nativeSetDamping(handle, value)
    }

    private external fun nativeSetWaterColor(handle: Long, r: Float, g: Float, b: Float)
    private external fun nativeSetAbsorption(handle: Long, value: Float)
    private external fun nativeSetSpecular(handle: Long, value: Float)
    private external fun nativeSetRaymarchSteps(handle: Long, steps: Int)
    private external fun nativeSetDamping(handle: Long, value: Float)

    private external fun nativeInit(gridSize: Int): Long
    private external fun nativeDestroy(handle: Long)
    private external fun nativeStep(handle: Long, timestamp: Long)
    private external fun nativeRender(handle: Long)
    private external fun nativeResize(handle: Long, width: Int, height: Int)
    private external fun nativeTouch(
        handle: Long, x: Float, y: Float, dx: Float, dy: Float
    )
    private external fun nativeSetGravity(handle: Long, gx: Float, gy: Float, gz: Float)
    private external fun nativeRotateCamera(handle: Long, yawDelta: Float, pitchDelta: Float)
}
