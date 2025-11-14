# ===============================================
# Traffic Generator Application
# ===============================================

set(TGEN_SOURCES
    main.c
    traffic_gen.c
)

add_executable(traffic_gen_app ${TGEN_SOURCES})
target_link_libraries(traffic_gen_app PRIVATE pcm)
install(TARGETS traffic_gen_app RUNTIME DESTINATION bin)
