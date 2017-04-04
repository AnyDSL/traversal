get_filename_component(TRAVERSAL_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
set(TRAVERSAL_INCLUDE_DIR "${TRAVERSAL_CMAKE_DIR}/src/frontend")

find_library(TRAVERSAL_LIBRARY_GPU traversal_gpu PATHS ${TRAVERSAL_CMAKE_DIR}/build_release/src ${TRAVERSAL_CMAKE_DIR}/build_debug/src ${TRAVERSAL_CMAKE_DIR}/build/src)
find_file(TRAVERSAL_EXECUTABLE_GPU NAMES frontend_gpu frontend_gpu.exe PATHS ${TRAVERSAL_CMAKE_DIR}/build_release/src ${TRAVERSAL_CMAKE_DIR}/build_debug/src ${TRAVERSAL_CMAKE_DIR}/build/src)

find_library(TRAVERSAL_LIBRARY_CPU traversal_cpu PATHS ${TRAVERSAL_CMAKE_DIR}/build_release/src ${TRAVERSAL_CMAKE_DIR}/build_debug/src ${TRAVERSAL_CMAKE_DIR}/build/src)
find_file(TRAVERSAL_EXECUTABLE_CPU NAMES frontend_cpu frontend_cpu.exe PATHS ${TRAVERSAL_CMAKE_DIR}/build_release/src ${TRAVERSAL_CMAKE_DIR}/build_debug/src ${TRAVERSAL_CMAKE_DIR}/build/src)
