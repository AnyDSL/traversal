project(TRAVERSAL)

cmake_minimum_required(VERSION 2.4)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

find_package(THORIN REQUIRED)

include(${THORIN_RUNTIME_DIR}/cmake/ThorinRuntime.cmake)
include_directories(${THORIN_RUNTIME_INCLUDE_DIRS})

if(BACKEND STREQUAL "cpu" OR BACKEND STREQUAL "avx")
    set(BACKEND "cpu")
else()
    set(BACKEND "nvvm")
endif()

set(ACCEL "bvh" CACHE STRING "Acceleration structure type (bvh, ...)")
set(VECTORIZE "" CACHE STRING "Hand coded vectorization length (1, 4, 8, 16)")

find_package(Threads REQUIRED)

add_subdirectory(src)

