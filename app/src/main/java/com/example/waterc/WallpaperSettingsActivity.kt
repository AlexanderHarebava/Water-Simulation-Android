package com.example.waterc

import android.R.attr.fontFamily
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.Space
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.Link
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign

import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.waterc.ui.theme.WatercTheme
import java.util.Locale

class WallpaperSettingsActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val initial = WatercSettings.load(this)
        setContent {
            WatercTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    SettingsScreen(
                        initial = initial,
                        onSave = { settings ->
                            WatercSettings.save(this, settings, synchronous = true)
                            finish()
                        }
                    )
                }
            }
        }
    }
}

@Composable
private fun SettingsScreen(
    initial: WatercSettings,
    onSave: (WatercSettings) -> Unit,
) {
    var mpmGridSize by remember { mutableIntStateOf(initial.mpmGridSize) }
    val context = LocalContext.current
    var gridSize by remember { mutableIntStateOf(initial.gridSize) }
    var accelEnabled by remember { mutableStateOf(initial.accelEnabled) }
    var waterR by remember { mutableFloatStateOf(initial.waterR) }
    var waterG by remember { mutableFloatStateOf(initial.waterG) }
    var waterB by remember { mutableFloatStateOf(initial.waterB) }
    var absorption by remember { mutableFloatStateOf(initial.absorption) }
    var specular by remember { mutableFloatStateOf(initial.specular) }
    var volSteps by remember { mutableIntStateOf(initial.volSteps) }
    var waterAmount by remember { mutableFloatStateOf(initial.waterAmount) }
    var gravityStrength by remember { mutableFloatStateOf(initial.gravityStrength) }
    var damping by remember { mutableFloatStateOf(initial.damping) }
    var tiltSensitivity by remember { mutableFloatStateOf(initial.tiltSensitivity) }
    var renderScale by remember { mutableFloatStateOf(initial.renderScale) }
    var targetFps by remember { mutableIntStateOf(initial.targetFps) }
    var simSpeed by remember { mutableFloatStateOf(initial.simSpeed) }
    var mpmWaterR by remember { mutableFloatStateOf(initial.mpmWaterR) }
    var mpmWaterG by remember { mutableFloatStateOf(initial.mpmWaterG) }
    var mpmWaterB by remember { mutableFloatStateOf(initial.mpmWaterB) }
    var mpmWaterOpacity by remember { mutableFloatStateOf(initial.mpmWaterOpacity) }
    var mpmParticleRadius by remember { mutableFloatStateOf(initial.mpmParticleRadius) }
    var mpmRenderStyle by remember { mutableIntStateOf(initial.mpmRenderStyle) }
    var mpmTouchStrength by remember { mutableFloatStateOf(initial.mpmTouchStrength) }
    var simMode by remember { mutableIntStateOf(initial.simMode) }
    var scenario by remember { mutableIntStateOf(initial.scenario) }
    var particleCount by remember { mutableIntStateOf(initial.particleCount) }

    fun currentSettings(): WatercSettings {
        return WatercSettings(
            waterR = waterR, waterG = waterG, waterB = waterB,
            absorption = absorption, specular = specular,
            simMode = simMode, scenario = scenario, simSpeed = simSpeed,
            volSteps = volSteps, gravityStrength = gravityStrength,
            damping = damping, tiltSensitivity = tiltSensitivity,
            renderScale = renderScale, targetFps = targetFps,
            gridSize = gridSize,
            mpmGridSize = mpmGridSize,
            mpmWaterR = mpmWaterR, mpmWaterG = mpmWaterG, mpmWaterB = mpmWaterB,
            mpmWaterOpacity = mpmWaterOpacity, mpmParticleRadius = mpmParticleRadius,
            mpmRenderStyle = mpmRenderStyle, mpmTouchStrength = mpmTouchStrength,
            accelEnabled = accelEnabled, waterAmount = waterAmount,
            particleCount = particleCount,
        )
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .statusBarsPadding()
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(20.dp)
    ) {
        Text(stringResource(R.string.settings_screen_title), fontSize = 22.sp)


        Section(stringResource(R.string.section_sim_mode)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                FilterChip(
                    selected = simMode == 0,
                    onClick = { simMode = 0 },
                    label = { Text(stringResource(R.string.mode_euler_gpu), fontSize = 12.sp) },
                    modifier = Modifier.weight(1f)
                )
                FilterChip(
                    selected = simMode == 1,
                    onClick = { simMode = 1 },
                    label = { Text(stringResource(R.string.mode_mls_mpm), fontSize = 12.sp) },
                    modifier = Modifier.weight(1f)
                )
            }
        }

        if (simMode == 0) {
            Text(stringResource(R.string.section_euler_settings),
                fontSize = 13.sp,
                color = MaterialTheme.colorScheme.primary)

            Section(stringResource(R.string.section_water_color)) {
                Text(stringResource(R.string.label_color_r, (waterR * 255).toInt()))
                Slider(value = waterR * 255f,
                    onValueChange = { waterR = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
                Text(stringResource(R.string.label_color_g, (waterG * 255).toInt()))
                Slider(value = waterG * 255f,
                    onValueChange = { waterG = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
                Text(stringResource(R.string.label_color_b, (waterB * 255).toInt()))
                Slider(value = waterB * 255f,
                    onValueChange = { waterB = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
            }

            Section(stringResource(R.string.section_grid_size)) {
                Text(stringResource(R.string.label_grid_size_value, gridSize))
                Slider(
                    value = gridSize.toFloat(),
                    onValueChange = { gridSize = it.toInt().coerceIn(16, 64) },
                    valueRange = 16f..64f,
                    modifier = Modifier.fillMaxWidth()
                )
                Text(
                    stringResource(R.string.hint_grid_size),
                    fontSize = 11.sp
                )
            }

            Section(stringResource(R.string.section_absorption)) {
                Text(stringResource(R.string.label_format_float_2, absorption))
                Slider(value = absorption,
                    onValueChange = { absorption = it.coerceIn(0f, 10f) },
                    valueRange = 0f..10f, modifier = Modifier.fillMaxWidth())
            }

            Section(stringResource(R.string.section_specular)) {
                Text(stringResource(R.string.label_format_float_2, specular))
                Slider(value = specular,
                    onValueChange = { specular = it.coerceIn(0f, 3f) },
                    valueRange = 0f..3f, modifier = Modifier.fillMaxWidth())
            }

            Section(stringResource(R.string.section_raymarch)) {
                Text(stringResource(R.string.label_fps_value, volSteps))
                Slider(value = volSteps.toFloat(),
                    onValueChange = { volSteps = it.toInt().coerceIn(8, 128) },
                    valueRange = 8f..128f, modifier = Modifier.fillMaxWidth())
            }

            Section(stringResource(R.string.section_damping)) {
                Text(stringResource(R.string.label_format_float_4, damping))
                Slider(value = damping,
                    onValueChange = { damping = it.coerceIn(0.9f, 1f) },
                    valueRange = 0.9f..1f, modifier = Modifier.fillMaxWidth())
            }
        }

        if (simMode == 1) {
            Text(stringResource(R.string.section_mpm_settings),
                fontSize = 13.sp,
                color = MaterialTheme.colorScheme.primary)

            Section(stringResource(R.string.section_mpm_grid_size)) {
                Text(stringResource(R.string.label_mpm_grid_size_value, mpmGridSize))
                Slider(
                    value = mpmGridSize.toFloat(),
                    onValueChange = { mpmGridSize = it.toInt().coerceIn(32, 80) },
                    valueRange = 32f..80f,
                    modifier = Modifier.fillMaxWidth()
                )
                Text(
                    stringResource(R.string.hint_mpm_grid_size),
                    fontSize = 11.sp
                )
            }

            Section(stringResource(R.string.section_scenario)) {
                Row(modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(
                        0 to R.string.scenario_dam_break,
                        1 to R.string.scenario_water_drop,
                        2 to R.string.scenario_dual_wave
                    ).forEach { (sc, labelRes) ->
                        FilterChip(
                            selected = scenario == sc,
                            onClick = { scenario = sc },
                            label = { Text(stringResource(labelRes), fontSize = 11.sp) },
                            modifier = Modifier.weight(1f))
                    }
                }
            }

            Section(stringResource(R.string.section_sim_speed)) {
                Text(stringResource(R.string.label_format_float_2, simSpeed))
                Slider(value = simSpeed,
                    onValueChange = { simSpeed = it.coerceIn(0.3f, 1.0f) },
                    valueRange = 0.3f..1.0f, modifier = Modifier.fillMaxWidth())
            }

            Section(stringResource(R.string.section_water_color)) {
                Text(stringResource(R.string.label_color_r, (mpmWaterR * 255).toInt()))
                Slider(value = mpmWaterR * 255f,
                    onValueChange = { mpmWaterR = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
                Text(stringResource(R.string.label_color_g, (mpmWaterG * 255).toInt()))
                Slider(value = mpmWaterG * 255f,
                    onValueChange = { mpmWaterG = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
                Text(stringResource(R.string.label_color_b, (mpmWaterB * 255).toInt()))
                Slider(value = mpmWaterB * 255f,
                    onValueChange = { mpmWaterB = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
            }

            Section(stringResource(R.string.section_mpm_opacity)) {
                Text(stringResource(R.string.label_format_float_2, mpmWaterOpacity))
                Slider(value = mpmWaterOpacity,
                    onValueChange = { mpmWaterOpacity = it.coerceIn(0.1f, 1f) },
                    valueRange = 0.1f..1f, modifier = Modifier.fillMaxWidth())
            }

            Section(stringResource(R.string.section_mpm_radius)) {
                Text(stringResource(R.string.label_format_float_2, mpmParticleRadius))
                Slider(value = mpmParticleRadius,
                    onValueChange = { mpmParticleRadius = it.coerceIn(0.35f, 1.1f) },
                    valueRange = 0.35f..1.1f, modifier = Modifier.fillMaxWidth())
            }

            Section(stringResource(R.string.section_mpm_render_style)) {
                Row(modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    listOf(
                        0 to R.string.style_volume,
                        1 to R.string.style_spheres,
                        2 to R.string.style_crystal,
                        3 to R.string.style_velocity
                    ).forEach { (style, labelRes) ->
                        FilterChip(
                            selected = mpmRenderStyle == style,
                            onClick = { mpmRenderStyle = style },
                            label = { Text(stringResource(labelRes), fontSize = 10.sp) },
                            modifier = Modifier.weight(1f))
                    }
                }
            }

            Section(stringResource(R.string.section_mpm_touch_strength)) {
                Text(stringResource(R.string.label_format_float_2, mpmTouchStrength))
                Slider(value = mpmTouchStrength,
                    onValueChange = { mpmTouchStrength = it.coerceIn(0.2f, 3f) },
                    valueRange = 0.2f..3f, modifier = Modifier.fillMaxWidth())
            }
        }

        Text(stringResource(R.string.section_common_settings),
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.primary)

        Section(stringResource(R.string.section_water_amount)) {
            Text(stringResource(R.string.label_format_multiplier, waterAmount))
            Slider(value = waterAmount,
                onValueChange = { waterAmount = it.coerceIn(0.1f, 2.0f) },
                valueRange = 0.1f..2.0f, modifier = Modifier.fillMaxWidth())
            Text(stringResource(R.string.hint_water_amount), fontSize = 11.sp)
        }

        Section(stringResource(R.string.section_gravity)) {
            Text(stringResource(R.string.label_format_float_2, gravityStrength))
            Slider(value = gravityStrength,
                onValueChange = { gravityStrength = it.coerceIn(0f, 8f) },
                valueRange = 0f..8f, modifier = Modifier.fillMaxWidth())
        }

        Section(stringResource(R.string.section_tilt)) {
            Text(stringResource(R.string.label_format_float_2, tiltSensitivity))
            Slider(value = tiltSensitivity,
                onValueChange = { tiltSensitivity = it.coerceIn(0f, 3f) },
                valueRange = 0f..3f, modifier = Modifier.fillMaxWidth())
        }

        Section(stringResource(R.string.section_render_scale)) {
            Text(stringResource(R.string.label_format_float_2, renderScale))
            Slider(value = renderScale,
                onValueChange = { renderScale = it.coerceIn(0.25f, 1f) },
                valueRange = 0.25f..1f, modifier = Modifier.fillMaxWidth())
        }

        Section(stringResource(R.string.section_target_fps)) {
            Text(stringResource(R.string.label_fps_value, targetFps))
            Slider(value = targetFps.toFloat(),
                onValueChange = { targetFps = it.toInt().coerceIn(15, 120) },
                valueRange = 15f..120f, modifier = Modifier.fillMaxWidth())
        }

        Section(stringResource(R.string.section_accelerometer)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Switch(checked = accelEnabled, onCheckedChange = { accelEnabled = it })
                Text(stringResource(R.string.hint_accel_toggle),
                    fontSize = 13.sp, modifier = Modifier.padding(start = 12.dp))
            }
        }


        Button(onClick = { onSave(currentSettings()) },
            modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.settings_save_button))
        }
        OutlinedButton(
            onClick = {
                WatercSettings.reset(context, synchronous = true)
                val d = WatercSettings()
                waterR = d.waterR; waterG = d.waterG; waterB = d.waterB
                absorption = d.absorption; specular = d.specular
                volSteps = d.volSteps; gravityStrength = d.gravityStrength
                damping = d.damping; simSpeed = d.simSpeed
                tiltSensitivity = d.tiltSensitivity; renderScale = d.renderScale
                targetFps = d.targetFps; gridSize = d.gridSize
                mpmGridSize = d.mpmGridSize; particleCount = d.particleCount
                mpmWaterR = d.mpmWaterR; mpmWaterG = d.mpmWaterG; mpmWaterB = d.mpmWaterB
                mpmWaterOpacity = d.mpmWaterOpacity; mpmParticleRadius = d.mpmParticleRadius
                mpmRenderStyle = d.mpmRenderStyle; mpmTouchStrength = d.mpmTouchStrength
                accelEnabled = d.accelEnabled; waterAmount = d.waterAmount
                simMode = d.simMode; scenario = d.scenario
            },
            modifier = Modifier.fillMaxWidth()
        ) {
            Text(stringResource(R.string.settings_reset_button))
        }

        Row(
            modifier = Modifier.fillMaxWidth()
                .clickable {
                    val intent = Intent(Intent.ACTION_VIEW,
                        Uri.parse("https://github.com/AlexanderHarebava"))
                    context.startActivity(intent)
                }
                .padding(top = 8.dp),
            horizontalArrangement = Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(imageVector = Icons.Default.Link, contentDescription = "GitHub",
                modifier = Modifier.size(18.dp),
                tint = MaterialTheme.colorScheme.primary)
            Text(text = stringResource(R.string.author_credit),
                fontSize = 14.sp, color = MaterialTheme.colorScheme.primary,
                textDecoration = TextDecoration.Underline,
                modifier = Modifier.padding(start = 8.dp))
        }

        ExpandableLicenseSection()
    }
}

@Composable
private fun ExpandableLicenseSection() {
    var expanded by remember { mutableStateOf(false) }

    Column(modifier = Modifier.padding(top = 16.dp)) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clickable { expanded = !expanded }
                .padding(vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text(
                text = "LICENSE",
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.primary
            )
            Icon(
                imageVector = Icons.Default.KeyboardArrowDown,
                contentDescription = if (expanded) "Collapse" else "Expand",
                modifier = Modifier.rotate(if (expanded) 180f else 0f)
            )
        }

        AnimatedVisibility(visible = expanded) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(
                    text = "Credit:",
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    textDecoration = TextDecoration.Underline,
                    modifier = Modifier.clickable {
                    }
                )
                val uriHandler = LocalUriHandler.current
                val licenseUrl = "https://github.com/tmarrec/fluid-simulation"

                Text(
                    text = licenseUrl,
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    textDecoration = TextDecoration.Underline,
                    modifier = Modifier.clickable {
                        try {
                            uriHandler.openUri(licenseUrl)
                        } catch (e: Exception) {

                            e.printStackTrace()
                        }
                    }
                )
                Spacer(modifier = Modifier.height(16.dp))
                Text(
                    text = "App License",
                    fontSize = 10.sp,
                    lineHeight = 14.sp,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.fillMaxWidth()
                )
                Text(
                    text = APACHE_LICENSE_TEXT,
                    fontSize = 10.sp,
                    lineHeight = 14.sp,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 8.dp)
                )
            }
        }
    }
}


private const val APACHE_LICENSE_TEXT = """
 MIT License

Copyright (c) 2026 Alexander Harebava

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""

@Composable
private fun Section(
    title: String,
    content: @Composable () -> Unit
) {
    Column(
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Text(
            title,
            fontSize = 15.sp,
            color = MaterialTheme.colorScheme.primary
        )
        content()
    }
}
