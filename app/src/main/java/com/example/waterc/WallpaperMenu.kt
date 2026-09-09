package com.example.waterc

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.*
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun WallpaperMenu(
    initialGridSize: Int,
    initialSimMode: Int,
    initialParticleCount: Int,
    initialMpmRenderStyle: Int,
    onGridSizeSelected: (Int) -> Unit,
    onSimModeSelected: (Int) -> Unit,
    onParticleCountSelected: (Int) -> Unit,
    onMpmRenderStyleSelected: (Int) -> Unit,
    onApplyClicked: () -> Unit,
    onOpenSettings: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var expanded by remember { mutableStateOf(false) }

    var gridSize       by remember(initialGridSize)       { mutableIntStateOf(initialGridSize) }
    var textInput      by remember(initialGridSize)       { mutableStateOf(initialGridSize.toString()) }
    var simMode        by remember(initialSimMode)        { mutableIntStateOf(initialSimMode) }
    var particleCount  by remember(initialParticleCount)  { mutableIntStateOf(initialParticleCount) }
    var mpmRenderStyle by remember(initialMpmRenderStyle) { mutableIntStateOf(initialMpmRenderStyle) }

    Column(
        modifier = modifier
            .navigationBarsPadding()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        if (expanded) {
            Surface(
                shape = MaterialTheme.shapes.large,
                tonalElevation = 6.dp,
                shadowElevation = 12.dp,
                color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.96f)
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    Text(
                        text = stringResource(R.string.menu_title),
                        fontSize = 18.sp,
                        color = MaterialTheme.colorScheme.onSurface
                    )

                    Text(
                        stringResource(R.string.section_sim_mode),
                        fontSize = 13.sp,
                        color = MaterialTheme.colorScheme.onSurface
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        FilterChip(
                            selected = simMode == 0,
                            onClick = {
                                simMode = 0
                                onSimModeSelected(0)
                            },
                            label = {
                                Text(
                                    stringResource(R.string.mode_euler_gpu),
                                    fontSize = 12.sp
                                )
                            },
                            modifier = Modifier.weight(1f)
                        )
                        FilterChip(
                            selected = simMode == 1,
                            onClick = {
                                simMode = 1
                                onSimModeSelected(1)
                            },
                            label = {
                                Text(
                                    stringResource(R.string.mode_mls_mpm),
                                    fontSize = 12.sp
                                )
                            },
                            modifier = Modifier.weight(1f)
                        )
                    }

                    if (simMode == 1) {
                        Text(
                            text = stringResource(R.string.section_mpm_water_style),
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.onSurface
                        )
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            FilterChip(
                                selected = mpmRenderStyle == 0,
                                onClick = {
                                    mpmRenderStyle = 0
                                    onMpmRenderStyleSelected(0)
                                },
                                label = {
                                    Text(
                                        stringResource(R.string.style_volume),
                                        fontSize = 12.sp
                                    )
                                },
                                modifier = Modifier.weight(1f)
                            )
                            FilterChip(
                                selected = mpmRenderStyle == 1,
                                onClick = {
                                    mpmRenderStyle = 1
                                    onMpmRenderStyleSelected(1)
                                },
                                label = {
                                    Text(
                                        stringResource(R.string.style_spheres),
                                        fontSize = 12.sp
                                    )
                                },
                                modifier = Modifier.weight(1f)
                            )
                        }

                        Text(
                            text = stringResource(R.string.section_particle_count),
                            fontSize = 13.sp,
                            color = MaterialTheme.colorScheme.onSurface
                        )
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            listOf(
                                25000 to R.string.particle_count_25k,
                                40000 to R.string.particle_count_40k,
                                70000 to R.string.particle_count_70k
                            ).forEach { (count, labelRes) ->
                                FilterChip(
                                    selected = particleCount == count,
                                    onClick = {
                                        particleCount = count
                                        onParticleCountSelected(count)
                                    },
                                    label = {
                                        Text(
                                            stringResource(labelRes),
                                            fontSize = 12.sp
                                        )
                                    },
                                    modifier = Modifier.weight(1f)
                                )
                            }
                        }
                    }

                    if (simMode == 0) {
                        Text(
                            text = stringResource(R.string.menu_grid_desc),
                            fontSize = 11.sp,
                            lineHeight = 15.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                        Text(
                            text = stringResource(R.string.label_grid_size_value, gridSize),
                            fontSize = 14.sp,
                            color = MaterialTheme.colorScheme.onSurface
                        )
                        Slider(
                            value = gridSize.toFloat(),
                            onValueChange = { v ->
                                gridSize = v.toInt().coerceIn(16, 128)
                                textInput = gridSize.toString()
                                onGridSizeSelected(gridSize)
                            },
                            valueRange = 16f..128f,
                            modifier = Modifier.fillMaxWidth()
                        )
                        OutlinedTextField(
                            value = textInput,
                            onValueChange = { newVal ->
                                textInput = newVal
                                val parsed = newVal.toIntOrNull()
                                if (parsed != null && parsed in 16..128) {
                                    gridSize = parsed
                                    onGridSizeSelected(gridSize)
                                }
                            },
                            label = {
                                Text(stringResource(R.string.section_grid_size) + " (16–128)")
                            },
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth()
                        )
                    }

                    Button(
                        onClick = { onApplyClicked() },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(stringResource(R.string.menu_apply_button))
                    }
                    OutlinedButton(
                        onClick = onOpenSettings,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(stringResource(R.string.menu_full_settings_button))
                    }
                }
            }
        }

        SmallFloatingActionButton(
            onClick = { expanded = !expanded },
            containerColor = MaterialTheme.colorScheme.primaryContainer,
            contentColor = MaterialTheme.colorScheme.onPrimaryContainer
        ) {
            Icon(
                imageVector = if (expanded) Icons.Filled.Close else Icons.Filled.Menu,
                contentDescription = stringResource(
                    if (expanded) R.string.menu_close else R.string.menu_open
                )
            )
        }
    }
}
