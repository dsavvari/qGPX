cmake_minimum_required(VERSION 3.20)
project(qGPX VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(qGPX STATIC
    src/GPXAnalytics.cpp
    src/GPXCore.cpp
    src/GPXParser.cpp
)
add_library(qGPX::qGPX ALIAS qGPX)
target_compile_features(qGPX PUBLIC cxx_std_17)
set_target_properties(qGPX PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(qGPX PRIVATE /W4)
else()
    target_compile_options(qGPX PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(qGPX
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/qGPX>
)

# XML Parsing Library - libexpat (system library on macOS/iOS)
find_package(EXPAT REQUIRED)
target_link_libraries(qGPX PUBLIC EXPAT::EXPAT)

# Unit Tests with Google Test

set(QGPX_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(QGPX_TOP_LEVEL ON)
endif()
option(BUILD_TESTS "Build tests" ${QGPX_TOP_LEVEL})
option(BUILD_VIEWER "Build the local browser-based GPX viewer" OFF)

if(BUILD_TESTS)
    enable_testing()

    # Fetch Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Include Google Test's cmake utilities
    include(GoogleTest)

    # Add unit tests
    add_subdirectory(test/unit)

    # Standalone test executables
    add_executable(test_parser test/test_parser.cpp)
    target_link_libraries(test_parser PRIVATE qGPX)

    add_executable(test_elevation test/test_elevation.cpp)
    target_link_libraries(test_elevation PRIVATE qGPX)
endif()

if(BUILD_VIEWER)
    add_subdirectory(viewer)
endif()

install(TARGETS qGPX
    EXPORT qGPXTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    include/GPXModels.hpp
    include/GPXParser.hpp
    include/GPXAnalytics.hpp
    include/GPXCore.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qGPX
)

set(QGPX_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/qGPX")
configure_package_config_file(
    cmake/qGPXConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/qGPXConfig.cmake
    INSTALL_DESTINATION ${QGPX_CMAKE_INSTALL_DIR}
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/qGPXConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(EXPORT qGPXTargets
    NAMESPACE qGPX::
    DESTINATION ${QGPX_CMAKE_INSTALL_DIR}
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/qGPXConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/qGPXConfigVersion.cmake
    DESTINATION ${QGPX_CMAKE_INSTALL_DIR}
)

# Print Configuration Summary

message(STATUS "")
message(STATUS "qGPX C++ Core Configuration:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: C++17 minimum")
message(STATUS "  Build Tests:  ${BUILD_TESTS}")
message(STATUS "  Build Viewer: ${BUILD_VIEWER}")
message(STATUS "")
