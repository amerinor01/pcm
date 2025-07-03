cmake_minimum_required(VERSION 3.13)
project(ReplaceCallWithLoadPass LANGUAGES C CXX)

# Find LLVM package
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set include paths and flags
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# Build shared library
add_library(ReplaceCallWithLoad MODULE ReplaceCallWithLoad.cpp)

# LLVM target
llvm_map_components_to_libnames(llvm_libs support core irreader passes)

target_link_libraries(ReplaceCallWithLoad PRIVATE ${llvm_libs})

# Optional: give the .so a clean name
set_target_properties(ReplaceCallWithLoad PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
    PREFIX ""  # Don't prepend "lib"
    SUFFIX ".so"  # Make it a .so plugin
)

