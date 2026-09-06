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
    val gravityStrength: Float = DEFAULT_GRAVITY_STRENGTH,
    val damping: Float = DEFAULT_DAMPING,
    val tiltSensitivity: Float = DEFAULT_TILT_SENSITIVITY,
    val renderScale: Float = DEFAULT_RENDER_SCALE,
    val targetFps: Int = DEFAULT_TARGET_FPS,
    val gridSize: Int = DEFAULT_GRID_SIZE,
    val accelEnabled: Boolean = DEFAULT_ACCEL_ENABLED,
    val waterAmount: Float = DEFAULT_WATER_AMOUNT,
) {
    companion object {
        const val PREFS_NAME = "waterc_wallpaper"
        const val KEY_WATER_AMOUNT = "water_amount"
        const val DEFAULT_WATER_AMOUNT = 1.0f

        const val KEY_WATER_R = "water_r"
        const val KEY_WATER_G = "water_g"
        const val KEY_WATER_B = "water_b"

        const val KEY_ABSORPTION = "absorption"
        const val KEY_SPECULAR = "specular"
        const val KEY_VOL_STEPS = "vol_steps"

        const val KEY_GRAVITY_STRENGTH = "gravity_strength"
        const val KEY_DAMPING = "damping"
        const val KEY_TILT_SENSITIVITY = "tilt_sensitivity"

        const val KEY_RENDER_SCALE = "render_scale"
        const val KEY_TARGET_FPS = "target_fps"

        const val KEY_GRID_SIZE = "grid_size"
        const val KEY_ACCEL_ENABLED = "accel_enabled"


        const val DEFAULT_WATER_R = 0.069f
        const val DEFAULT_WATER_G = 0.181f
        const val DEFAULT_WATER_B = 0.314f

        const val DEFAULT_ABSORPTION = 3.4f
        const val DEFAULT_SPECULAR = 0.8f
        const val DEFAULT_VOL_STEPS = 32

        const val DEFAULT_GRAVITY_STRENGTH = 2.5f
        const val DEFAULT_DAMPING = 0.998f
        const val DEFAULT_TILT_SENSITIVITY = 1.0f

        const val DEFAULT_RENDER_SCALE = 0.5f
        const val DEFAULT_TARGET_FPS = 60

        const val DEFAULT_GRID_SIZE = 50
        const val DEFAULT_ACCEL_ENABLED = true


        const val DEFAULT_WATER_COLOR_ARGB = 0xFF122E50.toInt()

        fun prefs(context: Context): SharedPreferences =
            context.applicationContext.getSharedPreferences(
                PREFS_NAME,
                Context.MODE_PRIVATE
            )

        fun load(context: Context): WatercSettings {
            val p = prefs(context)

            return WatercSettings(
                waterR = p.getFloat(KEY_WATER_R, DEFAULT_WATER_R).coerceIn(0f, 1f),
                waterG = p.getFloat(KEY_WATER_G, DEFAULT_WATER_G).coerceIn(0f, 1f),
                waterB = p.getFloat(KEY_WATER_B, DEFAULT_WATER_B).coerceIn(0f, 1f),
                absorption = p.getFloat(KEY_ABSORPTION, DEFAULT_ABSORPTION).coerceIn(0f, 10f),
                specular = p.getFloat(KEY_SPECULAR, DEFAULT_SPECULAR).coerceIn(0f, 3f),
                volSteps = p.getInt(KEY_VOL_STEPS, DEFAULT_VOL_STEPS).coerceIn(8, 128),
                gravityStrength = p.getFloat(KEY_GRAVITY_STRENGTH, DEFAULT_GRAVITY_STRENGTH)
                    .coerceIn(0f, 8f),
                damping = p.getFloat(KEY_DAMPING, DEFAULT_DAMPING).coerceIn(0.9f, 1f),
                tiltSensitivity = p.getFloat(KEY_TILT_SENSITIVITY, DEFAULT_TILT_SENSITIVITY)
                    .coerceIn(0f, 3f),
                renderScale = p.getFloat(KEY_RENDER_SCALE, DEFAULT_RENDER_SCALE)
                    .coerceIn(0.25f, 1f),
                targetFps = p.getInt(KEY_TARGET_FPS, DEFAULT_TARGET_FPS).coerceIn(15, 120),
                gridSize = p.getInt(KEY_GRID_SIZE, DEFAULT_GRID_SIZE).coerceIn(16, 128),
                accelEnabled = p.getBoolean(KEY_ACCEL_ENABLED, DEFAULT_ACCEL_ENABLED),
                waterAmount = p.getFloat(KEY_WATER_AMOUNT, DEFAULT_WATER_AMOUNT)
                    .coerceIn(0.1f, 2.0f),
            )
        }

        fun save(
            context: Context,
            settings: WatercSettings,
            synchronous: Boolean = false,
        ) {
            val editor = prefs(context).edit()
            editor.putFloat(KEY_WATER_AMOUNT, settings.waterAmount)
            editor.putFloat(KEY_WATER_R, settings.waterR)
            editor.putFloat(KEY_WATER_G, settings.waterG)
            editor.putFloat(KEY_WATER_B, settings.waterB)

            editor.putFloat(KEY_ABSORPTION, settings.absorption)
            editor.putFloat(KEY_SPECULAR, settings.specular)
            editor.putInt(KEY_VOL_STEPS, settings.volSteps)

            editor.putFloat(KEY_GRAVITY_STRENGTH, settings.gravityStrength)
            editor.putFloat(KEY_DAMPING, settings.damping)
            editor.putFloat(KEY_TILT_SENSITIVITY, settings.tiltSensitivity)

            editor.putFloat(KEY_RENDER_SCALE, settings.renderScale)
            editor.putInt(KEY_TARGET_FPS, settings.targetFps)

            editor.putInt(KEY_GRID_SIZE, settings.gridSize)
            editor.putBoolean(KEY_ACCEL_ENABLED, settings.accelEnabled)

            if (synchronous) {
                editor.commit()
            } else {
                editor.apply()
            }
        }

        fun reset(context: Context, synchronous: Boolean = true) {
            prefs(context).edit().clear().apply()

            if (synchronous) {
                save(context, WatercSettings(), synchronous = true)
            } else {
                save(context, WatercSettings(), synchronous = false)
            }
        }
    }
}