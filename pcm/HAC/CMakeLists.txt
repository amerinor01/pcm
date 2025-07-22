cmake_minimum_required(VERSION 3.13)
project(ReplaceCallWithLoadPass LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# Find LLVM package
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set include paths and flags
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Build shared library
add_library(ReplaceCallWithLoad MODULE ReplaceCallWithLoad.cpp)

# LLVM target
llvm_map_components_to_libnames(llvm_libs support core irreader passes)

target_link_libraries(ReplaceCallWithLoad PRIVATE LLVM)

# Optional: give the .so a clean name
set_target_properties(ReplaceCallWithLoad PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
    PREFIX ""  # Don't prepend "lib"
    SUFFIX ".so"  # Make it a .so plugin
)

# ===============================================
# HAC-Optimized Algorithm Libraries
# ===============================================

# Function to create HAC-optimized algorithm libraries using LLVM
function(create_hac_optimized_algo ALGO_NAME)
    # Get paths from parent variables
    set(PCM_ALGO_DIR "${CMAKE_SOURCE_DIR}/algorithms")
    set(PCM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
    
    set(YAML_FILE "${PCM_ALGO_DIR}/${ALGO_NAME}.yaml")
    set(FABRIC_FILE "${PCM_ALGO_DIR}/fabric.yaml")
    set(SOURCE_FILE "${PCM_ALGO_DIR}/${ALGO_NAME}.c")
    set(LLVM_IR_FILE "${CMAKE_BINARY_DIR}/lib/lib${ALGO_NAME}.ll")
    set(OPTIMIZED_IR_FILE "${CMAKE_BINARY_DIR}/lib/lib${ALGO_NAME}_opt.ll")
    set(OBJECT_FILE "${CMAKE_BINARY_DIR}/lib/lib${ALGO_NAME}.ll.o")
    set(SHARED_LIB "${CMAKE_BINARY_DIR}/lib/lib${ALGO_NAME}.ll.so")
    
    string(TOUPPER ${ALGO_NAME} ALGO_NAME_UPPER)
    
    # Generate LLVM IR from source
    add_custom_command(
        OUTPUT ${LLVM_IR_FILE}
        COMMAND clang -S -emit-llvm -fPIC -O3 -Wall -Wextra
                -I${PCM_INCLUDE_DIR} -I${PCM_ALGO_DIR} -I${CMAKE_BINARY_DIR}/algorithms
                -DHANDLER_BUILD -DBUILD_ALGO_${ALGO_NAME_UPPER}
                -o ${LLVM_IR_FILE} ${SOURCE_FILE}
                $<$<OR:$<STREQUAL:${ALGO_NAME},dctcp>,$<STREQUAL:${ALGO_NAME},newreno>>:${PCM_ALGO_DIR}/tcp_utils.c>
        DEPENDS ${SOURCE_FILE} pcm ${ALGO_NAME}_gen
                $<$<OR:$<STREQUAL:${ALGO_NAME},dctcp>,$<STREQUAL:${ALGO_NAME},newreno>>:${PCM_ALGO_DIR}/tcp_utils.c>
        COMMENT "Generating LLVM IR for ${ALGO_NAME}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    
    # Apply HAC optimization pass
    add_custom_command(
        OUTPUT ${OPTIMIZED_IR_FILE}
        COMMAND opt -load-pass-plugin $<TARGET_FILE:ReplaceCallWithLoad> 
                -passes=replace-call-with-load -S ${LLVM_IR_FILE} -o ${OPTIMIZED_IR_FILE}
        DEPENDS ${LLVM_IR_FILE} ReplaceCallWithLoad
        COMMENT "Applying HAC optimization to ${ALGO_NAME}"
    )
    
    # Apply LLVM optimizations and replace original IR
    add_custom_command(
        OUTPUT ${LLVM_IR_FILE}_final
        COMMAND opt -O3 ${OPTIMIZED_IR_FILE} -o ${LLVM_IR_FILE}
        COMMAND ${CMAKE_COMMAND} -E touch ${LLVM_IR_FILE}_final
        COMMAND ${CMAKE_COMMAND} -E remove ${OPTIMIZED_IR_FILE}
        DEPENDS ${OPTIMIZED_IR_FILE}
        COMMENT "Applying LLVM O3 optimizations to ${ALGO_NAME}"
    )
    
    # Compile IR to object file
    add_custom_command(
        OUTPUT ${OBJECT_FILE}
        COMMAND llc -filetype=obj -relocation-model=pic ${LLVM_IR_FILE} -o ${OBJECT_FILE}
        DEPENDS ${LLVM_IR_FILE}_final
        COMMENT "Compiling optimized IR to object file for ${ALGO_NAME}"
    )
    
    # Link to shared library
    add_custom_command(
        OUTPUT ${SHARED_LIB}
        COMMAND ${CMAKE_C_COMPILER} ${CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS} 
                -DHANDLER_BUILD -o ${SHARED_LIB} ${OBJECT_FILE} 
                -L${CMAKE_BINARY_DIR}/lib -lpcm -lm
        COMMAND ${CMAKE_COMMAND} -E remove ${OBJECT_FILE}
        DEPENDS ${OBJECT_FILE} pcm
        COMMENT "Linking HAC-optimized shared library for ${ALGO_NAME}"
    )
    
    # Create target for this HAC-optimized algorithm
    add_custom_target(${ALGO_NAME}_hac_optimized ALL DEPENDS ${SHARED_LIB})
    
    # Install the HAC-optimized library
    install(FILES ${SHARED_LIB} DESTINATION lib)
endfunction()

# Create HAC-optimized versions of all algorithms
if(ALGO_PLUGIN_NAMES)
    message(STATUS "HAC optimization enabled - building LLVM IR optimized algorithm libraries")
    foreach(ALGO ${ALGO_PLUGIN_NAMES})
        create_hac_optimized_algo(${ALGO})
    endforeach()
endif()

