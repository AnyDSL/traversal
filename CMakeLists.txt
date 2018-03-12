project(Traversal)

cmake_minimum_required(VERSION 3.1)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

set(CMAKE_CXX_STANDARD 11)

set(GPU_BACKEND ${GPU_BACKEND} CACHE STRING "select the GPU backend from the following: NVVM, AMDGPU")
if(NOT GPU_BACKEND)
    set(GPU_BACKEND nvvm CACHE STRING "select the GPU backend from the following: NVVM, AMDGPU" FORCE)
endif()
string(TOLOWER "${GPU_BACKEND}" GPU_BACKEND)
message(STATUS "Selected GPU backend: ${GPU_BACKEND}")
set(GPU_INTRINSICS intrinsics_${GPU_BACKEND}.impala)

if(GPU_BACKEND STREQUAL "nvvm")
    set(GPU_PLATFORM "Cuda")
    set(GPU_DEVICE "0")
else()
    set(GPU_PLATFORM "HSA")
    set(GPU_DEVICE "1")
endif()

find_package(AnyDSL_runtime REQUIRED)
include_directories(${AnyDSL_runtime_INCLUDE_DIRS})

find_package(Threads REQUIRED)
find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

add_subdirectory(src)
