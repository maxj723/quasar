# Define the test executable, built from our test source files
add_executable(core_tests
    OrderBookTests.cpp
    MatchingEngineTests.cpp
)

# Define the load test executable separately for performance testing
add_executable(load_tests
    LoadTests.cpp
)

# Link the test executable against our engine library and Google Test
target_link_libraries(core_tests
    PRIVATE
    engine_core
    gtest_main
)

# Link the load test executable
target_link_libraries(load_tests
    PRIVATE
    engine_core
    gtest_main
)

# Add the test executable to CTest for running
include(GoogleTest)
gtest_discover_tests(core_tests)
gtest_discover_tests(load_tests)
