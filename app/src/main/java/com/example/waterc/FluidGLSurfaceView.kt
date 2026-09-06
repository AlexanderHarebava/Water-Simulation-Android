package com.example.waterc

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.opengl.GLSurfaceView
import android.view.MotionEvent
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10


class FluidGLSurfaceView(
    context: Context,
    initialSettings: WatercSettings
) : GLSurfaceView(context), SensorEventListener {

    private val simulation = FluidSimulation()


    var fpsMeter: FpsMeter? = null
    @Volatile
    private var settings: WatercSettings = initialSettings

    private var surfaceW = 0
    private var surfaceH = 0

    private val sensorManager =
        context.getSystemService(Context.SENSOR_SERVICE) as SensorManager

    private val accelerometer =
        sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)


    private val gyroscope =
        sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)

    private var lastTouchX = 0f
    private var lastTouchY = 0f

    init {
        setEGLContextClientVersion(3)
        setEGLConfigChooser(8, 8, 8, 8, 16, 0)

        setRenderer(object : Renderer {
            override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                simulation.init(settings.gridSize)
            }

            override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                surfaceW = width
                surfaceH = height

                simulation.resize(width, height)


                simulation.applyVisualSettings(settings)
            }

            override fun onDrawFrame(gl: GL10?) {
                fpsMeter?.onFrame(System.nanoTime())
                simulation.step()
                simulation.render()
            }
        })

        renderMode = RENDERMODE_CONTINUOUSLY

        startSensors()
    }

    fun applySettings(newSettings: WatercSettings) {
        val old = settings
        settings = newSettings

        if (newSettings.gridSize != old.gridSize) {
            setGridSize(newSettings.gridSize)
        } else if (surfaceW > 0 && surfaceH > 0) {
            queueEvent {
                simulation.applyVisualSettings(newSettings)
            }
        }
    }

    fun setGridSize(size: Int) {
        settings = settings.copy(gridSize = size)

        if (surfaceW > 0 && surfaceH > 0) {
            queueEvent {
                simulation.init(size)
                simulation.resize(surfaceW, surfaceH)
                simulation.applyVisualSettings(settings)
            }
        }
    }

    fun startSensors() {
        accelerometer?.let {
            sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_GAME)
        }
        gyroscope?.let {
            sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_GAME)
        }
    }

    fun stopSensors() {
        sensorManager.unregisterListener(this)
    }


    private var lowPassX = 0f
    private var lowPassY = -2f
    private var lowPassZ = 0f
    private val alpha = 0.15f


    private var lastGyroTimeNs = 0L
    private val gyroSensitivity = 1.0f
    private val gyroDeadZone = 0.05f

    override fun onSensorChanged(event: SensorEvent) {
        when (event.sensor.type) {
            Sensor.TYPE_ACCELEROMETER -> {
                if (!settings.accelEnabled) return

                val ax = event.values[0]
                val ay = event.values[1]
                val az = event.values[2]

                val scale = settings.gravityStrength * settings.tiltSensitivity

                val gx = -(ax / 9.81f) * scale
                val gy = -(ay / 9.81f) * scale
                val gz = -(az / 9.81f) * scale

                lowPassX = lowPassX + alpha * (gx - lowPassX)
                lowPassY = lowPassY + alpha * (gy - lowPassY)
                lowPassZ = lowPassZ + alpha * (gz - lowPassZ)

                simulation.setGravity(lowPassX, lowPassY, lowPassZ)
            }
            Sensor.TYPE_GYROSCOPE -> {
                val rx = event.values[0]
                val ry = event.values[1]

                val now = event.timestamp

                if (lastGyroTimeNs != 0L) {
                    val dt = (now - lastGyroTimeNs) / 1_000_000_000f

                    if (dt > 0f && dt < 0.5f) {
                        val mag = kotlin.math.sqrt(rx * rx + ry * ry)

                        if (mag > gyroDeadZone) {
                            val sensitivity = settings.tiltSensitivity

                            val yawDelta = -ry * dt * sensitivity
                            val pitchDelta = -rx * dt * sensitivity

                            simulation.rotateCamera(yawDelta, pitchDelta)
                        }
                    }
                }

                lastGyroTimeNs = now
            }
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                lastTouchX = event.x
                lastTouchY = event.y
            }

            MotionEvent.ACTION_MOVE -> {
                val dx = event.x - lastTouchX
                val dy = event.y - lastTouchY

                simulation.touch(
                    event.x / width,
                    event.y / height,
                    dx / width,
                    dy / height
                )

                lastTouchX = event.x
                lastTouchY = event.y
            }
        }
        return true
    }

    fun cleanup() {
        stopSensors()
        simulation.destroy()
    }
}
