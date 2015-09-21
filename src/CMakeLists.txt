set(FRONTEND_SRCS
    frontend/bvh_format.h
    frontend/load_rays.cpp
    frontend/main.cpp
    frontend/loaders.h
    frontend/options.h
    frontend/traversal.h)

set(COMMON_SRCS
    common/debug.impala
    common/float.impala
    common/intersection.impala
    common/stack.impala
    common/range.impala
    common/vector.impala
    common/traversal.impala)

if(BACKEND STREQUAL "cpu" OR BACKEND STREQUAL "avx")
    set(MAPPING_SRCS mappings/mapping_cpu.impala)
    set(LOADER_SRC frontend/load_mbvh.cpp)
    set(TRAVERSAL_DEFS "get_time=thorin_get_micro_time" CPU)
elseif(BACKEND STREQUAL "nvvm" OR BACKEND STREQUAL "cuda")
    set(MAPPING_SRCS mappings/mapping_gpu.impala)
    set(LOADER_SRC frontend/load_bvh.cpp)
    set(TRAVERSAL_DEFS "get_time=thorin_get_kernel_time" GPU)
else()
    message(FATAL_ERROR "Backend not supported")
endif()

set(FRONTEND_SRCS ${FRONTEND_SRCS} ${LOADER_SRC})
set(IMPALA_SRCS ${MAPPING_SRCS} ${COMMON_SRCS})

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/frontend/traversal.h
                   COMMAND impala -emit-c-interface ${IMPALA_SRCS}
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_${BACKEND}.impala
                          ${THORIN_RUNTIME_DIR}/platforms/intrinsics_thorin.impala
                          -o frontend/traversal
                   DEPENDS ${IMPALA_SRCS}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

thorin_runtime_wrap(IMPALA_OBJS IMPALA_LIBS
                    BACKEND ${BACKEND}
                    FILES ${IMPALA_SRCS})

set_source_files_properties(frontend/traversal.h PROPERTIES GENERATED TRUE)

# Traversal library
add_library(traversal ${IMPALA_OBJS})
target_link_libraries(traversal ${IMPALA_LIBS})
target_compile_definitions(traversal PUBLIC ${TRAVERSAL_DEFS})

# Traversal frontend
add_executable(frontend ${FRONTEND_SRCS})
target_link_libraries(frontend traversal)

# Tools
set(TOOLS_COMMON_SRCS
    frontend/bvh_format.h
    frontend/loaders.h
    frontend/options.h
    frontend/traversal.h
    tools/linear.h
    tools/camera.h)

add_executable(fbuf2png tools/fbuf2png.cpp ${TOOLS_COMMON_SRCS})
target_link_libraries(fbuf2png ${PNG_LIBRARIES})

add_executable(gen_primary tools/gen_primary.cpp ${TOOLS_COMMON_SRCS})
add_executable(gen_random  tools/gen_random.cpp  ${TOOLS_COMMON_SRCS})
add_executable(gen_shadow  tools/gen_shadow.cpp  ${TOOLS_COMMON_SRCS})

add_executable(viewer tools/viewer.cpp ${LOADER_SRC} ${TOOLS_COMMON_SRCS})
target_link_libraries(viewer traversal ${SDL_LIBRARY})

