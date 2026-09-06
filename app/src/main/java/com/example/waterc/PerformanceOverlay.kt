package com.example.waterc

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shadow
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.util.Locale
import kotlin.math.roundToInt


@Composable
fun PerformanceOverlay(
    fpsMeter: FpsMeter,
    modifier: Modifier = Modifier,
    updateIntervalMs: Long = 500L,
) {
    val context = LocalContext.current
    val monitor = remember { PerformanceMonitor(context.applicationContext) }

    var stats by remember { mutableStateOf(PerformanceMonitor.Stats()) }
    var expanded by remember { mutableStateOf(true) }



    LaunchedEffect(updateIntervalMs) {
        while (true) {
            val fpsSnapshot = fpsMeter.snapshot(System.nanoTime())
            stats = withContext(Dispatchers.Default) {
                monitor.collect(fpsSnapshot)
            }
            delay(updateIntervalMs)
        }
    }

    Column(
        modifier = modifier
            .statusBarsPadding()


            .clickable { expanded = !expanded }
            .padding(start = 10.dp, top = 6.dp, end = 8.dp, bottom = 4.dp)
    ) {
        if (expanded) {
            Text(text = line("FPS", stats.fps.roundToInt().toString()), style = hudStyle)
            Text(text = line(stringResource(R.string.perf_label_frame), frameText(stats)), style = hudStyle)
            Text(text = line(stringResource(R.string.perf_label_cpu), percentText(stats.cpuPercent)), style = hudStyle)
            Text(text = line(stringResource(R.string.perf_label_gpu), percentText(stats.gpuPercent)), style = hudStyle)
            Text(text = line(stringResource(R.string.perf_label_ram), ramText(stats)), style = hudStyle)
            Text(text = line(stringResource(R.string.perf_label_battery), batteryText(stats)), style = hudStyle)
        } else {
            Text(text = line("FPS", stats.fps.roundToInt().toString()), style = hudStyle)
        }
    }
}







private val hudStyle = TextStyle(
    fontFamily = FontFamily.Monospace,
    fontSize = 10.sp,
    lineHeight = 14.sp,
    shadow = Shadow(color = Color(0xCC000000), blurRadius = 5f),
)



private val labelColor = Color(0x99FFFFFF)



private val valueColor = Color(0xF2FFFFFF)



private fun line(label: String, value: String): AnnotatedString =
    buildAnnotatedString {
        withStyle(SpanStyle(color = labelColor)) {
            append(String.format(Locale.US, "%-5s", label))
        }
        withStyle(SpanStyle(color = valueColor, fontWeight = FontWeight.Medium)) {
            append(value)
        }
    }


private fun frameText(stats: PerformanceMonitor.Stats): String {
    if (stats.fps <= 0f) return "-"
    return String.format(
        Locale.US,
        "%.1f ms (%.1f/%.1f)",
        stats.frameAvgMs,
        stats.frameMinMs,
        stats.frameMaxMs
    )
}


private fun percentText(value: Float): String =
    if (value < 0f) "N/A" else String.format(Locale.US, "%.0f%%", value)


private fun ramText(stats: PerformanceMonitor.Stats): String {
    if (stats.ramPercent < 0f || stats.ramUsedMb < 0f) return "N/A"
    return String.format(
        Locale.US,
        "%.1f%% · %.0f MB",
        stats.ramPercent,
        stats.ramUsedMb
    )
}


private fun batteryText(stats: PerformanceMonitor.Stats): String {
    if (stats.batteryLevel < 0) return "N/A"

    return buildString {
        append(stats.batteryLevel).append('%')

        if (stats.batteryCurrentMa != 0f) {
            append(String.format(Locale.US, " · %+.0f mA", stats.batteryCurrentMa))
        }

        when {
            stats.batteryDrainPerHour > 0f ->
                append(String.format(Locale.US, " · ~%.1f%%/h", stats.batteryDrainPerHour))

            stats.batteryCharging -> append(" · charging")
            stats.batteryFull -> append(" · charged")
        }
    }

}
