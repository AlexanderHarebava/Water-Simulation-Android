package com.example.waterc

import android.app.WallpaperManager
import android.content.ActivityNotFoundException
import android.content.ComponentName
import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import com.example.waterc.ui.theme.WatercTheme

class MainActivity : ComponentActivity() {
    private var glView: FluidGLSurfaceView? = null
    private val fpsMeter = FpsMeter()


    private lateinit var currentSettings: WatercSettings

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)


        currentSettings = WatercSettings.load(this)

        setContent {
            WatercTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    Box(modifier = Modifier.fillMaxSize()) {
                        AndroidView(
                            factory = { context ->
                                FluidGLSurfaceView(context, currentSettings).also { view ->
                                    view.fpsMeter = fpsMeter
                                    glView = view
                                }
                            },
                            modifier = Modifier.fillMaxSize()
                        )

                        PerformanceOverlay(
                            fpsMeter = fpsMeter,
                            modifier = Modifier.align(Alignment.TopStart)
                        )

                        WallpaperMenu(
                            initialGridSize = currentSettings.gridSize,
                            onGridSizeSelected = { size ->
                                glView?.setGridSize(size)
                            },
                            onApplyClicked = { size ->
                                applyLiveWallpaper(size)
                            },
                            onOpenSettings = {
                                startActivity(
                                    Intent(
                                        this@MainActivity,
                                        WallpaperSettingsActivity::class.java
                                    )
                                )
                            },
                            modifier = Modifier.align(Alignment.BottomCenter)
                        )
                    }
                }
            }
        }
    }

    private fun applyLiveWallpaper(gridSize: Int) {
        val settings = WatercSettings.load(this).copy(gridSize = gridSize)

        WatercSettings.save(this, settings, synchronous = true)

        val component = ComponentName(this, FluidWallpaperService::class.java)

        try {
            val intent = Intent(WallpaperManager.ACTION_CHANGE_LIVE_WALLPAPER).apply {
                putExtra(
                    WallpaperManager.EXTRA_LIVE_WALLPAPER_COMPONENT,
                    component
                )
            }

            startActivity(intent)
        } catch (e: ActivityNotFoundException) {
            try {
                startActivity(
                    Intent(WallpaperManager.ACTION_LIVE_WALLPAPER_CHOOSER)
                )
            } catch (e2: Exception) {
                Toast.makeText(
                    this,
                    "Не удалось открыть выбор живых обоев",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }
    }

    override fun onResume() {
        super.onResume()


        currentSettings = WatercSettings.load(this)
        glView?.applySettings(currentSettings)

        glView?.onResume()
        glView?.startSensors()
    }

    override fun onPause() {
        super.onPause()

        glView?.onPause()
        glView?.stopSensors()
    }

    override fun onDestroy() {
        super.onDestroy()

        glView?.cleanup()
    }
}
