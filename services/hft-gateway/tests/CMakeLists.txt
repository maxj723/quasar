# --- Google Test ---
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Test Executables ---
add_executable(hft_gateway_tests
    GatewayConfigTests.cpp
    HFTGatewayTests.cpp
    ClientSessionTests.cpp
    ../src/HFTGateway.cpp
    ../src/kafka/KafkaClient.cpp
)

target_include_directories(hft_gateway_tests PRIVATE
    ../include
    ${CMAKE_BINARY_DIR}
    ${BOOST_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

target_link_libraries(hft_gateway_tests
    PRIVATE
    gtest
    gtest_main
)

# Set compiler flags
target_compile_options(hft_gateway_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)

# Add test discovery
include(GoogleTest)
gtest_discover_tests(hft_gateway_tests)