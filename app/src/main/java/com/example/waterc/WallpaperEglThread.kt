package com.example.waterc

import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.EGLContext
import android.opengl.EGLDisplay
import android.opengl.EGLSurface
import android.util.Log
import android.view.SurfaceHolder

class WallpaperEglThread(
    private val holder: SurfaceHolder,
    private val simulation: FluidSimulation,
    @Volatile var settings: WatercSettings,
) {
    companion object {
        private const val TAG = "WallpaperEGL"
        private const val EGL_RECORDABLE_ANDROID = 0x3142
        private const val EGL_OPENGL_ES2_BIT = 0x04
    }

    @Volatile private var running = false
    @Volatile private var paused = false
    @Volatile private var width: Int = 0
    @Volatile private var height: Int = 0
    @Volatile private var surfaceDirty = false

    private var thread: Thread? = null

    private var appliedSettings: WatercSettings? = null

    fun setSize(w: Int, h: Int) {
        width = w
        height = h
    }

    fun start() {
        if (running) return

        running = true
        paused = false
        surfaceDirty = false

        thread = Thread({ runLoop() }, "WallpaperEglThread").apply {
            isDaemon = true
            start()
        }
    }

    fun stop() {
        running = false
        paused = false
        thread?.interrupt()

        try {
            thread?.join(2000)
        } catch (_: InterruptedException) {
        }

        thread = null
    }

    fun pause() {
        paused = true
    }

    fun resume() {
        paused = false
    }

    fun markSurfaceDirty() {
        surfaceDirty = true
    }

    fun isStarted(): Boolean = running

    private fun runLoop() {
        val display: EGLDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
        if (display == EGL14.EGL_NO_DISPLAY) {
            Log.e(TAG, "eglGetDisplay failed")
            return
        }

        val version = IntArray(2)
        if (!EGL14.eglInitialize(display, version, 0, version, 1)) {
            Log.e(TAG, "eglInitialize failed")
            return
        }

        val configAttribs = intArrayOf(
            EGL14.EGL_RED_SIZE, 8,
            EGL14.EGL_GREEN_SIZE, 8,
            EGL14.EGL_BLUE_SIZE, 8,
            EGL14.EGL_ALPHA_SIZE, 8,
            EGL14.EGL_DEPTH_SIZE, 16,
            EGL14.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RECORDABLE_ANDROID, 1,
            EGL14.EGL_NONE
        )

        val configs = arrayOfNulls<EGLConfig>(1)
        val numConfigs = IntArray(1)

        if (!EGL14.eglChooseConfig(
                display, configAttribs, 0, configs, 0, 1, numConfigs, 0
            ) || numConfigs[0] == 0
        ) {
            val fallback = intArrayOf(
                EGL14.EGL_RED_SIZE, 8,
                EGL14.EGL_GREEN_SIZE, 8,
                EGL14.EGL_BLUE_SIZE, 8,
                EGL14.EGL_ALPHA_SIZE, 8,
                EGL14.EGL_DEPTH_SIZE, 16,
                EGL14.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL14.EGL_NONE
            )

            if (!EGL14.eglChooseConfig(
                    display, fallback, 0, configs, 0, 1, numConfigs, 0
                ) || numConfigs[0] == 0
            ) {
                Log.e(TAG, "eglChooseConfig failed")
                EGL14.eglTerminate(display)
                return
            }
        }

        val eglConfig = configs[0]!!

        val contextAttribs = intArrayOf(
            EGL14.EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL14.EGL_NONE
        )

        val context: EGLContext = EGL14.eglCreateContext(
            display,
            eglConfig,
            EGL14.EGL_NO_CONTEXT,
            contextAttribs,
            0
        )

        if (context == EGL14.EGL_NO_CONTEXT) {
            Log.e(TAG, "eglCreateContext failed")
            EGL14.eglTerminate(display)
            return
        }

        var eglSurface: EGLSurface = EGL14.EGL_NO_SURFACE
        var simInited = false
        var curWidth = 0
        var curHeight = 0

        try {
            while (running) {
                if (paused) {
                    try {
                        Thread.sleep(50)
                    } catch (_: InterruptedException) {
                        if (!running) break
                    }
                    continue
                }

                val wantW = width
                val wantH = height

                if (wantW <= 0 || wantH <= 0) {
                    try {
                        Thread.sleep(50)
                    } catch (_: InterruptedException) {
                    }
                    continue
                }

                val needRecreate = eglSurface == EGL14.EGL_NO_SURFACE ||
                        curWidth != wantW ||
                        curHeight != wantH ||
                        surfaceDirty

                if (needRecreate) {
                    surfaceDirty = false

                    EGL14.eglMakeCurrent(
                        display,
                        EGL14.EGL_NO_SURFACE,
                        EGL14.EGL_NO_SURFACE,
                        EGL14.EGL_NO_CONTEXT
                    )

                    if (eglSurface != EGL14.EGL_NO_SURFACE) {
                        EGL14.eglDestroySurface(display, eglSurface)
                        eglSurface = EGL14.EGL_NO_SURFACE
                    }

                    val surfaceAttribs = intArrayOf(EGL14.EGL_NONE)

                    eglSurface = EGL14.eglCreateWindowSurface(
                        display,
                        eglConfig,
                        holder,
                        surfaceAttribs,
                        0
                    )

                    if (eglSurface == EGL14.EGL_NO_SURFACE) {
                        Log.e(
                            TAG,
                            "eglCreateWindowSurface failed: 0x" +
                                    Integer.toHexString(EGL14.eglGetError())
                        )

                        try {
                            Thread.sleep(100)
                        } catch (_: InterruptedException) {
                        }

                        continue
                    }

                    if (!EGL14.eglMakeCurrent(
                            display,
                            eglSurface,
                            eglSurface,
                            context
                        )
                    ) {
                        Log.e(
                            TAG,
                            "eglMakeCurrent failed: 0x" +
                                    Integer.toHexString(EGL14.eglGetError())
                        )

                        EGL14.eglDestroySurface(display, eglSurface)
                        eglSurface = EGL14.EGL_NO_SURFACE

                        try {
                            Thread.sleep(100)
                        } catch (_: InterruptedException) {
                        }

                        continue
                    }

                    val qw = IntArray(1)
                    val qh = IntArray(1)

                    EGL14.eglQuerySurface(display, eglSurface, EGL14.EGL_WIDTH, qw, 0)
                    EGL14.eglQuerySurface(display, eglSurface, EGL14.EGL_HEIGHT, qh, 0)

                    val surfW = if (qw[0] > 0) qw[0] else wantW
                    val surfH = if (qh[0] > 0) qh[0] else wantH

                    curWidth = wantW
                    curHeight = wantH

                    if (!simInited) {
                        simulation.init(settings.gridSize)
                        simInited = true
                    }

                    simulation.resize(surfW, surfH)

                    Log.i(
                        TAG,
                        "EGL surface (re)created: ${surfW}x${surfH}"
                    )
                }

                if (eglSurface != EGL14.EGL_NO_SURFACE && simInited) {

                    if (appliedSettings != settings) {
                        simulation.applyVisualSettings(settings)
                        appliedSettings = settings
                    }

                    val frameStart = System.nanoTime()

                    try {
                        simulation.step()
                        simulation.render()

                        if (!EGL14.eglSwapBuffers(display, eglSurface)) {
                            Log.w(
                                TAG,
                                "eglSwapBuffers failed: 0x" +
                                        Integer.toHexString(EGL14.eglGetError())
                            )

                            EGL14.eglMakeCurrent(
                                display,
                                EGL14.EGL_NO_SURFACE,
                                EGL14.EGL_NO_SURFACE,
                                EGL14.EGL_NO_CONTEXT
                            )

                            EGL14.eglDestroySurface(display, eglSurface)

                            eglSurface = EGL14.EGL_NO_SURFACE
                            curWidth = 0
                            curHeight = 0

                            continue
                        }
                    } catch (t: Throwable) {
                        Log.e(TAG, "frame crash", t)
                    }

                    val frameEnd = System.nanoTime()
                    val elapsed = frameEnd - frameStart

                    val fps = settings.targetFps.coerceIn(15, 120).toLong()
                    val frameIntervalNs = 1_00_00_00L / fps

                    val sleepNs = frameIntervalNs - elapsed

                    if (sleepNs > 0) {
                        try {
                            Thread.sleep(sleepNs / 1_000_000L)
                        } catch (_: InterruptedException) {
                            if (!running) break
                        }
                    }
                }
            }
        } finally {
            EGL14.eglMakeCurrent(
                display,
                EGL14.EGL_NO_SURFACE,
                EGL14.EGL_NO_SURFACE,
                EGL14.EGL_NO_CONTEXT
            )

            if (eglSurface != EGL14.EGL_NO_SURFACE) {
                EGL14.eglDestroySurface(display, eglSurface)
            }

            EGL14.eglDestroyContext(display, context)
            EGL14.eglTerminate(display)
        }
    }
}