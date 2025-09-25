#pragma once
#include <string>
#include <vector>
namespace kafka {
struct KafkaConfig { std::string brokers; std::string orders_new_topic; };
class KafkaClient {
public:
    KafkaClient(const KafkaConfig&) {}
    bool initialize() { return true; }
    void shutdown() {}
};
}
