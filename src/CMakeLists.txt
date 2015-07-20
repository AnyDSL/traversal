set(FRONTEND_SRCS
    frontend/load_rays.cpp
    frontend/main.cpp
    frontend/loaders.h
    frontend/options.h
    frontend/interface.h)

set(COMMON_SRCS
    common/debug.impala
    common/float.impala
    common/intersection.impala
    common/stack.impala
    common/range.impala
    common/vector.impala
    common/traversal.impala)

if(BACKEND STREQUAL "cpu" OR BACKEND STREQUAL "avx")
    set(MAPPING_SRCS mappings/mapping_packet.impala)
    set(FRONTEND_SRCS ${FRONTEND_SRCS} frontend/load_mbvh.cpp)
elseif(BACKEND STREQUAL "nvvm" OR BACKEND STREQUAL "cuda")
    set(MAPPING_SRCS mappings/mapping_gpu.impala)
    set(FRONTEND_SRCS ${FRONTEND_SRCS} frontend/load_bvh.cpp)
else()
    message(FATAL_ERROR "Backend not supported")
endif()

set(IMPALA_SRCS ${MAPPING_SRCS} ${COMMON_SRCS})

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

set_source_files_properties(frontend/interface.h PROPERTIES GENERATED TRUE)

add_executable(traversal ${IMPALA_OBJS} ${FRONTEND_SRCS})
target_link_libraries(traversal ${IMPALA_LIBS})

