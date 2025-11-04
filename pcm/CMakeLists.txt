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
option(BUILD_HAC_IR "Build HAC LLVM IR generation" OFF)
option(ENABLE_VM_PROFILING "Enable Linux perf-based profiling" OFF)
option(ATOMIC_PCM_VM_ALIGNED_STORAGE "Enable aligned atomic storage" OFF)
option(LOG_SNAPSHOT_SNS "Enable snapshot sequence number logging" OFF)

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

if(ATOMIC_PCM_VM_ALIGNED_STORAGE)
    add_compile_definitions(ATOMIC_PCM_VM_ALIGNED_STORAGE)
endif()

if(LOG_SNAPSHOT_SNS)
    add_compile_definitions(LOG_SNAPSHOT_SNS)
endif()

# Optional profiling
if(ENABLE_VM_PROFILING)
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
    add_compile_definitions(ENABLE_VM_PROFILING)

    if(NOT DEFINED VM_PROF_BACKEND)
        set(VM_PROF_BACKEND "rdtsc")
    endif()

    if(VM_PROF_BACKEND STREQUAL "perf")
        add_compile_definitions(VM_PROF_BACKEND_PERF)
    elseif(VM_PROF_BACKEND STREQUAL "chrono")
        add_compile_definitions(VM_PROF_BACKEND_CHRONO)
    elseif(VM_PROF_BACKEND STREQUAL "rdtsc")
        # Default
    else()
        message(FATAL_ERROR "Unknown profiling backend: ${VM_PROF_BACKEND}")
    endif()
endif()

# Directory structure
set(PCM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(PCM_ALGO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/algorithms)
set(PCM_APPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/apps)

# Include directories
include_directories(${PCM_INCLUDE_DIR})
include_directories(${PCM_ALGO_DIR})
include_directories(${CMAKE_BINARY_DIR}/algorithms)  # For YAML-generated algorithm headers

# If xxHash is present as a git submodule at repo root (../xxHash), expose its
# headers and any bundled libraries so project code can use <xxhash.h>.
set(XXHASH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../xxHash)
if(EXISTS ${XXHASH_DIR}/xxhash.h)
    message(STATUS "xxHash detected at ${XXHASH_DIR}: adding include dir")
    include_directories(${XXHASH_DIR})

    # Create IMPORTED targets if prebuilt libraries are available in the submodule
    if(EXISTS ${XXHASH_DIR}/libxxhash.a)
        add_library(xxhash_static STATIC IMPORTED)
        set_target_properties(xxhash_static PROPERTIES
            IMPORTED_LOCATION ${XXHASH_DIR}/libxxhash.a
        )
        target_include_directories(xxhash_static INTERFACE ${XXHASH_DIR})
    endif()

    if(EXISTS ${XXHASH_DIR}/libxxhash.so)
        add_library(xxhash_shared SHARED IMPORTED)
        set_target_properties(xxhash_shared PROPERTIES
            IMPORTED_LOCATION ${XXHASH_DIR}/libxxhash.so
        )
        target_include_directories(xxhash_shared INTERFACE ${XXHASH_DIR})
    endif()
endif()

# ===============================================
# HAC IR Generation
# ===============================================

if(BUILD_HAC_IR)
    add_subdirectory(hac)
endif()

# ===============================================
# Algorithm Plugins
# ===============================================

# Build algorithm plugins in their own subdirectory
add_subdirectory(algorithms)

# ===============================================
# HTSIM
# ===============================================

if(BUILD_HTSIM_PLUGIN)
    add_subdirectory(apps/htsim)
endif()

# ===============================================
# HTSIM V2 Application
# ===============================================

# HTSIM V2 is always built if dependencies are available
if(BUILD_HTSIM_PLUGIN)
    add_subdirectory(apps/htsim_v2)
endif()

# ===============================================
# HTSIM ATLAHS Application
# ===============================================

if(BUILD_HTSIM_PLUGIN)
    add_subdirectory(apps/htsim_atlahs)
endif()

# ===============================================
# Summary
# ===============================================

message(STATUS "PCM Build Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  HTSIM v1 plugin: ${BUILD_HTSIM_PLUGIN}")
message(STATUS "  HTSIM v2: ${BUILD_HTSIM_PLUGIN}")
message(STATUS "  HTSIM ATLAHS: ${BUILD_HTSIM_PLUGIN}")
message(STATUS "  Profiling: ${ENABLE_VM_PROFILING}")
if(ENABLE_VM_PROFILING)
    message(STATUS "  Profiling Backend: ${VM_PROF_BACKEND}")
endif()
message(STATUS "  IR generator: ${BUILD_HAC_IR}")
