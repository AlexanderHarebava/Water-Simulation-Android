package com.example.waterc

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.service.wallpaper.WallpaperService
import android.util.Log
import android.view.SurfaceHolder

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

        override fun onCreate(surfaceHolder: SurfaceHolder?) {
            super.onCreate(surfaceHolder)
            setTouchEventsEnabled(false)
            Log.i(TAG, "Engine.onCreate")

            settings = WatercSettings.load(this@FluidWallpaperService)

            loadPrefs()
        }

        private fun loadPrefs() {
            settings = WatercSettings.load(this@FluidWallpaperService)
            Log.i(
                TAG,
                "loadPrefs: gridSize=${settings.gridSize}, accel=${settings.accelEnabled}"
            )
        }

        override fun onSurfaceCreated(holder: SurfaceHolder?) {
            super.onSurfaceCreated(holder)
            Log.i(TAG, "onSurfaceCreated")

            val saved = WatercSettings.load(this@FluidWallpaperService)

            val existing = eglThread

            if (existing != null && existing.isStarted()) {
                if (saved.gridSize != settings.gridSize) {
                    Log.i(
                        TAG,
                        "GridSize changed ${settings.gridSize} -> ${saved.gridSize}, recreating"
                    )

                    destroyThreadAndSim()

                    settings = saved
                    createThread(holder!!, settings)
                } else {
                    settings = saved
                    existing.settings = saved
                    existing.resume()
                }

                return
            }

            settings = saved
            createThread(holder!!, settings)
        }

        private fun createThread(holder: SurfaceHolder, settings: WatercSettings) {
            val sim = FluidSimulation().also { simulation = it }

            val thread = WallpaperEglThread(
                holder = holder,
                simulation = sim,
                settings = settings,
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

            if (newSettings.gridSize != settings.gridSize) {
                Log.i(
                    TAG,
                    "Visibility triggered gridSize change: " +
                            "${settings.gridSize} -> ${newSettings.gridSize}"
                )

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
                    sensorManager.registerListener(
                        this,
                        it,
                        SensorManager.SENSOR_DELAY_GAME
                    )
                }
            }
        }

        private fun stopSensors() {
            sensorManager.unregisterListener(this)
        }
    }
}