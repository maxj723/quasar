cmake_minimum_required(VERSION 3.14)
project(matching-engine)

# --- Google Test ---
# Downloads and prepares the Google Test framework
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Matching Engine Library ---
# Compiles the core engine source files into a reusable library
add_library(engine_core
    src/core/MatchingEngine.cpp
    src/core/Order.cpp
    src/core/OrderBook.cpp
    src/core/Trade.cpp
)

# Makes the include directories available to other targets
target_include_directories(engine_core PUBLIC include)

# --- Tests ---
# Enables testing and includes the 'tests' subdirectory where our tests live
enable_testing()
add_subdirectory(tests)

# --- Main Executable ---
add_executable(matching_engine_cli
    src/main/pipeline_main.cpp
)

target_link_libraries(matching_engine_cli
    PRIVATE
    engine_core
)

# --- Benchmark Executable ---
add_executable(matching_engine_benchmark
    src/main/benchmark_main.cpp
)

target_link_libraries(matching_engine_benchmark
    PRIVATE
    engine_core
)

# --- Clean Rules ---
# Clean all executables and result files (but not build directory itself)
add_custom_target(clean-all
    COMMAND ${CMAKE_MAKE_PROGRAM} clean
    COMMAND ${CMAKE_COMMAND} -E remove -f matching_engine_cli
    COMMAND ${CMAKE_COMMAND} -E remove -f matching_engine_benchmark
    COMMAND ${CMAKE_COMMAND} -E remove -f tests/load_tests
    COMMAND ${CMAKE_COMMAND} -E remove -f tests/core_tests
    COMMAND ${CMAKE_COMMAND} -E remove_directory results || true
    COMMAND ${CMAKE_COMMAND} -E remove_directory ../results || true
    COMMAND ${CMAKE_COMMAND} -E remove_directory _deps || true
    COMMAND ${CMAKE_COMMAND} -E remove_directory CMakeFiles || true
    COMMAND ${CMAKE_COMMAND} -E remove_directory Testing || true
    COMMAND ${CMAKE_COMMAND} -E remove_directory tests || true
    COMMAND ${CMAKE_COMMAND} -E remove -f CMakeCache.txt || true
    COMMAND ${CMAKE_COMMAND} -E remove -f cmake_install.cmake || true
    COMMAND ${CMAKE_COMMAND} -E remove -f CTestTestfile.cmake || true
    COMMAND ${CMAKE_COMMAND} -E remove -f Makefile || true
    COMMENT "Cleaning all build artifacts, executables and result files"
)

# Clean only result files
add_custom_target(clean-results
    COMMAND ${CMAKE_COMMAND} -E remove_directory results || true
    COMMAND ${CMAKE_COMMAND} -E remove_directory ../results || true
    COMMENT "Cleaning all result files"
)

# Clean executables only (keep results)
add_custom_target(clean-executables
    COMMAND ${CMAKE_MAKE_PROGRAM} clean
    COMMAND ${CMAKE_COMMAND} -E remove -f matching_engine_cli
    COMMAND ${CMAKE_COMMAND} -E remove -f matching_engine_benchmark
    COMMAND ${CMAKE_COMMAND} -E remove -f tests/load_tests
    COMMAND ${CMAKE_COMMAND} -E remove -f tests/core_tests
    COMMENT "Cleaning all executables but keeping result files"
)
