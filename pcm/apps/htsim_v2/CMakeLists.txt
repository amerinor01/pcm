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
# HTSIM V2 Application (Header-Only Implementation)
# ===============================================

# Check if v2 application source exists
set(HTSIM_APP_V2_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/htsim_app_v2.cpp")
if(EXISTS ${HTSIM_APP_V2_SOURCE})
    # Create HTSIM v2 application
    add_executable(htsim_flow_app_v2 ${HTSIM_APP_V2_SOURCE})
    
    # Add HTSIM include directories
    target_include_directories(htsim_flow_app_v2 PRIVATE ${HTSIM_INCLUDES})
    
    # Link all required libraries (no plugin needed - header-only)
    target_link_libraries(htsim_flow_app_v2 PRIVATE 
        ${HTSIM_LIB}        # HTSIM simulation library
        ${HTSIM_DC_LIB}     # HTSIM datacenter library
        ${CMAKE_DL_LIBS}    # Dynamic loading support (dlopen/dlsym)
    )
    
    # Install the v2 application
    install(TARGETS htsim_flow_app_v2 RUNTIME DESTINATION bin)
    
    message(STATUS "HTSIM v2 application configured: ${HTSIM_APP_V2_SOURCE}")
else()
    message(WARNING "HTSIM v2 application source not found: ${HTSIM_APP_V2_SOURCE}")
endif()

# Summary
message(STATUS "HTSIM v2 integration summary:")
message(STATUS "  HTSIM lib: ${HTSIM_LIB}")
message(STATUS "  HTSIM DC lib: ${HTSIM_DC_LIB}")
message(STATUS "  Implementation: Header-only (no plugin library)")
if(EXISTS ${HTSIM_APP_V2_SOURCE})
    message(STATUS "  Application: htsim_flow_app_v2")
endif()
