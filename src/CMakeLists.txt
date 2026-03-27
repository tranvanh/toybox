add_subdirectory(Toybox)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    add_subdirectory(Toybox.Sandbox)

    if(BUILD_TESTS)
        add_subdirectory(Toybox.Test)
    endif()

    if(BUILD_BENCHMARK)
        add_subdirectory(Toybox.Benchmark)
    endif()
endif()
