cmake_minimum_required(VERSION 3.13)
project(LLVMCustomPasses LANGUAGES C CXX)

# Find LLVM package
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# Set LLVM include paths and flags
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Map necessary LLVM components
llvm_map_components_to_libnames(LLVM_LIBS support core irreader passes)

# List of pass source files (add more here)
set(PASS_SOURCES
    ReplaceCallWithLoad.cpp
    EnumeratePaths.cpp
)

# Function to create a plugin target
function(add_llvm_pass_plugin name source)
    add_library(${name} MODULE ${source})
    target_link_libraries(${name} PRIVATE LLVM)
    set_target_properties(${name} PROPERTIES
        COMPILE_FLAGS "-fno-rtti"
        PREFIX ""
        SUFFIX ".so"
    )
endfunction()

# Loop through sources and build a plugin for each
foreach(source_file ${PASS_SOURCES})
    get_filename_component(pass_name ${source_file} NAME_WE)
    add_llvm_pass_plugin(${pass_name} ${source_file})
endforeach()
