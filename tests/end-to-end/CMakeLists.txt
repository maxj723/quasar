cmake_minimum_required(VERSION 3.14)
project(quasar-e2e-tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Find the built libraries from our services
set(MATCHING_ENGINE_DIR "${CMAKE_SOURCE_DIR}/../../services/matching-engine")
set(HFT_GATEWAY_DIR "${CMAKE_SOURCE_DIR}/../../services/hft-gateway")

# Include directories
include_directories(${MATCHING_ENGINE_DIR}/include)
include_directories(${HFT_GATEWAY_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Find the built static libraries
find_library(ENGINE_CORE_LIB NAMES engine_core
             PATHS ${MATCHING_ENGINE_DIR}/build
             NO_DEFAULT_PATH REQUIRED)

# Find or create mock headers
set(MOCK_INCLUDES_DIR "${CMAKE_CURRENT_BINARY_DIR}/mock_includes")
file(MAKE_DIRECTORY ${MOCK_INCLUDES_DIR})

# Create mock headers for Kafka and other dependencies
file(WRITE ${MOCK_INCLUDES_DIR}/kafka/KafkaClient.h
     "#pragma once\n"
     "#include <string>\n"
     "#include <vector>\n"
     "namespace kafka {\n"
     "struct KafkaConfig { std::string brokers; std::string orders_new_topic; };\n"
     "class KafkaClient {\n"
     "public:\n"
     "    KafkaClient(const KafkaConfig&) {}\n"
     "    bool initialize() { return true; }\n"
     "    void shutdown() {}\n"
     "};\n"
     "}\n")

# Create mock Boost ASIO headers
file(MAKE_DIRECTORY ${MOCK_INCLUDES_DIR}/boost/asio)
file(WRITE ${MOCK_INCLUDES_DIR}/boost/asio.hpp "#pragma once\nnamespace boost { namespace asio { class io_context {}; }}")

# Create mock spdlog headers
file(MAKE_DIRECTORY ${MOCK_INCLUDES_DIR}/spdlog)
file(WRITE ${MOCK_INCLUDES_DIR}/spdlog/spdlog.h
     "#pragma once\n"
     "namespace spdlog { \n"
     "enum class level { debug, info };\n"
     "class logger { public: void set_level(level) {} };\n"
     "inline void info(const std::string&) {}\n"
     "inline void error(const std::string&) {}\n"
     "}\n")

# Create mock messages_generated.h
file(WRITE ${MOCK_INCLUDES_DIR}/messages_generated.h
     "#pragma once\n"
     "namespace quasar { namespace schema {\n"
     "inline bool VerifyMessageBuffer(const void*) { return true; }\n"
     "}}\n")

include_directories(${MOCK_INCLUDES_DIR})

# End-to-end test executable
add_executable(e2e_tests
    e2e_test.cpp
    ${HFT_GATEWAY_DIR}/src/HFTGateway.cpp
    ${HFT_GATEWAY_DIR}/src/kafka/KafkaClient.cpp
    ${MATCHING_ENGINE_DIR}/src/kafka/KafkaClient.cpp
)

target_link_libraries(e2e_tests
    PRIVATE
    ${ENGINE_CORE_LIB}
    gtest
    gtest_main
    pthread
)

# Compiler options
target_compile_options(e2e_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)

# Test discovery
include(GoogleTest)
gtest_discover_tests(e2e_tests)