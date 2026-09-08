package com.example.waterc

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.opengl.GLSurfaceView
import android.view.MotionEvent
import com.example.waterc.WatercSettings.Companion.effectiveGridSize
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
                simulation.init(settings.effectiveGridSize(), settings.simMode)

                if (settings.simMode == FluidSimulation.MODE_MPM) {
                    simulation.setParticleCount(settings.particleCount)
                }
            }
            override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                surfaceW = width
                surfaceH = height
                simulation.resize(width, height)
                simulation.applyVisualSettings(settings)

                if (settings.simMode == FluidSimulation.MODE_MPM) {
                    simulation.setParticleCount(settings.particleCount)
                }
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

        if (surfaceW <= 0 || surfaceH <= 0) return

        val needRecreate =
            newSettings.simMode != old.simMode ||
                    newSettings.effectiveGridSize() != old.effectiveGridSize()

        if (needRecreate) {
            queueEvent {
                simulation.destroy()
                simulation.init(newSettings.effectiveGridSize(), newSettings.simMode)

                if (newSettings.simMode == FluidSimulation.MODE_MPM) {
                    simulation.setParticleCount(newSettings.particleCount)
                }

                simulation.resize(surfaceW, surfaceH)
                simulation.applyVisualSettings(newSettings)
            }
        } else {
            queueEvent {
                if (
                    newSettings.simMode == FluidSimulation.MODE_MPM &&
                    newSettings.particleCount != old.particleCount
                ) {
                    simulation.setParticleCount(newSettings.particleCount)
                }

                simulation.applyVisualSettings(newSettings)
            }
        }
    }


    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (width <= 0 || height <= 0) return true

        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                lastTouchX = event.x
                lastTouchY = event.y
            }
            MotionEvent.ACTION_MOVE -> {
                val dx = event.x - lastTouchX
                val dy = event.y - lastTouchY

                if (settings.simMode == FluidSimulation.MODE_MPM) {
                    val g = settings.gridSize.toFloat()
                    val nx = event.x / width
                    val ny = event.y / height

                    val cx = nx * g
                    val cy = (1.0f - ny) * g * 0.75f
                    val cz = ny * g

                    val speedMult = settings.mpmTouchStrength * 1.8f

                    val fx = (dx / width) * g * speedMult
                    val fy = -(dy / height) * g * speedMult
                    val fz = 0f

                    val radius = 16.0f
                    simulation.setPointer(cx, cy, cz, fx, fy, fz, radius)
                } else {
                    simulation.touch(
                        event.x / width,
                        event.y / height,
                        dx / width,
                        dy / height
                    )
                }

                lastTouchX = event.x
                lastTouchY = event.y
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (settings.simMode == FluidSimulation.MODE_MPM) {
                    simulation.releasePointer()
                }
            }
        }
        return true
    }


    fun setGridSize(size: Int) {
        val newSettings = settings.copy(gridSize = size)
        applySettings(newSettings)
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

                if (settings.simMode == FluidSimulation.MODE_MPM) return

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

    fun cleanup() {
        stopSensors()
        queueEvent { simulation.destroy() }
    }
}
