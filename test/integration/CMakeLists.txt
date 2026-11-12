cmake_minimum_required(VERSION 3.20)

add_executable(test_analyse test_analyse.cpp)
target_link_libraries(test_analyse PRIVATE qGPX)

add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser PRIVATE qGPX)

add_executable(test_elevation test_elevation.cpp)
target_link_libraries(test_elevation PRIVATE qGPX)

set(TEST_GPX_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../activity_22044819196.gpx")
add_test(NAME test_analyse COMMAND test_analyse)
add_test(NAME test_parser COMMAND test_parser "${TEST_GPX_FILE}")
add_test(NAME test_elevation COMMAND test_elevation "${TEST_GPX_FILE}")
