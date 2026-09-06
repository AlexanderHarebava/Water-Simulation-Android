package com.example.waterc

import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Debug
import android.os.Process
import android.os.SystemClock
import java.io.File
import kotlin.math.abs



class PerformanceMonitor(context: Context) {

    data class Stats(



        val fps: Float = 0f,


        val frameAvgMs: Float = 0f,


        val frameMinMs: Float = 0f,


        val frameMaxMs: Float = 0f,



        val cpuPercent: Float = -1f,


        val gpuPercent: Float = -1f,


        val ramPercent: Float = -1f,


        val ramUsedMb: Float = -1f,



        val batteryLevel: Int = -1,


        val batteryCurrentMa: Float = 0f,


        val batteryDrainPerHour: Float = -1f,


        val batteryCharging: Boolean = false,


        val batteryFull: Boolean = false,
    )

    private val appContext = context.applicationContext

    private val activityManager =
        appContext.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager

    private val batteryManager =
        appContext.getSystemService(Context.BATTERY_SERVICE) as BatteryManager



    private val totalRamBytes: Long = ActivityManager.MemoryInfo().let {
        activityManager.getMemoryInfo(it)
        it.totalMem
    }


    private var lastCpuMs = -1L
    private var lastWallMs = -1L


    private var smoothedCurrentMa = Float.NaN





    private val gpuBusyFiles = listOf(
        "/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage",
        "/sys/class/misc/mali0/device/utilisation",
        "/sys/kernel/gpu/gpu_busy",
        "/sys/kernel/gpu/gpu_utilization",
    )



    private var devfreqLoadFile: File? = null
    private var devfreqSearched = false



    fun collect(fps: FpsMeter.Snapshot): Stats {
        val ram = appRam()
        val battery = readBattery()

        return Stats(
            fps = fps.fps,
            frameAvgMs = fps.avgMs,
            frameMinMs = fps.minMs,
            frameMaxMs = fps.maxMs,
            cpuPercent = appCpuPercent(),
            gpuPercent = gpuPercent(),
            ramPercent = ram?.first ?: -1f,
            ramUsedMb = ram?.second ?: -1f,
            batteryLevel = battery.levelPercent,
            batteryCurrentMa = battery.currentMa,
            batteryDrainPerHour = battery.drainPerHour,
            batteryCharging = battery.charging,
            batteryFull = battery.full,
        )
    }







    private fun appCpuPercent(): Float {
        val cpuMs = Process.getElapsedCpuTime()
        val wallMs = SystemClock.elapsedRealtime()

        var result = -1f
        if (lastCpuMs >= 0 && wallMs > lastWallMs) {
            val dWall = wallMs - lastWallMs


            if (dWall <= 10_000L) {
                val dCpu = cpuMs - lastCpuMs
                if (dCpu >= 0) {
                    result = dCpu * 100f / dWall
                }
            }
        }

        lastCpuMs = cpuMs
        lastWallMs = wallMs
        return result
    }







    private fun gpuPercent(): Float {
        for (path in gpuBusyFiles) {
            parseGpuPercent(readFile(path))?.let { return it }
        }
        return devfreqGpuPercent() ?: -1f
    }

    private fun readFile(path: String): String? = try {
        File(path).readText()
    } catch (e: Exception) {
        null
    }



    private fun parseGpuPercent(text: String?): Float? {
        if (text.isNullOrBlank()) return null

        val numbers = Regex("-?\\d+")
            .findAll(text)
            .mapNotNull { it.value.toIntOrNull() }
            .toList()

        return when {

            numbers.size >= 2 && numbers[1] > 0 ->
                (numbers[0] * 100f / numbers[1]).coerceIn(0f, 100f)


            numbers.size == 1 && numbers[0] in 0..100 ->
                numbers[0].toFloat()

            else -> null
        }
    }



    private fun devfreqGpuPercent(): Float? {
        if (!devfreqSearched) {
            devfreqSearched = true
            devfreqLoadFile = searchDevfreqLoadFile()
        }

        val file = devfreqLoadFile ?: return null
        return parseGpuPercent(readFile(file.absolutePath))
    }

    private fun searchDevfreqLoadFile(): File? = try {
        File("/sys/devices/platform")
            .listFiles()
            ?.filter { it.isDirectory && it.name.endsWith(".gpu") }
            .orEmpty()
            .asSequence()
            .map { gpuDir -> File(gpuDir, "devfreq") }
            .filter { it.isDirectory }
            .mapNotNull { devfreq ->
                devfreq.listFiles()?.firstOrNull { sub ->
                    File(sub, "load").canRead()
                }
            }
            .firstOrNull()
    } catch (e: Exception) {
        null
    }







    private fun appRam(): Pair<Float, Float>? = try {
        val info = Debug.MemoryInfo()
        Debug.getMemoryInfo(info)

        val pssBytes = info.getTotalPss() * 1024L
        val usedMb = pssBytes / (1024f * 1024f)
        val percent = if (totalRamBytes > 0) {
            pssBytes * 100f / totalRamBytes
        } else {
            -1f
        }
        percent to usedMb
    } catch (e: Exception) {
        null
    }





    private class BatteryRead(
        val levelPercent: Int,


        val currentMa: Float,


        val drainPerHour: Float,
        val charging: Boolean,
        val full: Boolean,
    )

    private fun readBattery(): BatteryRead {
        val status = batteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_STATUS)
        val charging = status == BatteryManager.BATTERY_STATUS_CHARGING
        val full = status == BatteryManager.BATTERY_STATUS_FULL

        var level = batteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY)
        if (level !in 0..100) {
            level = stickyBatteryLevel()
        }



        val rawUa = batteryManager.getIntProperty(BatteryManager.BATTERY_PROPERTY_CURRENT_NOW)
        val currentKnown = rawUa != 0 && rawUa != Int.MIN_VALUE

        var currentMa = 0f
        var drainPerHour = -1f

        if (currentKnown) {
            val magnitudeMa = abs(rawUa) / 1000f
            currentMa = if (charging || full) magnitudeMa else -magnitudeMa


            smoothedCurrentMa = if (smoothedCurrentMa.isNaN()) {
                currentMa
            } else {
                smoothedCurrentMa + SMOOTH_ALPHA * (currentMa - smoothedCurrentMa)
            }
            currentMa = smoothedCurrentMa


            val chargeUah =
                batteryManager.getLongProperty(BatteryManager.BATTERY_PROPERTY_CHARGE_COUNTER)
            if (!charging && !full && chargeUah > 0 && smoothedCurrentMa < 0f) {
                drainPerHour = abs(smoothedCurrentMa) * 1000f / chargeUah * 100f
            }
        }

        return BatteryRead(
            levelPercent = level,
            currentMa = currentMa,
            drainPerHour = drainPerHour,
            charging = charging,
            full = full,
        )
    }



    private fun stickyBatteryLevel(): Int = try {
        val intent = appContext.registerReceiver(
            null,
            IntentFilter(Intent.ACTION_BATTERY_CHANGED)
        )
        val level = intent?.getIntExtra(BatteryManager.EXTRA_LEVEL, -1) ?: -1
        val scale = intent?.getIntExtra(BatteryManager.EXTRA_SCALE, -1) ?: -1
        if (level >= 0 && scale > 0) level * 100 / scale else -1
    } catch (e: Exception) {
        -1
    }

    private companion object {


        const val SMOOTH_ALPHA = 0.35f
    }
}
