package com.example.waterc

class FluidSimulation {
    companion object {
        init { System.loadLibrary("waterc-fluid") }

        const val MODE_GPU_EULER = 0
        const val MODE_MPM       = 1
    }

    private var handle: Long = 0L
    private var initialized: Boolean = false

    fun init(gridSize: Int = 32, simMode: Int = MODE_MPM) {
        if (initialized) destroy()
        handle = nativeInit(gridSize, simMode)
        initialized = handle != 0L
    }

    fun step() { if (initialized) nativeStep(handle, System.nanoTime()) }
    fun render() { if (initialized) nativeRender(handle) }
    fun resize(w: Int, h: Int) { if (initialized) nativeResize(handle, w, h) }

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

    fun destroy() {
        if (initialized) {
            nativeDestroy(handle)
            handle = 0L
            initialized = false
        }
    }

    fun setSimMode(mode: Int) {
        if (initialized) nativeSetSimMode(handle, mode)
    }

    fun setParticleCount(count: Int) {
        if (initialized) nativeSetParticleCount(handle, count)
    }

    fun setScenario(scenario: Int) {
        if (initialized) nativeSetScenario(handle, scenario)
    }

    fun setPointer(
        cx: Float, cy: Float, cz: Float,
        fx: Float, fy: Float, fz: Float, radius: Float
    ) {
        if (initialized) nativeSetPointer(handle, cx, cy, cz, fx, fy, fz, radius)
    }

    fun releasePointer() {
        if (initialized) nativeReleasePointer(handle)
    }

    // === Euler settings ===
    fun setWaterColor(r: Float, g: Float, b: Float) {
        if (initialized) nativeSetWaterColor(handle, r, g, b)
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

    fun setWaterAmount(value: Float) {
        if (initialized) nativeSetWaterAmount(handle, value)
    }

    // === MPM settings ===
    fun setMpmRenderStyle(style: Int) {
        if (initialized) nativeSetMpmRenderStyle(handle, style)
    }

    fun setMpmWaterColor(r: Float, g: Float, b: Float) {
        if (initialized) nativeSetMpmWaterColor(handle, r, g, b)
    }

    fun setMpmWaterOpacity(value: Float) {
        if (initialized) nativeSetMpmWaterOpacity(handle, value)
    }

    fun setMpmParticleRadius(value: Float) {
        if (initialized) nativeSetMpmParticleRadius(handle, value)
    }
    fun setMpmSimSpeed(value: Float) {
        if (initialized) nativeSetMpmSimSpeed(handle, value)
    }
    fun setMpmTouchStrength(value: Float) {
        if (initialized) nativeSetMpmTouchStrength(handle, value)
    }

    fun applyVisualSettings(settings: WatercSettings) {
        setRenderScale(settings.renderScale)

        if (settings.simMode == MODE_MPM) {
            setMpmWaterColor(
                settings.mpmWaterR,
                settings.mpmWaterG,
                settings.mpmWaterB
            )

            setMpmWaterOpacity(settings.mpmWaterOpacity)
            setMpmParticleRadius(settings.mpmParticleRadius)
            setMpmRenderStyle(settings.mpmRenderStyle)
            setMpmTouchStrength(settings.mpmTouchStrength)
            setMpmSimSpeed(settings.simSpeed)

            setWaterAmount(settings.waterAmount)
            setScenario(settings.scenario)
            setParticleCount(settings.particleCount)
        } else {
            setWaterColor(
                settings.waterR,
                settings.waterG,
                settings.waterB
            )

            setAbsorption(settings.absorption)
            setSpecular(settings.specular)
            setRaymarchSteps(settings.volSteps)
            setDamping(settings.damping)
            setWaterAmount(settings.waterAmount)
        }
    }

    // === JNI declarations ===
    private external fun nativeInit(gridSize: Int, simMode: Int): Long
    private external fun nativeDestroy(handle: Long)
    private external fun nativeStep(handle: Long, timestamp: Long)
    private external fun nativeRender(handle: Long)
    private external fun nativeResize(handle: Long, width: Int, height: Int)
    private external fun nativeTouch(handle: Long, x: Float, y: Float, dx: Float, dy: Float)
    private external fun nativeSetGravity(handle: Long, gx: Float, gy: Float, gz: Float)
    private external fun nativeRotateCamera(handle: Long, yawDelta: Float, pitchDelta: Float)
    private external fun nativeSetRenderScale(handle: Long, scale: Float)

    private external fun nativeSetWaterColor(handle: Long, r: Float, g: Float, b: Float)
    private external fun nativeSetAbsorption(handle: Long, value: Float)
    private external fun nativeSetSpecular(handle: Long, value: Float)
    private external fun nativeSetRaymarchSteps(handle: Long, steps: Int)
    private external fun nativeSetDamping(handle: Long, value: Float)
    private external fun nativeSetWaterAmount(handle: Long, value: Float)

    private external fun nativeSetSimMode(handle: Long, mode: Int)
    private external fun nativeSetParticleCount(handle: Long, count: Int)
    private external fun nativeSetScenario(handle: Long, scenario: Int)
    private external fun nativeSetPointer(
        handle: Long, cx: Float, cy: Float, cz: Float,
        fx: Float, fy: Float, fz: Float, radius: Float
    )
    private external fun nativeReleasePointer(handle: Long)

    private external fun nativeSetMpmSimSpeed(handle: Long, value: Float)
    private external fun nativeSetMpmRenderStyle(handle: Long, style: Int)
    private external fun nativeSetMpmWaterColor(handle: Long, r: Float, g: Float, b: Float)
    private external fun nativeSetMpmWaterOpacity(handle: Long, value: Float)
    private external fun nativeSetMpmParticleRadius(handle: Long, value: Float)
    private external fun nativeSetMpmTouchStrength(handle: Long, value: Float)
}