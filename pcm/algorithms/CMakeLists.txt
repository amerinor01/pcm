# ===============================================
# Algorithm Plugins
# ===============================================

# Define algorithm names
set(ALGO_PLUGIN_NAMES newreno dctcp swift dcqcn smartt nscc uec_dctcp strack strack_light ops_lb reps momentum cubic)

# Make it available to other subdirectories (e.g., HAC) via cache
set(ALGO_PLUGIN_NAMES ${ALGO_PLUGIN_NAMES} CACHE INTERNAL "List of algorithm plugin names")

# Function to create algorithm plugins
function(create_algo_plugin ALGO_NAME)
    # Set build-time generated file locations
    set(YAML_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${ALGO_NAME}.yaml")
    set(FABRIC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/fabric.yaml")
    set(SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${ALGO_NAME}.c")  # Existing source file
    set(HEADER_FILE "${CMAKE_BINARY_DIR}/algorithms/${ALGO_NAME}.h")  # Generated header
    set(SPEC_FILE "${CMAKE_BINARY_DIR}/algorithms/${ALGO_NAME}_spec.cpp")  # Generated C++ spec file
    
    # Create algorithms output directory
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/algorithms")
    
    # Custom command to generate V2 header and spec files
    add_custom_command(
        OUTPUT ${HEADER_FILE} ${SPEC_FILE}
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../hac/hac --codegen
                --output ${CMAKE_BINARY_DIR}/algorithms --algoconf ${YAML_FILE} --fabricconf ${FABRIC_FILE}
        DEPENDS ${YAML_FILE} ${FABRIC_FILE}
        COMMENT "Generating ${ALGO_NAME} algorithm header and spec source"
    )
    
    # Create algorithm library using existing .c file
    add_library(${ALGO_NAME} SHARED ${SOURCE_FILE})

    # If xxhash imported targets were created in the parent CMake, make sure
    # algorithm targets can include and optionally link against them.
    if(TARGET xxhash_static)
        target_include_directories(${ALGO_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../xxHash)
        target_link_libraries(${ALGO_NAME} PRIVATE xxhash_static)
    elseif(TARGET xxhash_shared)
        target_include_directories(${ALGO_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../xxHash)
        target_link_libraries(${ALGO_NAME} PRIVATE xxhash_shared)
    else()
        # Still add the include dir if the xxHash headers exist in the repo
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../xxHash/xxhash.h)
            target_include_directories(${ALGO_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../xxHash)
        endif()
    endif()
    
    # Add dependency on generated V2 header
    add_custom_target(${ALGO_NAME}_gen DEPENDS ${HEADER_FILE})
    add_dependencies(${ALGO_NAME} ${ALGO_NAME}_gen)
    
    # Special handling for dctcp and newreno
    if(ALGO_NAME STREQUAL "dctcp" OR ALGO_NAME STREQUAL "newreno")
        target_sources(${ALGO_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tcp_utils.c")
    endif()
    
    # Compile definitions
    string(TOUPPER ${ALGO_NAME} ALGO_NAME_UPPER)
    target_compile_definitions(${ALGO_NAME} PRIVATE 
        HANDLER_BUILD 
        BUILD_ALGO_${ALGO_NAME_UPPER}
    )
    
    # Add assembly generation
    target_compile_options(${ALGO_NAME} PRIVATE -save-temps=obj -gno-record-gcc-switches -Wa,--no-warn)
    
    # Link with PCM and math library
    target_link_libraries(${ALGO_NAME} PRIVATE m)
    
    # Set library properties
    set_target_properties(${ALGO_NAME} PROPERTIES
        OUTPUT_NAME ${ALGO_NAME}
        PREFIX "lib"
        SUFFIX ".so"
    )
    
    # Install
    install(TARGETS ${ALGO_NAME} LIBRARY DESTINATION lib)
    
    # Create PCMC V2 version (using generated V2 C++ spec file from build directory)
    add_library(${ALGO_NAME}_spec SHARED ${SPEC_FILE})
    target_link_libraries(${ALGO_NAME}_spec PRIVATE)
    set_target_properties(${ALGO_NAME}_spec PROPERTIES
        OUTPUT_NAME ${ALGO_NAME}_spec
        PREFIX "lib"
        SUFFIX ".so"
    )
    install(TARGETS ${ALGO_NAME}_spec LIBRARY DESTINATION lib)
endfunction()

# Create all algorithm plugins
foreach(ALGO ${ALGO_PLUGIN_NAMES})
    create_algo_plugin(${ALGO})
endforeach()
