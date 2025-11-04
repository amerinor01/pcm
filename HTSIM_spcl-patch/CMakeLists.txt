cmake_minimum_required(VERSION 3.16)
include(FetchContent)

project(htsim_project LANGUAGES CXX)
option(ENABLE_TESTS "enable the test suite" OFF)

# Set C++ standard and compiler flags
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Ensures GNU extensions are disabled
set(CMAKE_CXX_FLAGS_RELEASE -O3) # Prevent -DNDEBUG, htsim requires 
                                 # asserts to be enabled.
# Set compiler flags
add_compile_options(
    -Wall
    -g
    -Wsign-compare
    -Wuninitialized
    -fPIC
    -Wno-deprecated
    -O3
)
# Optionally enable sanitizers
# -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined"

if ( ENABLE_TESTS ) 
    # Fetch GoogleTest
    FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    )
    FetchContent_MakeAvailable(googletest)
    include(GoogleTest)
    enable_testing()
endif()

# path should not be hardcoded, make it configurable
# Set PCM directory
set(PCM_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../PCM/src)
set(PCM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../PCM/include)
set(PCM_ALGO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../PCM/algorithms)
set(PCM_APPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../PCM/apps/htsim_atlahs/)
set(PCM_MISC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../PCM/misc)

# Function to add a symlink creation step for an executable. 
# Used to create a symlink in the source directory to the executable 
# in the build directory for easier access and backward compatibility.
function(add_symlink_target EXECUTABLE TARGET_DIR)
    add_custom_command(
        TARGET ${EXECUTABLE}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove ${TARGET_DIR}/${EXECUTABLE} # Remove existing symlink
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            $<TARGET_FILE:${EXECUTABLE}>  # Path to the executable
            ${TARGET_DIR}/${EXECUTABLE}  # Path for the symlink
        COMMENT "Removing and recreating symlink for ${EXECUTABLE} in ${TARGET_DIR}"
    )
endfunction()

# Define source files
set(SOURCE_FILES
    aeolusqueue.cpp
    callback_pipe.cpp
    cbr.cpp
    cbrpacket.cpp
    clock.cpp
    cnppacket.cpp
    compositeprioqueue.cpp
    compositequeue.cpp
    config.cpp
    cpqueue.cpp
    dcqcn.cpp
    dcqcn_logger.cpp
    dctcp.cpp
    dctcp_transfer.cpp
    ecnprioqueue.cpp
    ecnqueue.cpp
    eqds.cpp
    eqds_logger.cpp
    eqdspacket.cpp
    eth_pause_packet.cpp
    eventlist.cpp
    exoqueue.cpp
    fairpullqueue.cpp
    hpcc.cpp
    hpccpacket.cpp
    logfile.cpp
    loggers.cpp
    meter.cpp
    mtcp.cpp
    ndp.cpp
    ndp_transfer.cpp
    ndppacket.cpp
    ndptunnel.cpp
    ndptunnelpacket.cpp
    network.cpp
    oversubscribed_cc.cpp
    pciemodel.cpp
    pipe.cpp
    priopullqueue.cpp
    prioqueue.cpp
    qcn.cpp
    queue.cpp
    queue_lossless.cpp
    queue_lossless_input.cpp
    queue_lossless_output.cpp
    randomqueue.cpp
    rng.cpp
    roce.cpp
    rocepacket.cpp
    buffer_reps.cpp
    route.cpp
    routetable.cpp
    sent_packets.cpp
    strack.cpp
    strackpacket.cpp
    swift.cpp
    swift_scheduler.cpp
    swiftpacket.cpp
    switch.cpp
    tcp.cpp
    tcp_periodic.cpp
    tcp_transfer.cpp
    tcppacket.cpp
    trigger.cpp
    uec.cpp
    uec_logger.cpp
    uecpacket.cpp
    uec_mp.cpp
    uec_pdcses.cpp
    logsim-interface.cpp
    compute_event.cpp
    null_event.cpp
    atlahs_htsim_api.cpp
)

# Create shared library
add_library(htsim SHARED ${SOURCE_FILES})
target_include_directories(htsim PUBLIC 
                            ${CMAKE_CURRENT_SOURCE_DIR}
                            ${PCM_SOURCE_DIR}
                            ${PCM_INCLUDE_DIR}
                            ${PCM_ALGO_DIR}
                            ${PCM_APPS_DIR}
                            ${PCM_MISC_DIR}) # Needed for header-only libraries (e.g., loggertypes.h).

# Link required system libraries
#target_link_libraries(htsim PUBLIC stdc++ m)

# Set library version and soname for shared library
set_target_properties(htsim PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)

# Enable position independent code for shared libraries
set_property(TARGET htsim PROPERTY POSITION_INDEPENDENT_CODE ON)

# # Create static library
# add_library(htsim STATIC ${SOURCE_FILES})
# target_include_directories(htsim PUBLIC 
#                             ${CMAKE_CURRENT_SOURCE_DIR}
#                             ${PCM_SOURCE_DIR}
#                             ${PCM_INCLUDE_DIR}
#                             ${PCM_ALGO_DIR}
#                             ${PCM_APPS_DIR}
#                             ${PCM_MISC_DIR}) # Needed for header-only libraries (e.g., loggertypes.h).

# # Link required system libraries
# # target_link_libraries(htsim PUBLIC stdc++ m)

# # Set library version and soname for shared library
# set_target_properties(htsim PROPERTIES
#     VERSION 1.0.0
#     SOVERSION 1
# )

# # Enable position independent code for shared libraries
# set_property(TARGET htsim PROPERTY POSITION_INDEPENDENT_CODE ON)

# find_package(Boost REQUIRED COMPONENTS stacktrace_basic)
# target_link_libraries(htsim PRIVATE Boost::stacktrace_basic)
# target_compile_definitions(htsim PRIVATE BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)

# JsonCpp support
find_package(JsonCpp CONFIG REQUIRED)
target_link_libraries(htsim PRIVATE JsonCpp::JsonCpp)

# Add subdirectories
add_subdirectory(datacenter)

# Link datacenter sublibrary into main htsim library
target_link_libraries(htsim PRIVATE htsim_dc)

# Define executables and link with htsim library
add_executable(parse_output parse_output.cpp)
target_link_libraries(parse_output PRIVATE htsim)

if ( ENABLE_TESTS )
    # Unit tests.
    set(UNIT_TEST_FILES
        pipe_test
    )

    foreach(UNIT_TEST_FILE ${UNIT_TEST_FILES}) 
        add_executable(${UNIT_TEST_FILE} ${UNIT_TEST_FILE}.cpp)
        target_link_libraries(${UNIT_TEST_FILE} PRIVATE htsim gtest_main gmock_main)
        gtest_discover_tests(${UNIT_TEST_FILE}
            PROPERTIES LABELS "unit"
            PROPERTIES TIMEOUT 60
        )
    endforeach()
endif()

# Always print ENABLE_TESTS status, so folks don't miss it.
add_custom_target(
  always_run ALL
  COMMAND ${CMAKE_COMMAND} -E echo "ENABLE_TESTS=${ENABLE_TESTS}"
)