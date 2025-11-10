# HTSIM Integration CMake Configuration
# This directory contains both the HTSIM plugin and the HTSIM application
include(FetchContent)
# Set HTSIM build directory
if(NOT SPCL_HTSIM_ROOT_DIR)
    set(SPCL_HTSIM_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../HTSIM_spcl/htsim/sim/")
endif()

# Validate HTSIM directory exists
if(NOT EXISTS "${SPCL_HTSIM_ROOT_DIR}")
    message(FATAL_ERROR "HTSIM build directory not found: ${SPCL_HTSIM_ROOT_DIR}")
endif()

# Find HTSIM libraries (shared by both plugin and app)
find_library(SPCL_HTSIM_LIB
    NAMES htsim
    PATHS ${SPCL_HTSIM_ROOT_DIR}/build
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(SPCL_HTSIM_DC_LIB
    NAMES htsim_dc
    PATHS ${SPCL_HTSIM_ROOT_DIR}/build/datacenter
    NO_DEFAULT_PATH
    REQUIRED
)

# Common HTSIM include directories
set(HTSIM_INCLUDES
    ${PCM_SOURCE_DIR}
    ${SPCL_HTSIM_ROOT_DIR}
    ${SPCL_HTSIM_ROOT_DIR}/datacenter
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ===============================================
# HTSIM ATLAHS Application (Header-Only Implementation)
# ===============================================

# Check if ATLAHS application source exists
set(HTSIM_APP_ATLAHS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/htsim_app_atlahs.cpp")
if(EXISTS ${HTSIM_APP_ATLAHS_SOURCE})
    # Create HTSIM ATLAHS application
    add_executable(htsim_flow_app_atlahs ${HTSIM_APP_ATLAHS_SOURCE})

    # JsonCpp support
    set(JSONCPP_WITH_CMAKE_PACKAGE ON)
    set(JSONCPP_CXX_STANDARD 23)

    FetchContent_Declare(
    jsoncpp
    GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
    GIT_TAG        1.9.6
    )

    FetchContent_MakeAvailable(jsoncpp)
    # find_package(JsonCpp CONFIG REQUIRED)
    target_link_libraries(htsim_flow_app_atlahs PRIVATE jsoncpp_lib)
    
    # Add HTSIM include directories
    target_include_directories(htsim_flow_app_atlahs PRIVATE ${HTSIM_INCLUDES})
    
    # Link all required libraries (no plugin needed - header-only)
    target_link_libraries(htsim_flow_app_atlahs PRIVATE 
        pcm                  # Main PCM library
        ${SPCL_HTSIM_LIB}        # HTSIM simulation library
        ${SPCL_HTSIM_DC_LIB}     # HTSIM datacenter library
        ${CMAKE_DL_LIBS}    # Dynamic loading support (dlopen/dlsym)
    )
    
    # Install the atlahs application
    install(TARGETS htsim_flow_app_atlahs RUNTIME DESTINATION bin)
    
    message(STATUS "HTSIM ATLAHS application configured: ${HTSIM_APP_ATLAHS_SOURCE}")
else()
    message(WARNING "HTSIM ATLAHS application source not found: ${HTSIM_APP_ATLAHS_SOURCE}")
endif()

# Summary
message(STATUS "HTSIM ATLAHS integration summary:")
message(STATUS "  HTSIM lib: ${SPCL_HTSIM_LIB}")
message(STATUS "  HTSIM DC lib: ${SPCL_HTSIM_DC_LIB}")
message(STATUS "  Implementation: Header-only (no plugin library)")
if(EXISTS ${HTSIM_APP_ATLAHS_SOURCE})
    message(STATUS "  Application: htsim_flow_app_atlahs")
endif()
