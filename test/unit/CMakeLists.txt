# Unit Tests CMakeLists.txt

# Create test executable
add_executable(unit_tests
    TestGPXParser.cpp
    TestGPXModels.cpp
)

# Link against Google Test and our library
target_link_libraries(unit_tests
    PRIVATE
    qGPX
    GTest::gtest_main
)

# Discover tests
gtest_discover_tests(unit_tests)
