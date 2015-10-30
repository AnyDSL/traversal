get_filename_component(TRAVERSAL_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
set(TRAVERSAL_INCLUDE_DIR "${TRAVERSAL_CMAKE_DIR}/src/frontend")

find_library(TRAVERSAL_LIBRARY traversal PATHS ${TRAVERSAL_CMAKE_DIR}/build_release/src ${TRAVERSAL_CMAKE_DIR}/build_debug/src ${TRAVERSAL_CMAKE_DIR}/build/src)
find_file(TRAVERSAL_EXECUTABLE NAMES frontend frontend.exe PATHS ${TRAVERSAL_CMAKE_DIR}/build_release/src ${TRAVERSAL_CMAKE_DIR}/build_debug/src ${TRAVERSAL_CMAKE_DIR}/build/src)

