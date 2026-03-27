cmake_minimum_required(VERSION 3.22)
project(Toybox)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
include(FetchContent)

# -----------------------------------------------------------------------
# Top-level only: build tooling, generator check, custom configurations
# -----------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)

    if(NOT CMAKE_GENERATOR STREQUAL "Ninja Multi-Config")
        message(FATAL_ERROR
            "This project must be built with Ninja Multi-Config.\n"
            "Run cmake with: -G \"Ninja Multi-Config\""
        )
    endif()

    list(APPEND CMAKE_CONFIGURATION_TYPES "Assert")
    list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "" FORCE)

    set(CMAKE_CXX_FLAGS_ASSERT "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_ASSERT   "${CMAKE_C_FLAGS_DEBUG}"   CACHE STRING "" FORCE)

    option(BUILD_TESTS     "Build tests"      OFF)
    option(BUILD_BENCHMARK "Build benchmarks" OFF)

    enable_testing()

    if(BUILD_TESTS)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
            GIT_SHALLOW TRUE
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    if(BUILD_BENCHMARK)
        FetchContent_Declare(
            googlebenchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.9.4
            GIT_SHALLOW TRUE
        )
        set(BENCHMARK_ENABLE_TESTING      OFF CACHE BOOL "" FORCE)
        set(BENCHMARK_ENABLE_GTEST_TESTS  OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googlebenchmark)
    endif()
endif()

# -----------------------------------------------------------------------
# Library
# -----------------------------------------------------------------------
include(files.cmake)

find_package(Boost REQUIRED)

add_library(Toybox SHARED ${TOYBOX_SOURCES})
set_target_properties(Toybox PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(Toybox Boost::headers)

target_include_directories(Toybox PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(Toybox PUBLIC
    $<$<CONFIG:Release>:RELEASE_BUILD>
    $<$<CONFIG:Assert>:ASSERT_BUILD>
    $<$<CONFIG:Debug>:DEBUG_BUILD>
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(TARGETS Toybox
    EXPORT ToyboxTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(EXPORT ToyboxTargets
    FILE ToyboxConfig.cmake
    NAMESPACE Toybox::
    DESTINATION lib/cmake/Toybox
)

# -----------------------------------------------------------------------
# Top-level only: sandbox, tests, benchmarks
# -----------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    add_subdirectory(sandbox)

    if(BUILD_TESTS)
        add_subdirectory(tests)
    endif()

    if(BUILD_BENCHMARK)
        add_subdirectory(benchmarks)
    endif()
endif()
