find_package(Threads REQUIRED)

add_library(qgpx_viewer_server STATIC ViewerServer.cpp)
target_include_directories(qgpx_viewer_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(qgpx_viewer_server SYSTEM PUBLIC
    ${PROJECT_SOURCE_DIR}/third_party/cpp-httplib
    ${PROJECT_SOURCE_DIR}/third_party/nlohmann-json
)
target_link_libraries(qgpx_viewer_server PUBLIC qGPX Threads::Threads)

add_executable(qgpx-viewer main.cpp)
target_link_libraries(qgpx-viewer PRIVATE qgpx_viewer_server)
set_target_properties(qgpx-viewer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_compile_definitions(qgpx-viewer PRIVATE
    QGPX_VIEWER_ASSET_DIR="${PROJECT_SOURCE_DIR}/viewer/dist"
)

if(BUILD_TESTS)
    add_executable(viewer_tests ${PROJECT_SOURCE_DIR}/test/viewer/TestViewer.cpp)
    target_link_libraries(viewer_tests PRIVATE qgpx_viewer_server GTest::gtest_main)
    target_compile_definitions(viewer_tests PRIVATE
        QGPX_TEST_ASSET_DIR="${PROJECT_SOURCE_DIR}/test/viewer/assets"
    )
    gtest_discover_tests(viewer_tests)
endif()
