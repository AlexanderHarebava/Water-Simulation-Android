# Water Wallpaper Box - 3D Water Simulation & Live Wallpaper for Android

Real-time 3D fluid simulation Android application and Live Wallpaper built with Kotlin, Jetpack Compose and C++, **OpenGL ES 3.1** and **GLES Compute Shaders**.

---
# Results

[Screen_recording_20260907_010249.webm](../../Desktop/Screen_recording_20260907_010249.webm)

[Screen_recording_20260907_010420.webm](../../Desktop/Screen_recording_20260907_010420.webm)

[Screen_recording_20260907_010743.webm](../../Desktop/Screen_recording_20260907_010743.webm)

[Screen_recording_20260907_010826.webm](../../Desktop/Screen_recording_20260907_010826.webm)



## Project Structure

```
waterc/
└── app/
    └── src/
        └── main/
            ├── cpp/                          # C++ Native Fluid Engine & Renderer
            │   ├── rendering/                # OpenGL ES Raymarching & Shaders
            │   │   ├── GLRenderer.cpp        # 3D Ray-marching volume renderer engine
            │   │   ├── GLRenderer.h
            │   │   ├── Shader.cpp            # Shader compilation and management
            │   │   └── Shader.h
            │   └── simulation/               # 3D Fluid Dynamics Simulation Engine
            │       ├── Advect.cpp            # Semi-Lagrangian advection
            │       ├── Advect.h
            │       ├── config.cpp            # Fluid simulation global configuration
            │       ├── config.h
            │       ├── ConjugateGradient.cpp # CG / PCG solvers for pressure projection
            │       ├── ConjugateGradient.h
            │       ├── Fluids.cpp            # Main CPU fluid simulation pipeline
            │       ├── Fluids.h
            │       ├── FluidsGPU.cpp         # GLES 3.1 Compute Shader fluid backend
            │       ├── FluidsGPU.h
            │       ├── Project.cpp           # Divergence free field projection
            │       ├── Project.h
            │       ├── StaggeredGrid.h       # MAC (Marker-and-Cell) Staggered Grid structure
            │       └── types.h               # Data structures, Field grids & labels
            └── java/com/example/waterc/      # Kotlin Android Application & UI
                ├── FluidGLSurfaceView.kt     # Custom GLSurfaceView component
                ├── FluidSimulation.kt        # JNI / Kotlin interface to NDK engine
                ├── FluidWallpaperService.kt # Android Live Wallpaper Service setup
                ├── FpsMeter.kt               # Performance frame tracking
                ├── MainActivity.kt           # Main application entry point
                ├── PerformanceMonitor.kt     # Metric collection
                ├── PerformanceOverlay.kt     # UI overlay for statistics
                ├── WallpaperEglThread.kt     # EGL context worker thread for wallpaper
                ├── WallpaperMenu.kt          # Settings overlay UI
                ├── WallpaperSettingsActivity.kt # Wallpaper configuration screen
                └── WatercSettings.kt         # User preferences state management
```

---

## Architecture & Tech Stack

### **Android & UI Layer**
- **Language**: Kotlin
- **Graphics API**: OpenGL ES 3.0 / 3.1
- **Architecture**: EGL Render Threading, Android Wallpaper Engine integration (`WallpaperService`)

### **Native Engine (NDK / C++)**
- **Language**: C++17
- **Linear Algebra**: [Eigen 3](https://libeigen.gitlab.io/) for sparse matrix solving in CG/PCG methods.
- **GLES Compute Shaders**
- **Rendering**: Volumetric Ray Marching shader (`GLRenderer.cpp`) rendering a 3D scalar density texture with physically-based optical models.
---

## Also you need download Eigen and put here app/src/main/cpp

## 📜 License

**Apache License 2.0**.

## Credit

https://github.com/tmarrec/fluid-simulation
