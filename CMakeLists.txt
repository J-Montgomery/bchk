cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
add_library(bchk_core src/extractor.cpp src/checker.cpp)
target_include_directories(bchk_core PUBLIC src)
target_include_directories(bchk_core SYSTEM PUBLIC
                                     ${dartfrog_INCLUDE_DIRS}
                                     ${LLVM_INCLUDE_DIRS}
                                     ${CLANG_INCLUDE_DIRS})
target_compile_definitions(bchk_core PRIVATE ${LLVM_DEFINITIONS}
                           BCHK_INCLUDE_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/headers/bchk2.h")
target_compile_definitions(bchk_core PUBLIC
                           BCHK_SYSTEM_SUMMARIES="${CMAKE_CURRENT_SOURCE_DIR}/summary/std.summaries")
target_compile_options(bchk_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(bchk_core PUBLIC ${LIBCLANG_CPP} ${LIBLLVM_SHARED})


## Linter

find_program(CLANG_TIDY NAMES clang-tidy)

if(CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY};--fix;--fix-errors")
endif()


## Formatter

find_program(CLANG_FORMAT NAMES clang-format)

### This would ideally use get_target_property() over the targets, but that feels
### more fragile to me
file(GLOB_RECURSE ALL_SOURCE_FILES
    CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.hpp"
    "headers/*.h"
    "test/*.cpp"
    "test/*.hpp"
)

if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting..."
        VERBATIM)
else()
    message(STATUS "clang-format not found. Disabling format target")
endif()


## BCHK main
add_executable(bchk src/main.cpp)
target_compile_options(bchk PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable)
target_compile_definitions(bchk PRIVATE
                           LLVM_VERSION="${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}.${LLVM_VERSION_PATCH}"
)
target_link_libraries(bchk PRIVATE bchk_core)

enable_testing()
add_subdirectory(test)