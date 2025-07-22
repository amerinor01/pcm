cmake_minimum_required(VERSION 3.16)
project(PCM VERSION 1.0.0 LANGUAGES C CXX)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR 
   CMAKE_C_COMPILER_ID STREQUAL "AppleClang" OR 
   CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_C_COMPILER_ID}. Only GCC, Clang, and AppleClang are supported.")
endif()

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Configurable build options
option(BUILD_HTSIM_PLUGIN "Build HTSIM plugin" OFF)
option(ENABLE_PROFILING "Enable Linux perf-based profiling" OFF)
option(BUILD_HAC_PASS "Build HAC optimization pass" OFF)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Set build output directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optional profiling
if(ENABLE_PROFILING)
    # Check if we're on Linux
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "Profiling is only supported on Linux systems. Current system: ${CMAKE_SYSTEM_NAME}")
    endif()
    
    # Check for perf headers
    find_path(PERF_INCLUDE_DIR 
        NAMES linux/perf_event.h
        DOC "Path to perf event headers"
    )
    
    if(NOT PERF_INCLUDE_DIR)
        message(FATAL_ERROR "Profiling enabled but linux/perf_event.h not found.")
    endif()
    
    message(STATUS "Profiling enabled: using perf headers from ${PERF_INCLUDE_DIR}")
    add_compile_definitions(ENABLE_PROFILING)
endif()

# Directory structure
set(PCM_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(PCM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(PCM_ALGO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/algorithms)
set(PCM_APPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/apps)

# Include directories
include_directories(${PCM_INCLUDE_DIR})
include_directories(${PCM_ALGO_DIR})
include_directories(${CMAKE_BINARY_DIR}/algorithms)  # For YAML-generated algorithm headers

# ===============================================
# Core PCM Library
# ===============================================

# Build core PCM library in src subdirectory
add_subdirectory(src)

# ===============================================
# Algorithm Plugins
# ===============================================

# Build algorithm plugins in their own subdirectory
add_subdirectory(algorithms)

# ===============================================
# Traffic Generator Application
# ===============================================

# Build traffic generator in its own subdirectory
add_subdirectory(apps/traffic_gen)

# ===============================================
# HTSIM Plugin (Optional)
# ===============================================

if(BUILD_HTSIM_PLUGIN)
    add_subdirectory(apps/htsim)
endif()

# ===============================================
# HAC Pass (Optional)
# ===============================================

if(BUILD_HAC_PASS)
    add_subdirectory(HAC)
endif()

# ===============================================
# Summary
# ===============================================

message(STATUS "PCM Build Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  HTSIM plugin: ${BUILD_HTSIM_PLUGIN}")
message(STATUS "  Profiling: ${ENABLE_PROFILING}")
message(STATUS "  HAC pass: ${BUILD_HAC_PASS}")
if(BUILD_HTSIM_PLUGIN)
    message(STATUS "  HTSIM build dir: ${HTSIM_BUILD_DIR}")
endif()
