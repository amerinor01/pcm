# HTSIM Integration CMake Configuration
# This directory contains both the HTSIM plugin and the HTSIM application

# Set HTSIM build directory
if(NOT HTSIM_ROOT_DIR)
    set(HTSIM_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../uec-transport-simulation-code-main/htsim/sim/")
endif()

# Validate HTSIM directory exists
if(NOT EXISTS "${HTSIM_ROOT_DIR}")
    message(FATAL_ERROR "HTSIM build directory not found: ${HTSIM_ROOT_DIR}")
endif()

# Find HTSIM libraries (shared by both plugin and app)
find_library(HTSIM_LIB 
    NAMES htsim
    PATHS ${HTSIM_ROOT_DIR}/build
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(HTSIM_DC_LIB
    NAMES htsim_dc
    PATHS ${HTSIM_ROOT_DIR}/build/datacenter
    NO_DEFAULT_PATH
    REQUIRED
)

# Common HTSIM include directories
set(HTSIM_INCLUDES
    ${PCM_SOURCE_DIR}
    ${HTSIM_ROOT_DIR}
    ${HTSIM_ROOT_DIR}/datacenter
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ===============================================
# HTSIM Plugin Library
# ===============================================

# Find HTSIM plugin sources (both C and CPP, exclude the main app)
file(GLOB HTSIM_PLUGIN_CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB HTSIM_PLUGIN_C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
list(REMOVE_ITEM HTSIM_PLUGIN_CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/htsim_app.cpp") # exclude htsim application
set(HTSIM_PLUGIN_SOURCES ${HTSIM_PLUGIN_CPP_SOURCES} ${HTSIM_PLUGIN_C_SOURCES})

if(HTSIM_PLUGIN_SOURCES)
    # Create HTSIM plugin library
    add_library(pcm_htsim_plugin SHARED ${HTSIM_PLUGIN_SOURCES})
    
    # Target-specific compile definitions
    target_compile_definitions(pcm_htsim_plugin PRIVATE BUILD_HTSIM_PLUGIN)
    
    # Target-specific include directories
    target_include_directories(pcm_htsim_plugin PRIVATE ${HTSIM_INCLUDES})
    
    # Link libraries
    target_link_libraries(pcm_htsim_plugin PRIVATE 
        pcm                  # Link to main PCM library for symbols
        ${HTSIM_LIB}
        ${HTSIM_DC_LIB}
    )
    
    # Set RPATH for HTSIM libraries
    set_target_properties(pcm_htsim_plugin PROPERTIES
        INSTALL_RPATH "${HTSIM_ROOT_DIR}/build;${HTSIM_ROOT_DIR}/build/datacenter"
        BUILD_WITH_INSTALL_RPATH TRUE
        OUTPUT_NAME pcm_htsim
    )
    
    # Install the plugin
    install(TARGETS pcm_htsim_plugin 
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
    
    message(STATUS "HTSIM plugin configured:")
    message(STATUS "  HTSIM dir: ${HTSIM_ROOT_DIR}")
    message(STATUS "  Plugin sources: ${HTSIM_PLUGIN_SOURCES}")
else()
    message(WARNING "No HTSIM plugin sources found in ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# ===============================================
# HTSIM Application
# ===============================================

# Check if application source exists
set(HTSIM_APP_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/htsim_app.cpp")
if(EXISTS ${HTSIM_APP_SOURCE})
    # Create HTSIM application
    add_executable(htsim_flow_app ${HTSIM_APP_SOURCE})
    
    # Add HTSIM include directories
    target_include_directories(htsim_flow_app PRIVATE ${HTSIM_INCLUDES})
    
    # Link all required libraries
    target_link_libraries(htsim_flow_app PRIVATE 
        pcm 
        pcm_htsim_plugin
        ${HTSIM_LIB}
        ${HTSIM_DC_LIB}
    )
    
    # Install the application
    install(TARGETS htsim_flow_app RUNTIME DESTINATION bin)
    
    message(STATUS "HTSIM application configured: ${HTSIM_APP_SOURCE}")
else()
    message(WARNING "HTSIM application source not found: ${HTSIM_APP_SOURCE}")
endif()

# Summary
message(STATUS "HTSIM integration summary:")
message(STATUS "  HTSIM lib: ${HTSIM_LIB}")
message(STATUS "  HTSIM DC lib: ${HTSIM_DC_LIB}")
if(HTSIM_PLUGIN_SOURCES)
    message(STATUS "  Plugin: libpcm_htsim.so")
endif()
if(EXISTS ${HTSIM_APP_SOURCE})
    message(STATUS "  Application: htsim_flow_app")
endif()
