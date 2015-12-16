set(FRONTEND_SRCS main.cpp grid.h float3.h bbox.h common.h tribox.c)
set(IMPALA_SRCS grid.impala)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/grid.h
                   COMMAND impala -emit-c-interface ${IMPALA_SRCS}
                          ${THORIN_RUNTIME_DIR}/runtime.impala
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_${BACKEND}.impala
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala
                          -o grid
                   DEPENDS ${IMPALA_SRCS}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

thorin_runtime_wrap(IMPALA_PROGRAM
                    BACKEND ${BACKEND}
                    FILES ${IMPALA_SRCS})

set_source_files_properties(grid.h PROPERTIES GENERATED TRUE)

add_executable(hagrid ${FRONTEND_SRCS} ${IMPALA_PROGRAM})
target_link_libraries(hagrid ${THORIN_RUNTIME_LIBRARY} ${THORIN_RUNTIME_LIBRARIES} ${SDL_LIBRARY} -lassimp)

