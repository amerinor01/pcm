# ===============================================
# Core PCM Library
# ===============================================

# Find all core PCM C sources in current directory
file(GLOB PCM_C_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

# Create libpcm.so
add_library(pcm SHARED ${PCM_C_SOURCES})

# Set library properties
set_target_properties(pcm PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME pcm
)

# Install library
install(TARGETS pcm
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers (from parent include directory)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)
