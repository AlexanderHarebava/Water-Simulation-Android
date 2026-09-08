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

// === ЗАМЕНА тела SettingsScreen ===
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

        // ══════════════════════════════════════════

        Section("Режим симуляции") {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                FilterChip(
                    selected = simMode == 0,
                    onClick = { simMode = 0 },
                    label = { Text("Euler (GPU)", fontSize = 12.sp) },
                    modifier = Modifier.weight(1f)
                )
                FilterChip(
                    selected = simMode == 1,
                    onClick = { simMode = 1 },
                    label = { Text("MLS-MPM", fontSize = 12.sp) },
                    modifier = Modifier.weight(1f)
                )
            }
        }

        if (simMode == 0) {
            Text("— Настройки Euler —",
                fontSize = 13.sp,
                color = MaterialTheme.colorScheme.primary)

            Section(stringResource(R.string.section_water_color)) {
                Text("R: ${(waterR * 255).toInt()}")
                Slider(value = waterR * 255f,
                    onValueChange = { waterR = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
                Text("G: ${(waterG * 255).toInt()}")
                Slider(value = waterG * 255f,
                    onValueChange = { waterG = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
                Text("B: ${(waterB * 255).toInt()}")
                Slider(value = waterB * 255f,
                    onValueChange = { waterB = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
            }

            Section("Разрешение сетки (Euler)") {
                Text("Grid size: $gridSize")

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
                Text(String.format(Locale.US, "%.2f", absorption))
                Slider(value = absorption,
                    onValueChange = { absorption = it.coerceIn(0f, 10f) },
                    valueRange = 0f..10f, modifier = Modifier.fillMaxWidth())
            }
            Section(stringResource(R.string.section_specular)) {
                Text(String.format(Locale.US, "%.2f", specular))
                Slider(value = specular,
                    onValueChange = { specular = it.coerceIn(0f, 3f) },
                    valueRange = 0f..3f, modifier = Modifier.fillMaxWidth())
            }
            Section(stringResource(R.string.section_raymarch)) {
                Text("$volSteps")
                Slider(value = volSteps.toFloat(),
                    onValueChange = { volSteps = it.toInt().coerceIn(8, 128) },
                    valueRange = 8f..128f, modifier = Modifier.fillMaxWidth())
            }
            Section(stringResource(R.string.section_damping)) {
                Text(String.format(Locale.US, "%.4f", damping))
                Slider(value = damping,
                    onValueChange = { damping = it.coerceIn(0.9f, 1f) },
                    valueRange = 0.9f..1f, modifier = Modifier.fillMaxWidth())
            }
        }


        if (simMode == 1) {
            Text("— Настройки MLS-MPM —",
                fontSize = 13.sp,
                color = MaterialTheme.colorScheme.primary)

            Section("Разрешение сетки MLS-MPM") {
                Text("MPM grid size: $mpmGridSize")

                Slider(
                    value = mpmGridSize.toFloat(),
                    onValueChange = { mpmGridSize = it.toInt().coerceIn(32, 80) },
                    valueRange = 32f..80f,
                    modifier = Modifier.fillMaxWidth()
                )

                Text(
                    "Влияет только на MLS-MPM. Не связано с Euler Grid size.",
                    fontSize = 11.sp
                )
            }

            Section("Сценарий") {
                Row(modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(0 to "Dam Break", 1 to "Water Drop", 2 to "Dual Wave")
                        .forEach { (sc, label) ->
                            FilterChip(
                                selected = scenario == sc,
                                onClick = { scenario = sc },
                                label = { Text(label, fontSize = 11.sp) },
                                modifier = Modifier.weight(1f))
                        }
                }
            }
            Section("Скорость симуляции") {
                Text(String.format(Locale.US, "%.2f", simSpeed))
                Slider(value = simSpeed,
                    onValueChange = { simSpeed = it.coerceIn(0.3f, 1.0f) },
                    valueRange = 0.3f..1.0f, modifier = Modifier.fillMaxWidth())
            }
            Section("Цвет воды") {
                Text("R: ${(mpmWaterR * 255).toInt()}")
                Slider(value = mpmWaterR * 255f,
                    onValueChange = { mpmWaterR = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
                Text("G: ${(mpmWaterG * 255).toInt()}")
                Slider(value = mpmWaterG * 255f,
                    onValueChange = { mpmWaterG = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
                Text("B: ${(mpmWaterB * 255).toInt()}")
                Slider(value = mpmWaterB * 255f,
                    onValueChange = { mpmWaterB = (it / 255f).coerceIn(0f, 1f) },
                    valueRange = 0f..255f, modifier = Modifier.fillMaxWidth())
            }
            Section("Прозрачность воды") {
                Text(String.format(Locale.US, "%.2f", mpmWaterOpacity))
                Slider(value = mpmWaterOpacity,
                    onValueChange = { mpmWaterOpacity = it.coerceIn(0.1f, 1f) },
                    valueRange = 0.1f..1f, modifier = Modifier.fillMaxWidth())
            }
            Section("Радиус частиц") {
                Text(String.format(Locale.US, "%.2f", mpmParticleRadius))
                Slider(value = mpmParticleRadius,
                    onValueChange = { mpmParticleRadius = it.coerceIn(0.35f, 1.1f) },
                    valueRange = 0.35f..1.1f, modifier = Modifier.fillMaxWidth())
            }
            Section("Стиль рендера") {
                Row(modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    listOf(0 to "Объём", 1 to "Сферы", 2 to "Кристалл", 3 to "Скорость")
                        .forEach { (style, label) ->
                            FilterChip(
                                selected = mpmRenderStyle == style,
                                onClick = { mpmRenderStyle = style },
                                label = { Text(label, fontSize = 10.sp) },
                                modifier = Modifier.weight(1f))
                        }
                }
            }
            Section("Сила тача") {
                Text(String.format(Locale.US, "%.2f", mpmTouchStrength))
                Slider(value = mpmTouchStrength,
                    onValueChange = { mpmTouchStrength = it.coerceIn(0.2f, 3f) },
                    valueRange = 0.2f..3f, modifier = Modifier.fillMaxWidth())
            }
        }

        Text("— Общие настройки —",
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.primary)

        Section(stringResource(R.string.section_water_amount)) {
            Text(String.format(Locale.US, "%.2f×", waterAmount))
            Slider(value = waterAmount,
                onValueChange = { waterAmount = it.coerceIn(0.1f, 2.0f) },
                valueRange = 0.1f..2.0f, modifier = Modifier.fillMaxWidth())
            Text(stringResource(R.string.hint_water_amount), fontSize = 11.sp)
        }
        Section(stringResource(R.string.section_gravity)) {
            Text(String.format(Locale.US, "%.2f", gravityStrength))
            Slider(value = gravityStrength,
                onValueChange = { gravityStrength = it.coerceIn(0f, 8f) },
                valueRange = 0f..8f, modifier = Modifier.fillMaxWidth())
        }
        Section(stringResource(R.string.section_tilt)) {
            Text(String.format(Locale.US, "%.2f", tiltSensitivity))
            Slider(value = tiltSensitivity,
                onValueChange = { tiltSensitivity = it.coerceIn(0f, 3f) },
                valueRange = 0f..3f, modifier = Modifier.fillMaxWidth())
        }
        Section(stringResource(R.string.section_render_scale)) {
            Text(String.format(Locale.US, "%.2f", renderScale))
            Slider(value = renderScale,
                onValueChange = { renderScale = it.coerceIn(0.25f, 1f) },
                valueRange = 0.25f..1f, modifier = Modifier.fillMaxWidth())
        }
        Section(stringResource(R.string.section_target_fps)) {
            Text("$targetFps")
            Slider(value = targetFps.toFloat(),
                onValueChange = { targetFps = it.toInt().coerceIn(15, 120) },
                valueRange = 15f..120f, modifier = Modifier.fillMaxWidth())
        }
        Section(stringResource(R.string.section_grid_size)) {
            Text("Grid size: $gridSize")
            Slider(value = gridSize.toFloat(),
                onValueChange = { gridSize = it.toInt().coerceIn(16, 128) },
                valueRange = 16f..128f, modifier = Modifier.fillMaxWidth())
            Text(stringResource(R.string.hint_grid_size), fontSize = 11.sp)
        }
        Section(stringResource(R.string.section_accelerometer)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Switch(checked = accelEnabled, onCheckedChange = { accelEnabled = it })
                Text(stringResource(R.string.hint_accel_toggle),
                    fontSize = 13.sp, modifier = Modifier.padding(start = 12.dp))
            }
        }

        // Кнопки
        Button(onClick = { onSave(currentSettings()) },
            modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.settings_save_button))
        }
        OutlinedButton(
            onClick = {
                WatercSettings.reset(context, synchronous = true)

                val d = WatercSettings()

                waterR = d.waterR
                waterG = d.waterG
                waterB = d.waterB
                absorption = d.absorption
                specular = d.specular
                volSteps = d.volSteps
                gravityStrength = d.gravityStrength
                damping = d.damping
                simSpeed = d.simSpeed
                tiltSensitivity = d.tiltSensitivity
                renderScale = d.renderScale
                targetFps = d.targetFps
                gridSize = d.gridSize
                mpmGridSize = d.mpmGridSize
                particleCount = d.particleCount
                mpmWaterR = d.mpmWaterR
                mpmWaterG = d.mpmWaterG
                mpmWaterB = d.mpmWaterB
                mpmWaterOpacity = d.mpmWaterOpacity
                mpmParticleRadius = d.mpmParticleRadius
                mpmRenderStyle = d.mpmRenderStyle
                mpmTouchStrength = d.mpmTouchStrength
                accelEnabled = d.accelEnabled
                waterAmount = d.waterAmount
                simMode = d.simMode
                scenario = d.scenario
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
                                 Apache License
                           Version 2.0, January 2004
                        http://www.apache.org/licenses/

   TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION

   1. Definitions.

      "License" shall mean the terms and conditions for use, reproduction,
      and distribution as defined by Sections 1 through 9 of this document.

      "Licensor" shall mean the copyright owner or entity authorized by
      the copyright owner that is granting the License.

      "Legal Entity" shall mean the union of the acting entity and all
      other entities that control, are controlled by, or are under common
      control with that entity. For the purposes of this definition,
      "control" means (i) the power, direct or indirect, to cause the
      direction or management of such entity, whether by contract or
      otherwise, or (ii) ownership of fifty percent (50%) or more of the
      outstanding shares, or (iii) beneficial ownership of such entity.

      "You" (or "Your") shall mean an individual or Legal Entity
      exercising permissions granted by this License.

      "Source" form shall mean the preferred form for making modifications,
      including but not limited to software source code, documentation
      source, and configuration files.

      "Object" form shall mean any form resulting from mechanical
      transformation or translation of a Source form, including but
      not limited to compiled object code, generated documentation,
      and conversions to other media types.

      "Work" shall mean the work of authorship, whether in Source or
      Object form, made available under the License, as indicated by a
      copyright notice that is included in or attached to the work
      (an example is provided in the Appendix below).

      "Derivative Works" shall mean any work, whether in Source or Object
      form, that is based on (or derived from) the Work and for which the
      editorial revisions, annotations, elaborations, or other modifications
      represent, as a whole, an original work of authorship. For the purposes
      of this License, Derivative Works shall not include works that remain
      separable from, or merely link (or bind by name) to the interfaces of,
      the Work and Derivative Works thereof.

      "Contribution" shall mean any work of authorship, including
      the original version of the Work and any modifications or additions
      to that Work or Derivative Works thereof, that is intentionally
      submitted to Licensor for inclusion in the Work by the copyright owner
      or by an individual or Legal Entity authorized to submit on behalf of
      the copyright owner. For the purposes of this definition, "submitted"
      means any form of electronic, verbal, or written communication sent
      to the Licensor or its representatives, including but not limited to
      communication on electronic mailing lists, source code control systems,
      and issue tracking systems that are managed by, or on behalf of, the
      Licensor for the purpose of discussing and improving the Work, but
      excluding communication that is conspicuously marked or otherwise
      designated in writing by the copyright owner as "Not a Contribution."

      "Contributor" shall mean Licensor and any individual or Legal Entity
      on behalf of whom a Contribution has been received by Licensor and
      subsequently incorporated within the Work.

   2. Grant of Copyright License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      copyright license to reproduce, prepare Derivative Works of,
      publicly display, publicly perform, sublicense, and distribute the
      Work and such Derivative Works in Source or Object form.

   3. Grant of Patent License. Subject to the terms and conditions of
      this License, each Contributor hereby grants to You a perpetual,
      worldwide, non-exclusive, no-charge, royalty-free, irrevocable
      (except as stated in this section) patent license to make, have made,
      use, offer to sell, sell, import, and otherwise transfer the Work,
      where such license applies only to those patent claims licensable
      by such Contributor that are necessarily infringed by their
      Contribution(s) alone or by combination of their Contribution(s)
      with the Work to which such Contribution(s) was submitted. If You
      institute patent litigation against any entity (including a
      cross-claim or counterclaim in a lawsuit) alleging that the Work
      or a Contribution incorporated within the Work constitutes direct
      or contributory patent infringement, then any patent licenses
      granted to You under this License for that Work shall terminate
      as of the date such litigation is filed.

   4. Redistribution. You may reproduce and distribute copies of the
      Work or Derivative Works thereof in any medium, with or without
      modifications, and in Source or Object form, provided that You
      meet the following conditions:

      (a) You must give any other recipients of the Work or
          Derivative Works a copy of this License; and

      (b) You must cause any modified files to carry prominent notices
          stating that You changed the files; and

      (c) You must retain, in the Source form of any Derivative Works
          that You distribute, all copyright, patent, trademark, and
          attribution notices from the Source form of the Work,
          excluding those notices that do not pertain to any part of
          the Derivative Works; and

      (d) If the Work includes a "NOTICE" text file as part of its
          distribution, then any Derivative Works that You distribute must
          include a readable copy of the attribution notices contained
          within such NOTICE file, excluding those notices that do not
          pertain to any part of the Derivative Works, in at least one
          of the following places: within a NOTICE text file distributed
          as part of the Derivative Works; within the Source form or
          documentation, if provided along with the Derivative Works; or,
          within a display generated by the Derivative Works, if and
          wherever such third-party notices normally appear. The contents
          of the NOTICE file are for informational purposes only and
          do not modify the License. You may add Your own attribution
          notices within Derivative Works that You distribute, alongside
          or as an addendum to the NOTICE text from the Work, provided
          that such additional attribution notices cannot be construed
          as modifying the License.

      You may add Your own copyright statement to Your modifications and
      may provide additional or different license terms and conditions
      for use, reproduction, or distribution of Your modifications, or
      for any such Derivative Works as a whole, provided Your use,
      reproduction, and distribution of the Work otherwise complies with
      the conditions stated in this License.

   5. Submission of Contributions. Unless You explicitly state otherwise,
      any Contribution intentionally submitted for inclusion in the Work
      by You to the Licensor shall be under the terms and conditions of
      this License, without any additional terms or conditions.
      Notwithstanding the above, nothing herein shall supersede or modify
      the terms of any separate license agreement you may have executed
      with Licensor regarding such Contributions.

   6. Trademarks. This License does not grant permission to use the trade
      names, trademarks, service marks, or product names of the Licensor,
      except as required for reasonable and customary use in describing the
      origin of the Work and reproducing the content of the NOTICE file.

   7. Disclaimer of Warranty. Unless required by applicable law or
      agreed to in writing, Licensor provides the Work (and each
      Contributor provides its Contributions) on an "AS IS" BASIS,
      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
      implied, including, without limitation, any warranties or conditions
      of TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
      PARTICULAR PURPOSE. You are solely responsible for determining the
      appropriateness of using or redistributing the Work and assume any
      risks associated with Your exercise of permissions under this License.

   8. Limitation of Liability. In no event and under no legal theory,
      whether in tort (including negligence), contract, or otherwise,
      unless required by applicable law (such as deliberate and grossly
      negligent acts) or agreed to in writing, shall any Contributor be
      liable to You for damages, including any direct, indirect, special,
      incidental, or consequential damages of any character arising as a
      result of this License or out of the use or inability to use the
      Work (including but not limited to damages for loss of goodwill,
      work stoppage, computer failure or malfunction, or any and all
      other commercial damages or losses), even if such Contributor
      has been advised of the possibility of such damages.

   9. Accepting Warranty or Additional Liability. While redistributing
      the Work or Derivative Works thereof, You may choose to offer,
      and charge a fee for, acceptance of support, warranty, indemnity,
      or other liability obligations and/or rights consistent with this
      License. However, in accepting such obligations, You may act only
      on Your own behalf and on Your sole responsibility, not on behalf
      of any other Contributor, and only if You agree to indemnify,
      defend, and hold each Contributor harmless for any liability
      incurred by, or claims asserted against, such Contributor by reason
      of your accepting any such warranty or additional liability.

   END OF TERMS AND CONDITIONS

   APPENDIX: How to apply the Apache License to your work.

      To apply the Apache License to your work, attach the following
      boilerplate notice, with the fields enclosed by brackets "[]"
      replaced with your own identifying information. (Don't include
      the brackets!)  The text should be enclosed in the appropriate
      comment syntax for the file format. We also recommend that a
      file or class name and description of purpose be included on the
      same "printed page" as the copyright notice for easier
      identification within third-party archives.

   Copyright 2026 Alexander Harebava

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
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