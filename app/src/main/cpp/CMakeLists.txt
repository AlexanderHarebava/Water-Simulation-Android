
cmake_minimum_required(VERSION 3.22.1)
project("waterc-fluid" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/eigen/Eigen)

add_library(waterc-fluid SHARED
        jni_bridge.cpp
        simulation/Fluids.cpp
        simulation/FluidsGPU.cpp
        simulation/Advect.cpp
        simulation/FluidsMPMGPU.cpp
        simulation/Project.cpp
        simulation/ConjugateGradient.cpp
        simulation/config.cpp
        rendering/GLRenderer.cpp
        rendering/Shader.cpp
)

target_include_directories(waterc-fluid PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/simulation
        ${CMAKE_CURRENT_SOURCE_DIR}/rendering
)

target_link_libraries(waterc-fluid
        android
        log
        EGL
        GLESv3
        OpenSLES
        jnigraphics
)
