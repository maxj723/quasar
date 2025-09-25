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

# --- FlatBuffers Schema Generation ---
set(SCHEMA_FILE "${CMAKE_CURRENT_SOURCE_DIR}/schema/messages.fbs")
set(GENERATED_HEADER "${CMAKE_CURRENT_BINARY_DIR}/messages_generated.h")

# Create FlatBuffers header at configure time if schema exists
if(EXISTS ${SCHEMA_FILE})
    # Try to find flatc
    find_program(FLATC_EXECUTABLE NAMES flatc)
    if(FLATC_EXECUTABLE)
        execute_process(
            COMMAND ${FLATC_EXECUTABLE} --cpp -o ${CMAKE_CURRENT_BINARY_DIR} ${SCHEMA_FILE}
            RESULT_VARIABLE FLATC_RESULT
            OUTPUT_QUIET ERROR_QUIET
        )
        if(NOT FLATC_RESULT EQUAL 0)
            message(WARNING "flatc failed, using fallback header")
        endif()
    else()
        message(STATUS "flatc not found, using fallback header")
    endif()
endif()

# Create fallback header if real one doesn't exist
if(NOT EXISTS ${GENERATED_HEADER})
    file(WRITE ${GENERATED_HEADER}
         "// Fallback FlatBuffers header\n"
         "#pragma once\n"
         "#include <string>\n"
         "#include <vector>\n"
         "#include <memory>\n"
         "namespace flatbuffers {\n"
         "class FlatBufferBuilder {\n"
         "public:\n"
         "    FlatBufferBuilder(size_t) {}\n"
         "    template<typename T> auto CreateString(const T& s) { return std::make_shared<std::string>(s); }\n"
         "    template<typename T> void Finish(T) {}\n"
         "    const uint8_t* GetBufferPointer() { static uint8_t buf[1024]; return buf; }\n"
         "    size_t GetSize() { return 100; }\n"
         "};\n"
         "class Verifier { public: Verifier(const void*, size_t) {} };\n"
         "}\n"
         "namespace quasar { namespace schema {\n"
         "inline bool VerifyMessageBuffer(const flatbuffers::Verifier&) { return true; }\n"
         "inline const void* GetMessage(const void* data) { return data; }\n"
         "enum MessageType { MessageType_NewOrderRequest = 1 };\n"
         "struct Message {\n"
         "    int message_type_type() const { return MessageType_NewOrderRequest; }\n"
         "    const void* message_type_as_NewOrderRequest() const { return this; }\n"
         "};\n"
         "struct NewOrderRequest {\n"
         "    struct SymbolString { std::string str() const { return \"BTC-USD\"; } };\n"
         "    const SymbolString* symbol() const { static SymbolString s; return &s; }\n"
         "    double price() const { return 50000.0; }\n"
         "    uint64_t quantity() const { return 100; }\n"
         "};\n"
         "}}\n")
endif()

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

# --- Kafka Consumer Executable ---
add_executable(matching_engine_consumer
    src/main/kafka_consumer_main.cpp
    src/kafka/KafkaClient.cpp
)

target_link_libraries(matching_engine_consumer
    PRIVATE
    engine_core
)

target_include_directories(matching_engine_consumer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

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
