cmake_minimum_required(VERSION 3.13)
project(PathEnumeratorPass LANGUAGES C CXX)

# Set C++ standard and include LLVM headers
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Create the LLVM pass plugin
add_library(enumerate_paths MODULE enumerate_paths.cpp)

set_target_properties(enumerate_paths PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
    PREFIX ""
    SUFFIX ".so"
)