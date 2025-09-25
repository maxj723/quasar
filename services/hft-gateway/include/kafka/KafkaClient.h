#pragma once
#include <string>
#include <vector>
#include <functional>
namespace kafka {
struct KafkaConfig {
    std::string brokers;
    std::string client_id;
    std::string orders_new_topic;
};
class KafkaClient {
public:
    explicit KafkaClient(const KafkaConfig& config) : config_(config) {}
    bool initialize() { return true; }
    void shutdown() {}
    bool produce_async(const std::string& topic, const std::string& key, const std::vector<uint8_t>& data) { return true; }
    void set_error_callback(std::function<void(const std::string&, int, const std::string&)> cb) {}
    void set_delivery_callback(std::function<void(const std::string&, int32_t, int64_t, const std::string&)> cb) {}
private:
    KafkaConfig config_;
};
}
