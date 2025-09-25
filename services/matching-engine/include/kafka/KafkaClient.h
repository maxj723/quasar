#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

namespace kafka {

struct KafkaConfig {
    std::string brokers{"localhost:9092"};
    std::string client_id{"quasar-client"};
    std::string orders_new_topic{"orders.new"};
    std::string orders_cancel_topic{"orders.cancel"};
    std::string trades_topic{"trades"};
    std::string market_data_topic{"market_data"};

    // Performance settings
    int32_t batch_size{16384};
    int32_t linger_ms{5};
    int32_t queue_buffering_max_messages{100000};
    std::string compression_type{"snappy"};
};

class KafkaClient {
public:
    explicit KafkaClient(const KafkaConfig& config);
    ~KafkaClient();

    // Non-copyable, non-movable
    KafkaClient(const KafkaClient&) = delete;
    KafkaClient& operator=(const KafkaClient&) = delete;
    KafkaClient(KafkaClient&&) = delete;
    KafkaClient& operator=(KafkaClient&&) = delete;

    /**
     * Initialize the Kafka client
     */
    bool initialize();

    /**
     * Shutdown the client gracefully
     */
    void shutdown();

    /**
     * Produce a message asynchronously
     */
    bool produce_async(const std::string& topic, const std::string& key,
                      const std::vector<uint8_t>& payload);

    /**
     * Produce a message asynchronously (string payload)
     */
    bool produce_async(const std::string& topic, const std::string& key,
                      const std::string& payload);

    /**
     * Set error callback
     */
    void set_error_callback(std::function<void(const std::string& operation,
                                              int error_code,
                                              const std::string& error_msg)> callback);

    /**
     * Set delivery callback
     */
    void set_delivery_callback(std::function<void(const std::string& topic,
                                                 int32_t partition,
                                                 int64_t offset,
                                                 const std::string& error)> callback);

    /**
     * Get client statistics
     */
    struct Statistics {
        std::atomic<uint64_t> messages_produced{0};
        std::atomic<uint64_t> messages_failed{0};
        std::atomic<uint64_t> bytes_produced{0};
        std::atomic<uint64_t> errors{0};
    };

    const Statistics& get_statistics() const { return stats_; }

    /**
     * Flush any pending messages
     */
    void flush(int timeout_ms = 5000);

private:
    void poll_events();
    void handle_delivery_report();

    KafkaConfig config_;

    // Callbacks
    std::function<void(const std::string&, int, const std::string&)> error_callback_;
    std::function<void(const std::string&, int32_t, int64_t, const std::string&)> delivery_callback_;

    // Statistics
    mutable Statistics stats_;

    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};

    // Background thread for polling
    std::unique_ptr<std::thread> poll_thread_;
    mutable std::mutex callback_mutex_;

    // Mock producer implementation (for now)
    void* producer_{nullptr};
};

} // namespace kafka