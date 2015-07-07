set(FRONTEND_SRCS
    frontend/load_bvh.cpp
    frontend/load_rays.cpp
    frontend/main.cpp
    frontend/loaders.h
    frontend/options.h
    frontend/interface.h)

set(IMPALA_SRCS common/traversal.impala)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/frontend/interface.h
                   COMMAND impala -emit-c-interface ${IMPALA_SRCS}
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_${BACKEND}.impala
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala
                          -o frontend/interface
                   DEPENDS ${IMPALA_SRCS}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

thorin_runtime_wrap(IMPALA_OBJS IMPALA_LIBS
                    BACKEND ${BACKEND}
                    FILES ${IMPALA_SRCS})

#set_source_files_properties(frontend/interface.h PROPERTIES GENERATED TRUE)

add_executable(traversal ${IMPALA_OBJS} ${FRONTEND_SRCS})
target_link_libraries(traversal ${IMPALA_LIBS})

