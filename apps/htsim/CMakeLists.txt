# HTSIM Integration CMake Configuration
# This directory contains both the HTSIM plugin and the HTSIM application

# Set HTSIM build directory
if(NOT DEFINED HTSIM_ROOT_DIR)
    set(HTSIM_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third-party/uet-htsim/htsim/sim/")
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
# HTSIM Application (Header-Only Implementation)
# ===============================================

# Check if application source exists
set(HTSIM_APP_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/htsim_app.cpp")
if(EXISTS ${HTSIM_APP_SOURCE})
    # Create HTSIM application
    add_executable(htsim_flow_app ${HTSIM_APP_SOURCE})
    
    # Add HTSIM include directories
    target_include_directories(htsim_flow_app PRIVATE ${HTSIM_INCLUDES})
    
    # Link all required libraries (no plugin needed - header-only)
    target_link_libraries(htsim_flow_app PRIVATE 
        ${HTSIM_LIB}        # HTSIM simulation library
        ${HTSIM_DC_LIB}     # HTSIM datacenter library
        ${CMAKE_DL_LIBS}    # Dynamic loading support (dlopen/dlsym)
    )
    
    # Install the application
    install(TARGETS htsim_flow_app RUNTIME DESTINATION bin)
    
    message(STATUS "HTSIM application configured: ${HTSIM_APP_SOURCE}")
else()
    message(WARNING "HTSIM application source not found: ${HTSIM_APP_SOURCE}")
endif()

# Summary
message(STATUS "HTSIM build config summary:")
message(STATUS "  HTSIM lib: ${HTSIM_LIB}")
message(STATUS "  HTSIM DC lib: ${HTSIM_DC_LIB}")
message(STATUS "  Implementation: Header-only (no plugin library)")
if(EXISTS ${HTSIM_APP_SOURCE})
    message(STATUS "  Application: htsim_flow_app")
endif()
