# SPDX-License-Identifier: MIT
# Zephyr Edge AI Demo - CMakeLists.txt
#
# This is a Zephyr application that demonstrates Edge AI inference
# using TensorFlow Lite Micro on ARM Cortex-M processors.

cmake_minimum_required(VERSION 3.20.0)

# Find Zephyr package - this must come before project()
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zephyr_edge_ai_demo
    VERSION 1.0.0
    DESCRIPTION "Edge AI Gesture Recognition on Zephyr RTOS"
    LANGUAGES C CXX
)

# ------------------------------------------------------------------------------
# Source Files
# ------------------------------------------------------------------------------

# Main application
target_sources(app PRIVATE
    src/main.c
)

# Sensor Hardware Abstraction Layer
target_sources(app PRIVATE
    src/sensor/sensor_hal.c
    src/sensor/mock_accel.c
)

# Machine Learning / Inference Engine
target_sources(app PRIVATE
    src/ml/inference.cpp
    src/ml/preprocessing.c
    src/ml/gesture_model.c
)

# UART Output Protocol
target_sources(app PRIVATE
    src/output/uart_protocol.c
    src/output/ring_buffer.c
)

# Debug Infrastructure
target_sources(app PRIVATE
    src/debug/debug_monitor.c
    src/debug/timing.c
)

# ------------------------------------------------------------------------------
# Include Directories
# ------------------------------------------------------------------------------

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sensor
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ml
    ${CMAKE_CURRENT_SOURCE_DIR}/src/output
    ${CMAKE_CURRENT_SOURCE_DIR}/src/debug
)

# ------------------------------------------------------------------------------
# Compiler Flags
# ------------------------------------------------------------------------------

# Enable some useful warnings
target_compile_options(app PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Optimize for size in release builds
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(app PRIVATE -Os)
endif()

# ------------------------------------------------------------------------------
# Print Build Info
# ------------------------------------------------------------------------------

message(STATUS "")
message(STATUS "=== Zephyr Edge AI Demo Build Configuration ===")
message(STATUS "  Board:       ${BOARD}")
message(STATUS "  Version:     ${PROJECT_VERSION}")
message(STATUS "  Build Type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "================================================")
message(STATUS "")
