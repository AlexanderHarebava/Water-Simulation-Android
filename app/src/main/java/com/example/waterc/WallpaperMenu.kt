package com.example.waterc

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.horizontalScroll
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
    onGridSizeSelected: (Int) -> Unit,
    onApplyClicked: (Int) -> Unit,
    onOpenSettings: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var expanded by remember { mutableStateOf(false) }
    var gridSize by remember { mutableIntStateOf(initialGridSize) }
    var textInput by remember { mutableStateOf(initialGridSize.toString()) }

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
                        text = stringResource(R.string.menu_grid_desc),

                        fontSize = 11.sp,
                        lineHeight = 15.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )

                    Text(
                        text = "Grid size: $gridSize",
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
                        label = { Text(stringResource(R.string.section_grid_size) + " (16–128)") },

                        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )

                    Button(
                        onClick = { onApplyClicked(gridSize) },
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