package com.example.waterc

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.service.wallpaper.WallpaperService
import android.util.Log
import android.view.MotionEvent
import android.view.SurfaceHolder
import com.example.waterc.WatercSettings.Companion.effectiveGridSize

class FluidWallpaperService : WallpaperService() {
    companion object {
        private const val TAG = "FluidWallpaper"
    }

    override fun onCreateEngine(): Engine = FluidEngine()

    inner class FluidEngine : Engine(), SensorEventListener {
        private val sensorManager: SensorManager =
            getSystemService(Context.SENSOR_SERVICE) as SensorManager
        private val accelerometer: Sensor? =
            sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
        private var eglThread: WallpaperEglThread? = null
        private var simulation: FluidSimulation? = null
        private lateinit var settings: WatercSettings
        private var surfaceW = 0
        private var surfaceH = 0
        private var lowPassX = 0f
        private var lowPassY = -2f
        private var lowPassZ = 0f
        private val alpha = 0.15f


        @Volatile private var touchActive = false
        @Volatile private var touchX = 0f
        @Volatile private var touchY = 0f
        @Volatile private var touchDX = 0f
        @Volatile private var touchDY = 0f
        private var lastTouchX = 0f
        private var lastTouchY = 0f

        override fun onCreate(surfaceHolder: SurfaceHolder?) {
            super.onCreate(surfaceHolder)
            setTouchEventsEnabled(true)
            Log.i(TAG, "Engine.onCreate")
            settings = WatercSettings.load(this@FluidWallpaperService)
            loadPrefs()
        }

        private fun loadPrefs() {
            settings = WatercSettings.load(this@FluidWallpaperService)
            Log.i(TAG, "loadPrefs: gridSize=${settings.gridSize}, accel=${settings.accelEnabled}")
        }

        override fun onTouchEvent(event: MotionEvent) {
            super.onTouchEvent(event)
            if (settings.simMode != FluidSimulation.MODE_MPM) return
            if (surfaceW <= 0 || surfaceH <= 0) return

            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    lastTouchX = event.x
                    lastTouchY = event.y
                    touchX = event.x
                    touchY = event.y
                    touchDX = 0f
                    touchDY = 0f
                    touchActive = true
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = event.x - lastTouchX
                    val dy = event.y - lastTouchY
                    touchX = event.x
                    touchY = event.y
                    touchDX = touchDX * 0.5f + dx * 0.5f
                    touchDY = touchDY * 0.5f + dy * 0.5f
                    lastTouchX = event.x
                    lastTouchY = event.y
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    touchActive = false
                    touchDX = 0f
                    touchDY = 0f
                    simulation?.releasePointer()
                }
            }
        }



        fun applyTouchForce() {
            if (!touchActive) return
            val sim = simulation ?: return
            val w = surfaceW
            val h = surfaceH
            if (w <= 0 || h <= 0) return
            val g = settings.effectiveGridSize().toFloat()
            val nx = touchX / w
            val ny = touchY / h

            val cx = nx * g
            val cy = (1.0f - ny) * g
            val cz = g * 0.5f
            val radius = g * 0.4f

            val speed = kotlin.math.sqrt(touchDX * touchDX + touchDY * touchDY)
            if (speed > 0.5f) {
                val strength = settings.mpmTouchStrength * 8.0f
                val fx = (touchDX / w) * g * strength
                val fy = -(touchDY / h) * g * strength
                sim.setPointer(cx, cy, cz, fx, fy, 0f, radius)
            } else {
                val holdStrength = settings.mpmTouchStrength * 8.0f
                sim.setPointer(cx, cy, cz, 0f, -holdStrength * 0.3f, 0f, radius)
            }
            touchDX *= 0.7f
            touchDY *= 0.7f
        }

        override fun onSurfaceCreated(holder: SurfaceHolder?) {
            super.onSurfaceCreated(holder)
            Log.i(TAG, "onSurfaceCreated")
            val h = holder ?: return
            val saved = WatercSettings.load(this@FluidWallpaperService)
            val existing = eglThread
            if (existing != null && existing.isStarted()) {
                if (saved.effectiveGridSize() != settings.effectiveGridSize()) {
                    Log.i(TAG, "GridSize changed, recreating")
                    destroyThreadAndSim()
                    settings = saved
                    createThread(h, settings)
                } else {
                    settings = saved
                    existing.settings = saved
                    existing.resume()
                }
                return
            }
            settings = saved
            createThread(h, settings)
        }

        private fun createThread(holder: SurfaceHolder, settings: WatercSettings) {
            val sim = FluidSimulation().also { simulation = it }
            val thread = WallpaperEglThread(
                holder = holder,
                simulation = sim,
                settings = settings,
                touchForceCallback = { applyTouchForce() }
            )
            thread.start()
            if (surfaceW > 0 && surfaceH > 0) {
                thread.setSize(surfaceW, surfaceH)
            }
            eglThread = thread
        }

        private fun destroyThreadAndSim() {
            eglThread?.stop()
            eglThread = null
            simulation?.destroy()
            simulation = null
        }

        override fun onSurfaceChanged(
            holder: SurfaceHolder?,
            format: Int,
            width: Int,
            height: Int
        ) {
            super.onSurfaceChanged(holder, format, width, height)
            Log.i(TAG, "onSurfaceChanged ${width}x${height}")
            surfaceW = width
            surfaceH = height
            eglThread?.setSize(width, height)
        }

        override fun onVisibilityChanged(visible: Boolean) {
            super.onVisibilityChanged(visible)
            Log.i(TAG, "onVisibilityChanged visible=$visible")
            if (visible) {
                checkSettingsAndRecreateIfNeeded()
                startSensors()
                eglThread?.resume()
            } else {
                eglThread?.pause()
                stopSensors()
            }
        }

        private fun checkSettingsAndRecreateIfNeeded() {
            val newSettings = WatercSettings.load(this@FluidWallpaperService)
            if (newSettings.effectiveGridSize() != settings.effectiveGridSize()) {
                Log.i(TAG, "Visibility triggered gridSize change: " +
                        "${settings.gridSize} -> ${newSettings.gridSize}")
                val holder = this.surfaceHolder ?: return
                destroyThreadAndSim()
                settings = newSettings
                createThread(holder, newSettings)
                if (surfaceW > 0 && surfaceH > 0) {
                    eglThread?.setSize(surfaceW, surfaceH)
                }
            } else {
                settings = newSettings
                eglThread?.settings = newSettings
            }
        }

        override fun onSurfaceDestroyed(holder: SurfaceHolder?) {
            super.onSurfaceDestroyed(holder)
            Log.i(TAG, "onSurfaceDestroyed")
            eglThread?.markSurfaceDirty()
            eglThread?.pause()
        }

        override fun onDestroy() {
            super.onDestroy()
            Log.i(TAG, "onDestroy")
            stopSensors()
            destroyThreadAndSim()
        }

        override fun onSensorChanged(event: SensorEvent) {
            val sim = simulation ?: return
            if (event.sensor.type == Sensor.TYPE_ACCELEROMETER) {
                if (!settings.accelEnabled) return
                val ax = event.values[0]
                val ay = event.values[1]
                val az = event.values[2]
                val scale = settings.gravityStrength * settings.tiltSensitivity
                val gx = -(ax / 9.81f) * scale
                val gy = -(ay / 9.81f) * scale
                val gz = -(az / 9.81f) * scale
                lowPassX += alpha * (gx - lowPassX)
                lowPassY += alpha * (gy - lowPassY)
                lowPassZ += alpha * (gz - lowPassZ)
                sim.setGravity(lowPassX, lowPassY, lowPassZ)
            }
        }

        override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

        private fun startSensors() {
            if (settings.accelEnabled) {
                accelerometer?.let {
                    sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_GAME)
                }
            }
        }

        private fun stopSensors() {
            sensorManager.unregisterListener(this)
        }
    }
}
