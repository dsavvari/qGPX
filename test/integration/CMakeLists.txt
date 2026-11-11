cmake_minimum_required(VERSION 3.20)
project(qGPXConsumer LANGUAGES CXX)

if(QGPX_SOURCE_DIR)
    add_subdirectory("${QGPX_SOURCE_DIR}" qgpx)
else()
    find_package(qGPX 0.2 CONFIG REQUIRED)
endif()

if(TARGET qgpx-viewer OR TARGET qgpx_viewer_server OR TARGET unit_tests)
    message(FATAL_ERROR "The library consumer must not build the viewer or library tests")
endif()

add_executable(qgpx-consumer main.cpp)
target_link_libraries(qgpx-consumer PRIVATE qGPX::qGPX)
enable_testing()
add_test(NAME qgpx-consumer COMMAND qgpx-consumer)
