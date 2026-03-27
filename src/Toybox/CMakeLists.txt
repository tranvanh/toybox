include(files.cmake)

find_package(Boost REQUIRED)

add_library(Toybox SHARED ${TOYBOX_HEADERS} ${TOYBOX_SOURCES})
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
