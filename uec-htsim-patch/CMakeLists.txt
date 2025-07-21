# List of common source files shared among executables.
set(LIB_SOURCES
    firstfit.cpp
    vl2_topology.cpp
    fat_tree_topology.cpp
    bcube_topology.cpp
    connection_matrix.cpp
    oversubscribed_fat_tree_topology.cpp
    shortflows.cpp
    multihomed_fat_tree_topology.cpp
    star_topology.cpp
    fat_tree_switch.cpp
    dragon_fly_topology.cpp
    generic_topology.cpp
)

# Create shared library
add_library(htsim_dc SHARED ${LIB_SOURCES})
target_include_directories(htsim_dc PUBLIC ${PROJECT_SOURCE_DIR}) 
target_include_directories(htsim_dc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Set library version and soname for shared library
set_target_properties(htsim_dc PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)

# Enable position independent code for shared libraries
set_property(TARGET htsim_dc PROPERTY POSITION_INDEPENDENT_CODE ON)

# Link htsim_dc with the main htsim library
target_link_libraries(htsim_dc PUBLIC htsim) 

# -------------------------------
# Executable Definitions
# -------------------------------
# List of Executables and Their Main Source Files.
add_executable(htsim_tcp main_tcp.cpp)
set_property(TARGET htsim_tcp PROPERTY EXCLUDE_FROM_ALL TRUE)

add_executable(htsim_ndp main_ndp.cpp)
set_property(TARGET htsim_ndp PROPERTY EXCLUDE_FROM_ALL TRUE)

add_executable(htsim_roce main_roce.cpp)
set_property(TARGET htsim_roce PROPERTY EXCLUDE_FROM_ALL TRUE)

add_executable(htsim_swift main_swift.cpp)
set_property(TARGET htsim_swift PROPERTY EXCLUDE_FROM_ALL TRUE)

add_executable(htsim_hpcc main_hpcc.cpp)
set_property(TARGET htsim_hpcc PROPERTY EXCLUDE_FROM_ALL TRUE)

add_executable(htsim_eqds main_eqds.cpp)
set_property(TARGET htsim_eqds PROPERTY EXCLUDE_FROM_ALL TRUE)

add_executable(htsim_uec main_uec.cpp)

# -------------------------------
# Linking Executables with Libraries
# -------------------------------
set (ALL_EXECUTABLES
    htsim_tcp
    htsim_ndp
    htsim_roce
    htsim_swift
    htsim_hpcc
    htsim_eqds
    htsim_uec
)
foreach(EXECUTABLE ${ALL_EXECUTABLES})
    # Link each executable with the 'htsim' library defined in the parent directory.
    target_link_libraries(${EXECUTABLE} PRIVATE htsim_dc)
    target_link_libraries(${EXECUTABLE} PRIVATE htsim)
    # Add a post-build command to explicitly remove and recreate the symlink for all previous executables.
    add_symlink_target(${EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

# Installation configuration
include(GNUInstallDirs)

# Install the shared library
install(TARGETS htsim_dc
    EXPORT htsim-dc-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/htsim/datacenter
    FILES_MATCHING PATTERN "*.h"
)

