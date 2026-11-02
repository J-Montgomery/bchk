cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(
    bchk
    VERSION 0.0.1
    LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g1")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -Wall")
endif()

## Clang
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

find_library(LIBCLANG_CPP clang-cpp HINTS ${LLVM_LIBRARY_DIRS})
find_library(LIBLLVM_SHARED LLVM-${LLVM_VERSION_MAJOR} HINTS ${LLVM_LIBRARY_DIRS})

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})

## Dartfrog
find_package(dartfrog REQUIRED)

## BCHK core
add_library(bchk_core src/extractor.cpp)
target_include_directories(bchk_core PUBLIC src)
target_include_directories(bchk_core SYSTEM PUBLIC
                                     ${dartfrog_INCLUDE_DIRS}
                                     ${LLVM_INCLUDE_DIRS}
                                     ${CLANG_INCLUDE_DIRS})
target_compile_definitions(bchk_core PRIVATE ${LLVM_DEFINITIONS}
                           BCHK_INCLUDE_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/headers/bchk2.h")
target_compile_definitions(bchk_core PUBLIC
                           BCHK_SYSTEM_SUMMARIES="${CMAKE_CURRENT_SOURCE_DIR}/summary/std.summaries")
target_compile_options(bchk_core PRIVATE -Wall -Wextra)
target_link_libraries(bchk_core PUBLIC ${LIBCLANG_CPP} ${LIBLLVM_SHARED})

## Formatter
find_program(CLANG_FORMAT 
    NAMES clang-format-${LLVM_VERSION_MAJOR} clang-format
    DOC "Path to clang-format executable"
)

if(CLANG_FORMAT)
    add_custom_target(format 
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting..."
        VERBATIM
    )
else()
    message(STATUS "clang-format not found. Disabling format target")
endif()

## BCHK main
add_executable(bchk src/main.cpp)
target_compile_options(bchk PRIVATE -Wall -Wextra)
target_link_libraries(bchk PRIVATE bchk_core)

enable_testing()
add_subdirectory(test)