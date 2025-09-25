#pragma once

#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <unordered_set>
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include "kafka/KafkaClient.h"

namespace quasar {
namespace gateway {

struct GatewayConfig {
    // Network configuration
    std::string listen_address{"0.0.0.0"};
    uint16_t listen_port{31337};

    // Kafka configuration
    std::string kafka_brokers{"localhost:9092"};
    std::string orders_topic{"orders.new"};
    std::string client_id{"hft-gateway"};

    // Performance tuning
    int32_t tcp_no_delay{1};
    int32_t socket_buffer_size{65536};
    size_t max_message_size{4096};

    // Load from environment variables
    static GatewayConfig from_environment();
    static GatewayConfig from_file(const std::string& config_file);
};

class ClientSession;

/**
 * High-performance TCP gateway for HFT clients
 * Accepts binary orders and publishes them to Kafka pipeline
 */
class HFTGateway {
public:
    explicit HFTGateway(const GatewayConfig& config);
    ~HFTGateway();

    // Non-copyable, non-movable
    HFTGateway(const HFTGateway&) = delete;
    HFTGateway& operator=(const HFTGateway&) = delete;
    HFTGateway(HFTGateway&&) = delete;
    HFTGateway& operator=(HFTGateway&&) = delete;

    /**
     * Initialize the gateway (setup Kafka and network)
     */
    bool initialize();

    /**
     * Start the gateway and begin accepting connections
     */
    void run();

    /**
     * Shutdown the gateway gracefully
     */
    void shutdown();

    /**
     * Get current statistics
     */
    struct Statistics {
        std::atomic<uint64_t> connections_accepted{0};
        std::atomic<uint64_t> connections_active{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> messages_published{0};
        std::atomic<uint64_t> bytes_received{0};
        std::atomic<uint64_t> bytes_published{0};
        std::atomic<uint64_t> protocol_errors{0};
        std::atomic<uint64_t> kafka_errors{0};
        std::atomic<uint64_t> validation_errors{0};
    };

    const Statistics& get_statistics() const { return stats_; }

    /**
     * Publish order to Kafka (called by ClientSession)
     */
    bool publish_order(const std::vector<uint8_t>& serialized_order,
                      const std::string& trading_pair);

    /**
     * Register/unregister client sessions
     */
    void register_session(std::shared_ptr<ClientSession> session);
    void unregister_session(std::shared_ptr<ClientSession> session);

private:
    void start_accept();
    void handle_signals();
    void log_statistics();

    // Configuration
    GatewayConfig config_;

    // Boost.Asio components
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::signal_set signals_;
    boost::asio::steady_timer stats_timer_;

    // Kafka client
    std::unique_ptr<kafka::KafkaClient> kafka_client_;
    kafka::KafkaConfig kafka_config_;

    // Session management
    std::unordered_set<std::shared_ptr<ClientSession>> active_sessions_;
    std::mutex sessions_mutex_;

    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};

public:
    // Statistics - public for ClientSession access
    mutable Statistics stats_;

private:
    // Logger
    std::shared_ptr<spdlog::logger> logger_;
};

/**
 * Represents a singleclient connection
 * Handles message framing and deserialization
 */
class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    explicit ClientSession(boost::asio::ip::tcp::socket socket,
                          HFTGateway* gateway);
    ~ClientSession();

    void start();
    void stop();

    std::string get_remote_endpoint() const;

    // Public socket access for HFTGateway
    boost::asio::ip::tcp::socket& socket() { return socket_; }

private:
    void read_message_length();
    void read_message_body(uint32_t length);
    void handle_message(const std::vector<uint8_t>& message);
    void handle_error(const boost::system::error_code& error);
    bool validate_order_message(const std::vector<uint8_t>& message);

    // Network
    boost::asio::ip::tcp::socket socket_;
    std::string remote_endpoint_;

    // Message framing
    uint32_t current_message_length_{0};
    std::vector<uint8_t> length_buffer_;
    std::vector<uint8_t> message_buffer_;

    // Gateway reference
    HFTGateway* gateway_;

    // State
    std::atomic<bool> active_{false};

    // Logger
    std::shared_ptr<spdlog::logger> logger_;
};

}} // namespace quasar::gateway