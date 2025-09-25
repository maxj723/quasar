cmake_minimum_required(VERSION 3.14)
project(quasar-load-tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Load test executable
add_executable(pipeline_load_test
    pipeline_load_test.cpp
)

# Set compiler options for performance and debugging
target_compile_options(pipeline_load_test PRIVATE
    -Wall -Wextra -O3 -march=native
    $<$<CONFIG:Debug>:-g -O0>
)

# Link required libraries
target_link_libraries(pipeline_load_test
    PRIVATE
    pthread
)

# Set output directory
set_target_properties(pipeline_load_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

message(STATUS "Pipeline Load Test configured")
message(STATUS "Build with: cmake --build . --config Release")