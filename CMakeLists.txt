cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(
    bchk
    VERSION 0.0.1
    LANGUAGES C CXX
)

find_package(GTest REQUIRED)
file(GLOB_RECURSE ALL_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp ${CMAKE_CURRENT_SOURCE_DIR}/headers/*.h)
add_custom_target(format COMMAND clang-format-18 -i ${ALL_SOURCE_FILES})

find_package(Clang CONFIG REQUIRED)
find_package(LLVM CONFIG REQUIRED)

find_package(dartfrog REQUIRED)
include_directories(${dartfrog_INCLUDE_DIRS})

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g1")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -Wall")
endif()

add_executable(borrowck src/main.cpp)
target_link_libraries(borrowck PRIVATE
    clang-cpp
    LLVM
    dartfrog
)
