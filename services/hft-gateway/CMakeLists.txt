cmake_minimum_required(VERSION 3.14)
project(hft-gateway)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Create mock dependencies
set(GENERATED_HEADERS_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Create minimal FlatBuffers placeholder
file(WRITE ${GENERATED_HEADERS_DIR}/messages_generated.h
     "// Placeholder for FlatBuffers schema\n"
     "#pragma once\n"
     "#include <string>\n"
     "namespace flatbuffers { \n"
     "class Verifier { public: Verifier(const void*, size_t) {} }; \n"
     "}\n"
     "namespace quasar { namespace schema { \n"
     "inline bool VerifyMessageBuffer(const flatbuffers::Verifier&) { return true; }\n"
     "inline const void* GetMessage(const void* data) { return data; }\n"
     "enum MessageType { MessageType_NewOrderRequest = 1 };\n"
     "struct Message { \n"
     "    int message_type_type() const { return MessageType_NewOrderRequest; } \n"
     "    const void* message_type_as_NewOrderRequest() const { return this; } \n"
     "};\n"
     "struct NewOrderRequest { \n"
     "    struct SymbolString { const char* c_str() const { return \"BTC-USD\"; } int size() const { return 7; } std::string str() const { return \"BTC-USD\"; } };\n"
     "    const SymbolString* symbol() const { static SymbolString s; return &s; } \n"
     "    double price() const { return 50000.0; } \n"
     "    uint64_t quantity() const { return 100; } \n"
     "};\n"
     "}}\n")

# Create mock Boost ASIO headers
set(BOOST_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/boost_mock")
file(MAKE_DIRECTORY ${BOOST_INCLUDE_DIR}/boost/asio)
file(MAKE_DIRECTORY ${BOOST_INCLUDE_DIR}/boost/asio/ip)

# Create minimal Boost ASIO mocks
file(WRITE ${BOOST_INCLUDE_DIR}/boost/asio.hpp
     "#pragma once\n"
     "#include \"asio/io_context.hpp\"\n"
     "#include \"asio/ip/tcp.hpp\"\n"
     "#include \"asio/signal_set.hpp\"\n"
     "#include \"asio/steady_timer.hpp\"\n"
     "#include \"asio/buffer.hpp\"\n"
     "#include \"asio/async_read.hpp\"\n"
     "#include \"asio/error.hpp\"\n")

file(WRITE ${BOOST_INCLUDE_DIR}/boost/asio/io_context.hpp
     "#pragma once\n"
     "#include \"../system/error_code.hpp\"\n"
     "namespace boost { namespace asio {\n"
     "class io_context {\n"
     "public:\n"
     "    void run() {}\n"
     "    void stop() {}\n"
     "};\n"
     "}}\n")

file(WRITE ${BOOST_INCLUDE_DIR}/boost/asio/ip/tcp.hpp
     "#pragma once\n"
     "#include \"../../system/error_code.hpp\"\n"
     "#include <string>\n"
     "#include <memory>\n"
     "namespace boost { namespace asio { namespace ip {\n"
     "struct address { static address from_string(const std::string&) { return {}; } std::string to_string() const { return \"127.0.0.1\"; } };\n"
     "struct endpoint { endpoint() {} endpoint(const address&, uint16_t) {} address address() const { return {}; } uint16_t port() const { return 8080; } int protocol() const { return 0; } };\n"
     "class tcp {\n"
     "public:\n"
     "    typedef endpoint endpoint;\n"
     "    class socket {\n"
     "    public:\n"
     "        socket(io_context&) {}\n"
     "        socket(socket&&) = default;\n"
     "        endpoint remote_endpoint() const { return {}; }\n"
     "        void close(boost::system::error_code&) {}\n"
     "        template<typename T> void set_option(T, boost::system::error_code&) {}\n"
     "    };\n"
     "    class acceptor {\n"
     "    public:\n"
     "        acceptor(io_context&) {}\n"
     "        void open(int) {}\n"
     "        template<typename T> void set_option(T) {}\n"
     "        void bind(const endpoint&) {}\n"
     "        void listen() {}\n"
     "        void close(boost::system::error_code&) {}\n"
     "        template<typename S, typename F> void async_accept(S&, F) {}\n"
     "    };\n"
     "    struct no_delay { no_delay(bool) {} };\n"
     "};\n"
     "}}}\n")

file(WRITE ${BOOST_INCLUDE_DIR}/boost/asio/signal_set.hpp
     "#pragma once\n"
     "#include \"../system/error_code.hpp\"\n"
     "namespace boost { namespace asio {\n"
     "class signal_set {\n"
     "public:\n"
     "    signal_set(io_context&, int, int) {}\n"
     "    template<typename F> void async_wait(F) {}\n"
     "};\n"
     "}}\n")

file(WRITE ${BOOST_INCLUDE_DIR}/boost/asio/steady_timer.hpp
     "#pragma once\n"
     "#include \"../system/error_code.hpp\"\n"
     "#include <chrono>\n"
     "namespace boost { namespace asio {\n"
     "class steady_timer {\n"
     "public:\n"
     "    steady_timer(io_context&) {}\n"
     "    void expires_after(std::chrono::seconds) {}\n"
     "    template<typename F> void async_wait(F) {}\n"
     "};\n"
     "}}\n")

file(WRITE ${BOOST_INCLUDE_DIR}/boost/asio/buffer.hpp
     "#pragma once\n"
     "#include <vector>\n"
     "namespace boost { namespace asio {\n"
     "template<typename T> auto buffer(T& t) { return &t; }\n"
     "}}\n")

file(WRITE ${BOOST_INCLUDE_DIR}/boost/asio/async_read.hpp
     "#pragma once\n"
     "#include \"../system/error_code.hpp\"\n"
     "namespace boost { namespace asio {\n"
     "template<typename S, typename B, typename F> void async_read(S&, B, F f) {\n"
     "    f(boost::system::error_code{}, 0);\n"
     "}\n"
     "}}\n")

file(WRITE ${BOOST_INCLUDE_DIR}/boost/asio/error.hpp
     "#pragma once\n"
     "#include \"../system/error_code.hpp\"\n"
     "namespace boost { namespace asio { namespace error {\n"
     "const boost::system::error_code eof{};\n"
     "const boost::system::error_code connection_reset{};\n"
     "const boost::system::error_code operation_aborted{};\n"
     "const boost::system::error_code invalid_argument{};\n"
     "}}}\n")

file(WRITE ${BOOST_INCLUDE_DIR}/boost/system/error_code.hpp
     "#pragma once\n"
     "#include <string>\n"
     "namespace boost { namespace system {\n"
     "class error_code {\n"
     "public:\n"
     "    error_code() = default;\n"
     "    operator bool() const { return false; }\n"
     "    std::string message() const { return \"no error\"; }\n"
     "};\n"
     "}}\n")

# Create mock spdlog
set(SPDLOG_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/spdlog_mock")
file(MAKE_DIRECTORY ${SPDLOG_INCLUDE_DIR}/spdlog)
file(MAKE_DIRECTORY ${SPDLOG_INCLUDE_DIR}/spdlog/sinks)

file(WRITE ${SPDLOG_INCLUDE_DIR}/spdlog/spdlog.h
     "#pragma once\n"
     "#include <memory>\n"
     "#include <string>\n"
     "#include <iostream>\n"
     "namespace spdlog {\n"
     "enum class level { debug, info, warn, error };\n"
     "class logger {\n"
     "public:\n"
     "    logger(const std::string&, std::initializer_list<std::shared_ptr<void>>) {}\n"
     "    template<typename... Args> void debug(const std::string& fmt, Args&&... args) { std::cout << \"[DEBUG] \" << fmt << std::endl; }\n"
     "    template<typename... Args> void info(const std::string& fmt, Args&&... args) { std::cout << \"[INFO] \" << fmt << std::endl; }\n"
     "    template<typename... Args> void warn(const std::string& fmt, Args&&... args) { std::cout << \"[WARN] \" << fmt << std::endl; }\n"
     "    template<typename... Args> void error(const std::string& fmt, Args&&... args) { std::cout << \"[ERROR] \" << fmt << std::endl; }\n"
     "    void set_level(level) {}\n"
     "};\n"
     "inline std::shared_ptr<logger> get(const std::string&) { return nullptr; }\n"
     "inline std::shared_ptr<logger> default_logger() { static auto l = std::make_shared<logger>(\"\", std::initializer_list<std::shared_ptr<void>>{}); return l; }\n"
     "inline void register_logger(std::shared_ptr<logger>) {}\n"
     "inline void set_default_logger(std::shared_ptr<logger>) {}\n"
     "template<typename... Args> void info(const std::string& fmt, Args&&... args) { std::cout << \"[INFO] \" << fmt << std::endl; }\n"
     "template<typename... Args> void error(const std::string& fmt, Args&&... args) { std::cout << \"[ERROR] \" << fmt << std::endl; }\n"
     "typedef std::initializer_list<std::shared_ptr<void>> sinks_init_list;\n"
     "}\n")

file(WRITE ${SPDLOG_INCLUDE_DIR}/spdlog/sinks/stdout_color_sinks.h
     "#pragma once\n"
     "#include \"../spdlog.h\"\n"
     "namespace spdlog { namespace sinks {\n"
     "class stdout_color_sink_mt {\n"
     "public:\n"
     "    void set_level(level) {}\n"
     "    void set_pattern(const std::string&) {}\n"
     "};\n"
     "}}\n")

file(WRITE ${SPDLOG_INCLUDE_DIR}/spdlog/sinks/rotating_file_sink.h
     "#pragma once\n"
     "#include \"../spdlog.h\"\n"
     "namespace spdlog { namespace sinks {\n"
     "class rotating_file_sink_mt {\n"
     "public:\n"
     "    rotating_file_sink_mt(const std::string&, size_t, size_t) {}\n"
     "    void set_level(level) {}\n"
     "    void set_pattern(const std::string&) {}\n"
     "};\n"
     "}}\n")

# Create Kafka client directory structure if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/kafka)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/kafka)

# Create Kafka client header if it doesn't exist
set(KAFKA_CLIENT_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/kafka/KafkaClient.h")
if(NOT EXISTS ${KAFKA_CLIENT_HEADER})
    file(WRITE ${KAFKA_CLIENT_HEADER}
         "#pragma once\n"
         "#include <string>\n"
         "#include <vector>\n"
         "#include <functional>\n"
         "namespace kafka {\n"
         "struct KafkaConfig {\n"
         "    std::string brokers;\n"
         "    std::string client_id;\n"
         "    std::string orders_new_topic;\n"
         "};\n"
         "class KafkaClient {\n"
         "public:\n"
         "    explicit KafkaClient(const KafkaConfig& config) : config_(config) {}\n"
         "    bool initialize() { return true; }\n"
         "    void shutdown() {}\n"
         "    bool produce_async(const std::string& topic, const std::string& key, const std::vector<uint8_t>& data) { return true; }\n"
         "    void set_error_callback(std::function<void(const std::string&, int, const std::string&)> cb) {}\n"
         "    void set_delivery_callback(std::function<void(const std::string&, int32_t, int64_t, const std::string&)> cb) {}\n"
         "private:\n"
         "    KafkaConfig config_;\n"
         "};\n"
         "}\n")
endif()

# Create Kafka client source if it doesn't exist
set(KAFKA_CLIENT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/kafka/KafkaClient.cpp")
if(NOT EXISTS ${KAFKA_CLIENT_SRC})
    file(WRITE ${KAFKA_CLIENT_SRC}
         "#include \"kafka/KafkaClient.h\"\n"
         "// Kafka client implementation placeholder\n")
endif()

# Include mock directories
include_directories(${BOOST_INCLUDE_DIR})
include_directories(${SPDLOG_INCLUDE_DIR})

# Add executable
add_executable(hft_gateway
    src/main.cpp
    src/HFTGateway.cpp
    src/kafka/KafkaClient.cpp
)

# Set output directory
set_target_properties(hft_gateway PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Create logs directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)

# Set compiler flags for better error reporting
target_compile_options(hft_gateway PRIVATE -Wall -Wextra)

# --- Tests ---
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "HFT Gateway configured with mock dependencies")
message(STATUS "This is a development build without real Kafka/Boost dependencies")