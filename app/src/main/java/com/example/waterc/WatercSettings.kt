package com.example.waterc

import android.content.Context
import android.content.SharedPreferences

data class WatercSettings(
    val waterR: Float = DEFAULT_WATER_R,
    val waterG: Float = DEFAULT_WATER_G,
    val waterB: Float = DEFAULT_WATER_B,
    val absorption: Float = DEFAULT_ABSORPTION,
    val specular: Float = DEFAULT_SPECULAR,
    val volSteps: Int = DEFAULT_VOL_STEPS,
    val mpmGridSize: Int = DEFAULT_MPM_GRID_SIZE,
    val mpmWaterR: Float = DEFAULT_MPM_WATER_R,
    val mpmWaterG: Float = DEFAULT_MPM_WATER_G,
    val mpmWaterB: Float = DEFAULT_MPM_WATER_B,
    val mpmWaterOpacity: Float = DEFAULT_MPM_WATER_OPACITY,
    val mpmParticleRadius: Float = DEFAULT_MPM_PARTICLE_RADIUS,
    val mpmRenderStyle: Int = DEFAULT_MPM_RENDER_STYLE,
    val mpmTouchStrength: Float = DEFAULT_MPM_TOUCH_STRENGTH,
    val gravityStrength: Float = DEFAULT_GRAVITY_STRENGTH,
    val damping: Float = DEFAULT_DAMPING,
    val tiltSensitivity: Float = DEFAULT_TILT_SENSITIVITY,
    val renderScale: Float = DEFAULT_RENDER_SCALE,
    val targetFps: Int = DEFAULT_TARGET_FPS,
    val gridSize: Int = DEFAULT_GRID_SIZE,
    val simMode: Int = DEFAULT_SIM_MODE,
    val scenario: Int = DEFAULT_SCENARIO,
    val particleCount: Int = DEFAULT_PARTICLE_COUNT,
    val accelEnabled: Boolean = DEFAULT_ACCEL_ENABLED,
    val simSpeed: Float = DEFAULT_SIM_SPEED,
    val waterAmount: Float = DEFAULT_WATER_AMOUNT,
) : java.io.Serializable {

    companion object {
        const val PREFS_NAME = "waterc_wallpaper"
        const val PREFS_NAME_APPLIED = "waterc_wallpaper_applied"

        const val KEY_SIM_SPEED = "sim_speed"
        const val DEFAULT_SIM_SPEED = 1.0f

        const val KEY_WATER_R = "water_r"
        const val KEY_WATER_G = "water_g"
        const val KEY_WATER_B = "water_b"
        const val KEY_ABSORPTION = "absorption"
        const val KEY_SPECULAR = "specular"
        const val KEY_VOL_STEPS = "vol_steps"

        const val KEY_MPM_WATER_R = "mpm_water_r"
        const val KEY_MPM_WATER_G = "mpm_water_g"
        const val KEY_MPM_WATER_B = "mpm_water_b"
        const val KEY_MPM_WATER_OPACITY = "mpm_water_opacity"
        const val KEY_MPM_PARTICLE_RADIUS = "mpm_particle_radius"
        const val KEY_MPM_RENDER_STYLE = "mpm_render_style"
        const val KEY_MPM_TOUCH_STRENGTH = "mpm_touch_strength"
        const val KEY_WATER_AMOUNT = "water_amount"

        const val KEY_SIM_MODE = "sim_mode"
        const val KEY_SCENARIO = "scenario"
        const val KEY_PARTICLE_COUNT = "particle_count"
        const val KEY_GRAVITY_STRENGTH = "gravity_strength"
        const val KEY_DAMPING = "damping"
        const val KEY_TILT_SENSITIVITY = "tilt_sensitivity"
        const val KEY_RENDER_SCALE = "render_scale"
        const val KEY_TARGET_FPS = "target_fps"
        const val KEY_GRID_SIZE = "grid_size"
        const val KEY_ACCEL_ENABLED = "accel_enabled"
        const val KEY_MPM_GRID_SIZE = "mpm_grid_size"

        const val DEFAULT_WATER_R = 0.069f
        const val DEFAULT_WATER_G = 0.181f
        const val DEFAULT_WATER_B = 0.314f
        const val DEFAULT_ABSORPTION = 3.4f
        const val DEFAULT_SPECULAR = 0.8f
        const val DEFAULT_VOL_STEPS = 32
        const val DEFAULT_MPM_GRID_SIZE = 50
        const val DEFAULT_MPM_WATER_R = 0.12f
        const val DEFAULT_MPM_WATER_G = 0.60f
        const val DEFAULT_MPM_WATER_B = 0.95f
        const val DEFAULT_MPM_WATER_OPACITY = 0.85f
        const val DEFAULT_MPM_PARTICLE_RADIUS = 0.62f
        const val DEFAULT_MPM_RENDER_STYLE = 0
        const val DEFAULT_MPM_TOUCH_STRENGTH = 2.9f
        const val DEFAULT_SIM_MODE = 1
        const val DEFAULT_SCENARIO = 0
        const val DEFAULT_PARTICLE_COUNT = 40000
        const val DEFAULT_GRAVITY_STRENGTH = 2.5f
        const val DEFAULT_DAMPING = 0.998f
        const val DEFAULT_TILT_SENSITIVITY = 1.0f
        const val DEFAULT_RENDER_SCALE = 0.5f
        const val DEFAULT_TARGET_FPS = 60
        const val DEFAULT_GRID_SIZE = 50
        const val DEFAULT_ACCEL_ENABLED = true
        const val DEFAULT_WATER_AMOUNT = 1.0f

        fun prefs(context: Context): SharedPreferences =
            context.applicationContext.getSharedPreferences(
                PREFS_NAME,
                Context.MODE_PRIVATE
            )

        fun appliedPrefs(context: Context): SharedPreferences =
            context.applicationContext.getSharedPreferences(
                PREFS_NAME_APPLIED,
                Context.MODE_PRIVATE
            )

        fun load(context: Context): WatercSettings = read(prefs(context))

        fun loadApplied(context: Context): WatercSettings {
            val applied = appliedPrefs(context)
            return if (applied.contains(KEY_SIM_MODE)) {
                read(applied)
            } else {
                read(prefs(context))
            }
        }

        fun save(
            context: Context,
            settings: WatercSettings,
            synchronous: Boolean = false,
        ) = write(prefs(context), settings, synchronous)

        fun saveApplied(
            context: Context,
            settings: WatercSettings,
            synchronous: Boolean = false,
        ) = write(appliedPrefs(context), settings, synchronous)

        private fun read(p: SharedPreferences): WatercSettings {
            return WatercSettings(
                simSpeed = p.getFloat(KEY_SIM_SPEED, DEFAULT_SIM_SPEED).coerceIn(0.3f, 1.0f),
                waterR = p.getFloat(KEY_WATER_R, DEFAULT_WATER_R).coerceIn(0f, 1f),
                waterG = p.getFloat(KEY_WATER_G, DEFAULT_WATER_G).coerceIn(0f, 1f),
                waterB = p.getFloat(KEY_WATER_B, DEFAULT_WATER_B).coerceIn(0f, 1f),
                absorption = p.getFloat(KEY_ABSORPTION, DEFAULT_ABSORPTION).coerceIn(0f, 10f),
                specular = p.getFloat(KEY_SPECULAR, DEFAULT_SPECULAR).coerceIn(0f, 3f),
                volSteps = p.getInt(KEY_VOL_STEPS, DEFAULT_VOL_STEPS).coerceIn(8, 128),
                gridSize = p.getInt(KEY_GRID_SIZE, DEFAULT_GRID_SIZE).coerceIn(16, 128),
                mpmGridSize = p.getInt(KEY_MPM_GRID_SIZE, DEFAULT_MPM_GRID_SIZE).coerceIn(32, 80),
                mpmWaterR = p.getFloat(KEY_MPM_WATER_R, DEFAULT_MPM_WATER_R).coerceIn(0f, 1f),
                mpmWaterG = p.getFloat(KEY_MPM_WATER_G, DEFAULT_MPM_WATER_G).coerceIn(0f, 1f),
                mpmWaterB = p.getFloat(KEY_MPM_WATER_B, DEFAULT_MPM_WATER_B).coerceIn(0f, 1f),
                mpmWaterOpacity = p.getFloat(KEY_MPM_WATER_OPACITY, DEFAULT_MPM_WATER_OPACITY).coerceIn(0.1f, 1f),
                mpmParticleRadius = p.getFloat(KEY_MPM_PARTICLE_RADIUS, DEFAULT_MPM_PARTICLE_RADIUS).coerceIn(0.35f, 1.1f),
                mpmRenderStyle = p.getInt(KEY_MPM_RENDER_STYLE, DEFAULT_MPM_RENDER_STYLE).coerceIn(0, 3),
                mpmTouchStrength = p.getFloat(KEY_MPM_TOUCH_STRENGTH, DEFAULT_MPM_TOUCH_STRENGTH).coerceIn(0.2f, 3f),
                particleCount = p.getInt(KEY_PARTICLE_COUNT, DEFAULT_PARTICLE_COUNT).coerceIn(10000, 100000),
                simMode = p.getInt(KEY_SIM_MODE, DEFAULT_SIM_MODE).coerceIn(0, 1),
                scenario = p.getInt(KEY_SCENARIO, DEFAULT_SCENARIO).coerceIn(0, 2),
                gravityStrength = p.getFloat(KEY_GRAVITY_STRENGTH, DEFAULT_GRAVITY_STRENGTH).coerceIn(0f, 8f),
                damping = p.getFloat(KEY_DAMPING, DEFAULT_DAMPING).coerceIn(0.9f, 1f),
                tiltSensitivity = p.getFloat(KEY_TILT_SENSITIVITY, DEFAULT_TILT_SENSITIVITY).coerceIn(0f, 3f),
                renderScale = p.getFloat(KEY_RENDER_SCALE, DEFAULT_RENDER_SCALE).coerceIn(0.25f, 1f),
                targetFps = p.getInt(KEY_TARGET_FPS, DEFAULT_TARGET_FPS).coerceIn(15, 120),
                accelEnabled = p.getBoolean(KEY_ACCEL_ENABLED, DEFAULT_ACCEL_ENABLED),
                waterAmount = p.getFloat(KEY_WATER_AMOUNT, DEFAULT_WATER_AMOUNT).coerceIn(0.1f, 2.0f),
            )
        }

        private fun write(
            p: SharedPreferences,
            settings: WatercSettings,
            synchronous: Boolean,
        ) {
            val editor = p.edit()
            editor.putFloat(KEY_SIM_SPEED, settings.simSpeed)
            editor.putInt(KEY_MPM_GRID_SIZE, settings.mpmGridSize)
            editor.putFloat(KEY_WATER_R, settings.waterR)
            editor.putFloat(KEY_WATER_G, settings.waterG)
            editor.putFloat(KEY_WATER_B, settings.waterB)
            editor.putFloat(KEY_ABSORPTION, settings.absorption)
            editor.putFloat(KEY_SPECULAR, settings.specular)
            editor.putInt(KEY_VOL_STEPS, settings.volSteps)
            editor.putFloat(KEY_MPM_WATER_R, settings.mpmWaterR)
            editor.putFloat(KEY_MPM_WATER_G, settings.mpmWaterG)
            editor.putFloat(KEY_MPM_WATER_B, settings.mpmWaterB)
            editor.putFloat(KEY_MPM_WATER_OPACITY, settings.mpmWaterOpacity)
            editor.putFloat(KEY_MPM_PARTICLE_RADIUS, settings.mpmParticleRadius)
            editor.putInt(KEY_MPM_RENDER_STYLE, settings.mpmRenderStyle)
            editor.putFloat(KEY_MPM_TOUCH_STRENGTH, settings.mpmTouchStrength)
            editor.putFloat(KEY_WATER_AMOUNT, settings.waterAmount)
            editor.putInt(KEY_SIM_MODE, settings.simMode)
            editor.putInt(KEY_SCENARIO, settings.scenario)
            editor.putInt(KEY_PARTICLE_COUNT, settings.particleCount)
            editor.putFloat(KEY_GRAVITY_STRENGTH, settings.gravityStrength)
            editor.putFloat(KEY_DAMPING, settings.damping)
            editor.putFloat(KEY_TILT_SENSITIVITY, settings.tiltSensitivity)
            editor.putFloat(KEY_RENDER_SCALE, settings.renderScale)
            editor.putInt(KEY_TARGET_FPS, settings.targetFps)
            editor.putInt(KEY_GRID_SIZE, settings.gridSize)
            editor.putBoolean(KEY_ACCEL_ENABLED, settings.accelEnabled)
            if (synchronous) editor.commit() else editor.apply()
        }

        fun WatercSettings.effectiveGridSize(): Int {
            return if (simMode == FluidSimulation.MODE_MPM) mpmGridSize else gridSize
        }
    }
}
