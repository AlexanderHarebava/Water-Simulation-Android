

<img width="100" alt="Component 2 (1)" src="https://github.com/user-attachments/assets/6d361e62-78b2-4ab1-abd0-d0bc0018a466" />

# Water Wallpaper Box - 3D Water Simulation & Live Wallpaper for Android

Real-time 3D fluid simulation Android application and Live Wallpaper built with Kotlin, Jetpack Compose and C++, **OpenGL ES 3.1** and **GLES Compute Shaders**.

---
# Results




https://github.com/user-attachments/assets/923bbb12-73bd-4b2a-98e3-7e0d1fae6440




https://github.com/user-attachments/assets/5416bfc5-a07b-42d9-9719-7f92df8a5321






https://github.com/user-attachments/assets/467192e6-8c73-4c72-8320-ab238d258581





https://github.com/user-attachments/assets/9ac75161-2e21-4932-a557-0cbf9a066a46













https://github.com/user-attachments/assets/19653bfd-6058-4293-87f1-d32686cca72a




## New Mode

https://github.com/user-attachments/assets/1e01f116-96f9-4674-8962-c14b87864d0b




## Project Structure

```
waterc/
└── app/
    └── src/
        └── main/
            ├── cpp/                                # C++ Native Fluid Engine & Renderer
            │   ├── rendering/                      # OpenGL ES Raymarching & Shaders
            │   │   ├── GLRenderer.cpp              # 3D Ray-marching volume & MPM sphere instancing renderer
            │   │   ├── GLRenderer.h
            │   │   ├── Shader.cpp                  # Shader compilation, linking, and uniform management
            │   │   └── Shader.h
            │   ├── simulation/                     # 3D Fluid Dynamics Simulation Engine
            │   │   ├── Advect.cpp                  # Semi-Lagrangian advection implementations (2D & 3D)
            │   │   ├── Advect.h
            │   │   ├── config.cpp                  # Global fluid simulation parameters and configuration
            │   │   ├── config.h
            │   │   ├── ConjugateGradient.cpp       # CG / PCG solvers and incomplete Cholesky preconditioning
            │   │   ├── ConjugateGradient.h
            │   │   ├── Fluids.cpp                  # CPU-based 3D Level-Set fluid simulation engine
            │   │   ├── Fluids.h
            │   │   ├── FluidsGPU.cpp               # GLES 3.1 Compute Shader Eulerian fluid backend
            │   │   ├── FluidsGPU.h
            │   │   ├── FluidsMPMGPU.cpp            # GLES 3.1 Compute Shader MPM (MLS-MPM) particle backend
            │   │   ├── FluidsMPMGPU.h
            │   │   ├── p2gDensity.comp             # Compute shader for particle-to-grid density mapping
            │   │   ├── Project.cpp                 # Divergence-free velocity field pressure projection
            │   │   ├── Project.h
            │   │   ├── StaggeredGrid.h             # MAC (Marker-and-Cell) staggered grid data structures
            │   │   └── types.h                     # Data structures, field grids, and cell labels
            │   ├── CMakeLists.txt                  # NDK CMake build configuration and library links
            │   ├── jni_bridge.cpp                  # Native JNI bridge exposing C++ engine functions to Java/Kotlin
            │   └── jni_bridge.h
            └── java/com/example/waterc/            # Kotlin Android Application & UI
                ├── ui/
                │   └── theme/                      # Jetpack Compose UI styling components
                │       ├── Color.kt                # Application color definitions
                │       ├── Theme.kt                # Material Theme provider configuration
                │       └── Type.kt                 # Material typography styles
                ├── FluidGLSurfaceView.kt           # Custom GLSurfaceView component
                ├── FluidSimulation.kt              # JNI / Kotlin interface to NDK engine
                ├── FluidWallpaperService.kt        # Android Live Wallpaper Service setup
                ├── FpsMeter.kt                     # Performance frame tracking
                ├── MainActivity.kt                  # Main application entry point
                ├── PerformanceMonitor.kt            # Metric collection
                ├── PerformanceOverlay.kt           # UI overlay for statistics
                ├── WallpaperEglThread.kt            # EGL context worker thread for wallpaper
                ├── WallpaperMenu.kt                 # Settings overlay UI
                ├── WallpaperSettingsActivity.kt    # Wallpaper configuration screen
                └── WatercSettings.kt                # User preferences state management

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

## License

**Apache License 2.0**.

## Credit

https://github.com/tmarrec/fluid-simulation


