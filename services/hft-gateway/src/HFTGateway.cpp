#include "HFTGateway.h"
#include "messages_generated.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <boost/asio/buffer.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
// #include <flatbuffers/flatbuffers.h> // Using mock implementation

namespace quasar {
namespace gateway {

// GatewayConfig Implementation
GatewayConfig GatewayConfig::from_environment() {
    GatewayConfig config;

    if (const char* address = std::getenv("LISTEN_ADDRESS")) {
        config.listen_address = address;
    }
    if (const char* port = std::getenv("LISTEN_PORT" )) {
        config.listen_port = static_cast<uint16_t>(std::stoi(port));
    }
    if (const char* brokers = std::getenv("KAFKA_BROKERS")) {
        config.kafka_brokers = brokers;
    }
    if (const char* topic = std::getenv("ORDERS_TOPIC")) {
        config.orders_topic = topic;
    }
    if (const char* client_id = std::getenv("KAFKA_CLIENT_ID")) {
        config.client_id = client_id;
    }

    return config;
}

GatewayConfig GatewayConfig::from_file(const std::string& config_file) {
    GatewayConfig config;
    std::ifstream file(config_file);

    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_file);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Trim whitespace and handle inline comments
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));

        // Remove inline comments from value
        auto comment_pos = value.find('#');
        if (comment_pos != std::string::npos) {
            value = value.substr(0, comment_pos);
        }

        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        if (key == "listen_address") config.listen_address = value;
        else if (key == "listen_port") config.listen_port = static_cast<uint16_t>(std::stoi(value));
        else if (key == "kafka_brokers") config.kafka_brokers = value;
        else if (key == "orders_topic") config.orders_topic = value;
        else if (key == "client_id") config.client_id = value;
    }

    return config;
}

// HFTGateway implementation
HFTGateway::HFTGateway(const GatewayConfig& config)
    : config_(config)
    , io_context_()
    , acceptor_(io_context_)
    , signals_(io_context_, SIGINT, SIGTERM)
    , stats_timer_(io_context_)
    , logger_(spdlog::get("gateway") ? spdlog::get("gateway") : spdlog::default_logger()) {

    // Create specialized logger for gateway if it doesn't exist
    if (!spdlog::get("gateway")) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%n] [%^%l%$] %v");

        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/hft_gateway.log", 1024*1024*10, 3);
        file_sink->set_level(spdlog::level::debug);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%n] [%l] [%t] %v");

        logger_ = std::make_shared<spdlog::logger>("gateway",
            spdlog::sinks_init_list{console_sink, file_sink});
        logger_->set_level(spdlog::level::debug);
        spdlog::register_logger(logger_);
    }

    logger_->info("HFT Gateway created with config:");
    logger_->info(" Listen: {}:{}", config_.listen_address, config_.listen_port);
    logger_->info(" Kafka: {}", config_.kafka_brokers);
    logger_->info(" Orders Topic: {}", config_.orders_topic);
}

HFTGateway::~HFTGateway() {
    shutdown();
}

bool HFTGateway::initialize() {
    logger_->info("Initializing HFT Gateway");

    try {
        // Setup Kafka configuration
        kafka_config_.brokers = config_.kafka_brokers;
        kafka_config_.client_id = config_.client_id;
        kafka_config_.orders_new_topic = config_.orders_topic;

        // Create and initialize Kafka client
        kafka_client_ = std::make_unique<kafka::KafkaClient>(kafka_config_);

        if (!kafka_client_->initialize()) {
            logger_->error("Failed to initialize Kafka client");
            return false;
        }

        // Setup Kafka callbacks
        kafka_client_->set_error_callback(
            [this](const std::string& operation, int error_code, const std::string& error_msg) {
                logger_->error("Kafka error in {}: {} ({})", operation, error_msg, error_code);
                stats_.kafka_errors.fetch_add(1);
            }
        );

        kafka_client_->set_delivery_callback(
            [this](const std::string& topic, int32_t partition, int64_t offset, const std::string& error) {
                if (!error.empty()) {
                    logger_->error("Message delivery failed to {}:{}: {}", topic, partition, error);
                    stats_.kafka_errors.fetch_add(1);
                } else {
                    logger_->debug("Message delivered to {}:{} at offset {}", topic, partition, offset);
                    stats_.messages_published.fetch_add(1);
                }
            }
        );

        // Setup TCP acceptor
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::address::from_string(config_.listen_address),
            config_.listen_port
        );

        acceptor_.open(endpoint.protocol());
        // acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true)); // Mock doesn't support this
        acceptor_.bind(endpoint);
        acceptor_.listen();

        logger_->info("TCP acceptor bound to {}:{}", config_.listen_address, config_.listen_port);

        // Setup signal handling
        handle_signals();

        initialized_.store(true);
        logger_->info("HFT Gateway initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logger_->error("Exception during initialization: {}", e.what());
        return false;
    }
}

void HFTGateway::run() {
    if (!initialized_.load()) {
        logger_->error("Gateway not initialized, cannot run");
        return;
    }

    logger_->info("Starting HFT Gateway");

    // Start accepting connections
    start_accept();

    // Start statistics timer
    log_statistics();

    // Run the io_context
    logger_->info("HFT Gateway running, accepting connections on port {}", config_.listen_port);
    io_context_.run();

    logger_->info("HFT Gateway stopped");
}

void HFTGateway::shutdown() {
    if (shutting_down_.exchange(true)) {
        return; // Already shutting down
    }

    logger_->info("Shutting down HFT Gateway");

    // Stop accepting new connections
    boost::system::error_code ec;
    acceptor_.close(ec);

    // Close all active sessions
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& session : active_sessions_) {
            session->stop();
        }
        active_sessions_.clear();
    }

    // Shutdown Kafka client
    if (kafka_client_) {
        kafka_client_->shutdown();
    }

    // Stop io_context
    io_context_.stop();

    logger_->info("HFT Gateway shutdown complete");
}

bool HFTGateway::publish_order(const std::vector<uint8_t>& serialized_order,
                              const std::string& trading_pair) {
    if (!kafka_client_) {
        logger_->error("Kafka client not available");
        return false;
    }

    // Use trading pair as message key for proper partitioning
    std::string key = trading_pair.empty() ? "DEFAULT" : trading_pair;

    bool success = kafka_client_->produce_async(
        config_.orders_topic, key, serialized_order);
    
    if (success) {
        stats_.bytes_published.fetch_add(serialized_order.size());
        logger_->debug("Order published to topic {} with key {}", config_.orders_topic, key);
    } else {
        stats_.kafka_errors.fetch_add(1);
        logger_->error("Failed to publish order to Kafka");
    }

    return success;
}

void HFTGateway::register_session(std::shared_ptr<ClientSession> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    active_sessions_.insert(session);
    stats_.connections_active.store(active_sessions_.size());
    logger_->debug("Registered session from {}, total active: {}",
                  session->get_remote_endpoint(), active_sessions_.size());
}

void HFTGateway::unregister_session(std::shared_ptr<ClientSession> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    active_sessions_.erase(session);
    stats_.connections_active.store(active_sessions_.size());
    logger_->debug("Unregistered session from {}, total active: {}",
                  session->get_remote_endpoint(), active_sessions_.size());
}

void HFTGateway::start_accept() {
    auto new_session = std::make_shared<ClientSession>(
        boost::asio::ip::tcp::socket(io_context_), this);

    acceptor_.async_accept(new_session->socket(),
        [this, new_session](boost::system::error_code ec) {
            if (!ec) {
                stats_.connections_accepted.fetch_add(1);
                logger_->info("New connection from {}", new_session->get_remote_endpoint());

                register_session(new_session);
                new_session->start();

                // Continue accepting connections
                start_accept();
            } else if (ec != boost::asio::error::operation_aborted) {
                logger_->error("Accept error: {}", ec.message());
                // Continue accepting even after errors
                start_accept();
            }
        });
}

void HFTGateway::handle_signals() {
    signals_.async_wait([this](boost::system::error_code ec, int signal_number) {
        if (!ec) {
            logger_->info("Received signal {}, shutting down", signal_number);
            shutdown();
        }
    });
}

void HFTGateway::log_statistics() {
    stats_timer_.expires_after(std::chrono::seconds(30));
    stats_timer_.async_wait([this](boost::system::error_code ec) {
        if (!ec && !shutting_down_.load()) {
            logger_->info("=== HFT GATEWAY STATISTICS ===");
            logger_->info("Connections: accepted={}, active={}",
                         stats_.connections_accepted.load(),
                         stats_.connections_active.load());
            logger_->info("Messages: received={}, published={}",
                         stats_.messages_received.load(),
                         stats_.messages_published.load());
            logger_->info("Bytes: received={}, published={}",
                         stats_.bytes_received.load(),
                         stats_.bytes_published.load());
            logger_->info("Errors: protocol={}, kafka={}, validation={}",
                         stats_.protocol_errors.load(),
                         stats_.kafka_errors.load(),
                         stats_.validation_errors.load());
            logger_->info("==============================");

            // Schedule next log
            log_statistics();
        }
    });
}

// ClientSession implementation
ClientSession::ClientSession(boost::asio::ip::tcp::socket socket, HFTGateway* gateway)
    : socket_(std::move(socket))
    , length_buffer_(4) // uint32_t for message length
    , gateway_(gateway)
    , logger_(spdlog::get("gateway")) {

    try {
        remote_endpoint_ = socket_.remote_endpoint().address().to_string() + ":" + 
                         std::to_string(socket_.remote_endpoint().port());
    } catch (const std::exception& e) {
        remote_endpoint_ = "unknown";
        logger_->warn("Failed to get remote endpoint: {}", e.what());
    }
}

ClientSession::~ClientSession() {
    stop();
}

void ClientSession::start() {
    active_.store(true);

    // Set socket options for performance
    boost::system::error_code ec;
    socket_.set_option(boost::asio::ip::tcp::no_delay(true), ec);
    if (ec) {
        logger_->warn("Failed to set TCP_NODELAY: {}", ec.message());
    }

    logger_->debug("Starting session for {}", remote_endpoint_);
    read_message_length();
}

void ClientSession::stop() {
    if (active_.exchange(false)) {
        logger_->debug("Stopping session for {}", remote_endpoint_);

        boost::system::error_code ec;
        socket_.close(ec);

        if (gateway_) {
            gateway_->unregister_session(shared_from_this());
        }
    }
}

std::string ClientSession::get_remote_endpoint() const {
    return remote_endpoint_;
}

void ClientSession::read_message_length() {
    if (!active_.load()) return;

    boost::asio::async_read(socket_,
        boost::asio::buffer(length_buffer_),
        [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec && bytes_transferred == 4) {
                // Convert from network byte order
                uint32_t message_length = 
                    (static_cast<uint32_t>(self->length_buffer_[0]) << 24) |
                    (static_cast<uint32_t>(self->length_buffer_[1]) << 16) |
                    (static_cast<uint32_t>(self->length_buffer_[2]) << 8) |
                    static_cast<uint32_t>(self->length_buffer_[3]);

                // Validate message length
                if (message_length > 0 && message_length <= 4096) {
                    self->read_message_body(message_length);
                } else {
                    self->logger_->error("Invalid message length {} from {}",
                                        message_length, self->remote_endpoint_);
                    self->gateway_->stats_.protocol_errors.fetch_add(1);
                    self->handle_error(boost::asio::error::invalid_argument);
                }
            } else {
                self->handle_error(ec);
            }
        });
}

void ClientSession::read_message_body(uint32_t length) {
    if (!active_.load()) return;

    message_buffer_.resize(length);
    current_message_length_ = length;

    boost::asio::async_read(socket_,
        boost::asio::buffer(message_buffer_),
        [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec && bytes_transferred == self->current_message_length_) {
                self->gateway_->stats_.messages_received.fetch_add(1);
                self->gateway_->stats_.bytes_received.fetch_add(bytes_transferred);

                self->handle_message(self->message_buffer_);

                // Continue reading next message
                self->read_message_length();
            } else {
                self->handle_error(ec);
            }
        });
}

void ClientSession::handle_message(const std::vector<uint8_t>& message) {
    logger_->debug("Received message of {} bytes from {}", message.size(), remote_endpoint_);

    // Validate the FlatBuffer message
    if (!validate_order_message(message)) {
        logger_->error("Invalid FlatBuffer message from {}", remote_endpoint_);
        gateway_->stats_.validation_errors.fetch_add(1);
        return;
    }

    // Extract trading pair from the message for partitioning
    std::string trading_pair = "DEFAULT";

    try {
        // Parse FlatBuffer to extract symbol
        auto verifier = flatbuffers::Verifier(message.data(), message.size());
        if (quasar::schema::VerifyMessageBuffer(verifier)) {
            auto fb_message = static_cast<const quasar::schema::Message*>(quasar::schema::GetMessage(message.data()));
            if (fb_message->message_type_type() == quasar::schema::MessageType_NewOrderRequest) {
                auto order_request = static_cast<const quasar::schema::NewOrderRequest*>(fb_message->message_type_as_NewOrderRequest());
                if (order_request && order_request->symbol()) {
                    trading_pair = order_request->symbol()->str();
                }
            }
        }
    } catch (const std::exception& e) {
        logger_->error("Exception parsing message for trading pair: {}", e.what());
        // Continue with default trading pair
    }

    // Publish to Kafka
    bool success = gateway_->publish_order(message, trading_pair);

    if (!success) {
        logger_->error("Failed to publish order from {} to Kafka", remote_endpoint_);
    }
}

void ClientSession::handle_error(const boost::system::error_code& error) {
    if (error == boost::asio::error::eof ||
        error == boost::asio::error::connection_reset) {
        logger_->info("Client {} disconnected", remote_endpoint_);
    } else if (error != boost::asio::error::operation_aborted) {
            logger_->error("Session error for {}: {}", remote_endpoint_, error.message());
    }

    stop();
}

bool ClientSession::validate_order_message(const std::vector<uint8_t>& message) {
    if (message.empty()) {
        return false;
    }

    try {
        // Verify FlatBuffer integrity
        flatbuffers::Verifier verifier(message.data(), message.size());
        if (!quasar::schema::VerifyMessageBuffer(verifier)) {
            return false;
        }

        // Parse and validate message content
        auto fb_message = quasar::schema::GetMessage(message.data());
        if (!fb_message) {
            return false;
        }

        // Check if it's a new order request
        auto fb_message_typed = static_cast<const quasar::schema::Message*>(fb_message);
        if (fb_message_typed->message_type_type() == quasar::schema::MessageType_NewOrderRequest) {
            auto order_request = static_cast<const quasar::schema::NewOrderRequest*>(fb_message_typed->message_type_as_NewOrderRequest());
            if (!order_request) {
                return false;
            }

            // Validate order fields
            if (order_request->price() <= 0.0 || order_request->quantity() == 0) {
                logger_->error("Invalid order: price={}, quantity={}",
                              order_request->price(), order_request->quantity());
                return false;
            }

            if (!order_request->symbol() || order_request->symbol()->size() == 0) {
                logger_->error("Invalid order: missing symbol");
                return false;
            }
        }

        return true;

    } catch (const std::exception& e) {
        logger_->error("Exception validating message: {}", e.what());
        return false;
    }
}

}} // namespace quasar::gateway