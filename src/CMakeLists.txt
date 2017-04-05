# Source files common to the frontend and the viewer tool
set(FRONTEND_SRCS
    frontend/bvh_format.h
    frontend/load_rays.cpp
    frontend/loaders.h
    frontend/options.h
    frontend/traversal.h)

# Common impala files used in both the CPU and GPU versions
set(COMMON_SRCS
    common/debug.impala
    common/float.impala
    common/intersection.impala
    common/stack.impala
    common/range.impala
    common/vector.impala
    common/transform.impala
    common/transparency.impala
    common/instancing.impala
    common/traversal.impala)

set(CLANG_FLAGS -mavx2 -mavx -mfma -ffast-math CACHE ON "CLANG compilation options (AVX2 and AVX are required on the CPU)")

function(generate_traversal)
    cmake_parse_arguments("PARGS" "" "NAME;FRONTEND;LOADER;MAPPING;VIEWER" "DEFS" ${ARGN})
    if(NOT "${PARGS_UNPARSED_ARGUMENTS}" STREQUAL "")
        message(FATAL_ERROR "Unparsed arguments ${PARGS_UNPARSED_ARGUMENTS}")
    endif()

    # Traversal library on the CPU
    set(_impala_srcs ${PARGS_MAPPING} ${COMMON_SRCS})

    anydsl_runtime_wrap(_impala_program
                        NAME ${PARGS_NAME}
                        INTERFACE "frontend/${PARGS_NAME}"
                        CLANG_FLAGS ${CLANG_FLAGS}
                        FILES ${_impala_srcs})

    set(_interface_target ${PARGS_NAME}-interface)
    add_custom_target(${_interface_target} DEPENDS frontend/${PARGS_NAME}.h)

    add_library(${PARGS_NAME} ${_impala_program})
    target_link_libraries(${PARGS_NAME} ${ANYDSL_RUNTIME_LIBRARY} ${ANYDSL_RUNTIME_LIBRARIES})
    target_compile_definitions(${PARGS_NAME} PUBLIC ${PARGS_DEFS})

    add_executable(${PARGS_FRONTEND} frontend/main.cpp ${FRONTEND_SRCS} ${PARGS_LOADER})
    add_dependencies(${PARGS_FRONTEND} ${_interface_target})
    target_link_libraries(${PARGS_FRONTEND} ${PARGS_NAME})

    add_executable(${PARGS_VIEWER}
        tools/viewer.cpp
        frontend/load_mesh.cpp
        ${PARGS_LOADER}
        ${FRONTEND_SRCS})
    add_dependencies(${PARGS_VIEWER} ${_interface_target})
    target_compile_definitions(${PARGS_VIEWER} PUBLIC ${PARGS_DEFS})
    target_link_libraries(${PARGS_VIEWER} ${PARGS_NAME} ${SDL2_LIBRARY})
endfunction()

generate_traversal(NAME traversal_cpu
                   VIEWER viewer_cpu
                   FRONTEND frontend_cpu
                   LOADER frontend/load_mbvh.cpp
                   MAPPING mappings/mapping_cpu.impala
                   DEFS
                        -Dget_time=anydsl_get_micro_time
                        -Dintersect=intersect_cpu
                        -Doccluded=occluded_cpu
                        -DTRAVERSAL_PLATFORM=Host
                        -DTRAVERSAL_DEVICE=0
                        # Prevent conflicts between traversal_cpu/traversal_gpu
                        -DTRAVERSAL_CPU)

generate_traversal(NAME traversal_gpu
                   VIEWER viewer_gpu
                   FRONTEND frontend_gpu
                   LOADER frontend/load_bvh.cpp
                   MAPPING mappings/mapping_gpu.impala
                   DEFS
                        -Dget_time=anydsl_get_kernel_time
                        -Dintersect=intersect_gpu
                        -Doccluded=occluded_gpu
                        -DTRAVERSAL_PLATFORM=Cuda
                        -DTRAVERSAL_DEVICE=0
                        # Prevent conflicts between traversal_cpu/traversal_gpu
                        -DTRAVERSAL_GPU)

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

add_executable(fbufdiff tools/fbufdiff.cpp)

add_executable(gen_primary tools/gen_primary.cpp ${TOOLS_COMMON_SRCS})
add_executable(gen_random  tools/gen_random.cpp  ${TOOLS_COMMON_SRCS})
add_executable(gen_shadow  tools/gen_shadow.cpp  ${TOOLS_COMMON_SRCS})
