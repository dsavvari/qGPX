cmake_minimum_required(VERSION 3.20)
project(qGPX VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler options
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(qGPX STATIC
    src/GPXAnalytics.cpp
    src/GPXCore.cpp
    src/GPXParser.cpp
)

target_include_directories(qGPX
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/qGPX>
)

# XML Parsing Library - libexpat (system library on macOS/iOS)
find_package(EXPAT REQUIRED)
target_link_libraries(qGPX PUBLIC EXPAT::EXPAT)

# Unit Tests with Google Test

option(BUILD_TESTS "Build tests" ON)

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

install(TARGETS qGPX
    ARCHIVE DESTINATION lib
)

install(FILES
    include/GPXModels.hpp
    include/GPXParser.hpp
    include/GPXAnalytics.hpp
    include/GPXCore.h
    DESTINATION include/qGPX
)

# Print Configuration Summary

message(STATUS "")
message(STATUS "qGPX C++ Core Configuration:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Tests:  ${BUILD_TESTS}")
message(STATUS "")
