package com.example.waterc


class FpsMeter(private val windowMs: Long = 1_000L) {

    data class Snapshot(


        val fps: Float,


        val avgMs: Float,


        val minMs: Float,


        val maxMs: Float,
    )

    private val windowNs = windowMs * 1_000_000L



    private val frames = ArrayDeque<Long>()



    @Synchronized
    fun onFrame(nanos: Long = System.nanoTime()) {
        frames.addLast(nanos)
        trim(nowNanos = nanos)
    }



    @Synchronized
    fun snapshot(nowNanos: Long = System.nanoTime()): Snapshot {
        trim(nowNanos)

        val list = frames.toList()
        if (list.size < 2) {

            return Snapshot(list.size * 1000f / windowMs, 0f, 0f, 0f)
        }

        var sum = 0.0
        var min = Double.MAX_VALUE
        var max = -Double.MAX_VALUE

        for (i in 1 until list.size) {
            val deltaMs = (list[i] - list[i - 1]) / 1_000_000.0
            sum += deltaMs
            if (deltaMs < min) min = deltaMs
            if (deltaMs > max) max = deltaMs
        }

        val count = list.size - 1

        return Snapshot(
            fps = list.size * 1000f / windowMs,
            avgMs = (sum / count).toFloat(),
            minMs = min.toFloat(),
            maxMs = max.toFloat(),
        )
    }



    private fun trim(nowNanos: Long) {
        while (frames.isNotEmpty() && nowNanos - frames.first() > windowNs) {
            frames.removeFirst()
        }
    }
}
