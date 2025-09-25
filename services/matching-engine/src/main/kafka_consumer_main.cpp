#include "core/MatchingEngine.h"
#include "core/Trade.h"
#include "kafka/KafkaClient.h"
#include "messages_generated.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <signal.h>
#include <memory>
#include <fstream>
#include <random>
#include <algorithm>

using namespace quasar;

class MatchingEngineConsumer {
public:
    MatchingEngineConsumer(const kafka::KafkaConfig& kafka_config)
        : kafka_config_(kafka_config)
        , engine_(std::make_unique<MatchingEngine>())
        , running_(false) {

        // Set up trade callback to publish to market data topic
        engine_->set_trade_callback([this](const Trade& trade) {
            publish_trade(trade);
            stats_.total_trades.fetch_add(1);
        });
    }

    bool initialize() {
        std::cout << "Initializing Matching Engine Consumer..." << std::endl;

        // Initialize Kafka client
        kafka_client_ = std::make_unique<kafka::KafkaClient>(kafka_config_);
        if (!kafka_client_->initialize()) {
            std::cerr << "Failed to initialize Kafka client" << std::endl;
            return false;
        }

        // Set up Kafka callbacks
        kafka_client_->set_error_callback([this](const std::string& operation, int error_code, const std::string& error_msg) {
            std::cerr << "Kafka error in " << operation << ": " << error_msg << " (" << error_code << ")" << std::endl;
            stats_.kafka_errors.fetch_add(1);
        });

        kafka_client_->set_delivery_callback([this](const std::string& topic, int32_t partition, int64_t offset, const std::string& error) {
            if (!error.empty()) {
                std::cerr << "Message delivery failed to " << topic << ":" << partition << ": " << error << std::endl;
                stats_.delivery_errors.fetch_add(1);
            } else {
                stats_.messages_published.fetch_add(1);
            }
        });

        std::cout << "Matching Engine Consumer initialized successfully" << std::endl;
        return true;
    }

    void run() {
        if (!initialize()) {
            return;
        }

        std::cout << "Starting Matching Engine Consumer" << std::endl;
        running_ = true;

        // Start statistics thread
        std::thread stats_thread(&MatchingEngineConsumer::print_stats, this);

        // Main message processing loop (mock implementation)
        while (running_) {
            // In real implementation, this would consume from Kafka
            // For now, simulate message processing
            process_mock_messages();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Wait for stats thread
        if (stats_thread.joinable()) {
            stats_thread.join();
        }

        // Shutdown
        if (kafka_client_) {
            kafka_client_->shutdown();
        }

        std::cout << "Matching Engine Consumer stopped" << std::endl;
    }

    void stop() {
        running_ = false;
    }

    struct Statistics {
        std::atomic<uint64_t> orders_processed{0};
        std::atomic<uint64_t> total_trades{0};
        std::atomic<uint64_t> messages_published{0};
        std::atomic<uint64_t> kafka_errors{0};
        std::atomic<uint64_t> delivery_errors{0};
        std::atomic<uint64_t> validation_errors{0};
    };

    const Statistics& get_stats() const { return stats_; }

private:
    void process_mock_messages() {
        // Simulate processing incoming orders
        static uint64_t client_id = 1;
        static std::vector<std::string> symbols = {"BTC-USD", "ETH-USD", "SOL-USD"};
        static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

        std::uniform_int_distribution<size_t> symbol_dist(0, symbols.size() - 1);
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_real_distribution<double> price_dist(40000.0, 60000.0);
        std::uniform_int_distribution<uint64_t> quantity_dist(1, 100);

        // Process a few orders
        for (int i = 0; i < 3; ++i) {
            std::string symbol = symbols[symbol_dist(rng)];
            Side side = (side_dist(rng) == 0) ? Side::BUY : Side::SELL;
            double price = price_dist(rng);
            uint64_t quantity = quantity_dist(rng);

            uint64_t order_id = engine_->submit_order(client_id++, symbol, side, price, quantity);
            stats_.orders_processed.fetch_add(1);
        }
    }

    void publish_trade(const Trade& trade) {
        if (!kafka_client_) return;

        // Create FlatBuffer for trade
        flatbuffers::FlatBufferBuilder builder(1024);

        auto symbol_str = builder.CreateString(trade.symbol);

        // Create trade message (simplified)
        auto trade_data = builder.CreateString(
            "trade_id=" + std::to_string(trade.trade_id) +
            ",symbol=" + trade.symbol +
            ",price=" + std::to_string(trade.price) +
            ",quantity=" + std::to_string(trade.quantity)
        );

        builder.Finish(trade_data);

        std::vector<uint8_t> data(builder.GetBufferPointer(),
                                  builder.GetBufferPointer() + builder.GetSize());

        // Publish to market data topic
        kafka_client_->produce_async(kafka_config_.trades_topic, trade.symbol, data);
    }

    void print_stats() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(10));

            std::cout << "\n=== MATCHING ENGINE STATISTICS ===" << std::endl;
            std::cout << "Orders Processed: " << stats_.orders_processed.load() << std::endl;
            std::cout << "Total Trades: " << stats_.total_trades.load() << std::endl;
            std::cout << "Messages Published: " << stats_.messages_published.load() << std::endl;
            std::cout << "Kafka Errors: " << stats_.kafka_errors.load() << std::endl;
            std::cout << "Delivery Errors: " << stats_.delivery_errors.load() << std::endl;

            auto engine_stats = engine_->get_stats();
            std::cout << "Engine Active Orders: " << engine_stats.active_orders << std::endl;
            std::cout << "Engine Total Trades: " << engine_stats.total_trades << std::endl;
            std::cout << "===================================" << std::endl;
        }
    }

    kafka::KafkaConfig kafka_config_;
    std::unique_ptr<kafka::KafkaClient> kafka_client_;
    std::unique_ptr<MatchingEngine> engine_;
    std::atomic<bool> running_;
    Statistics stats_;
};

// Global consumer for signal handling
std::unique_ptr<MatchingEngineConsumer> g_consumer;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    if (g_consumer) {
        g_consumer->stop();
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        // Configure Kafka
        kafka::KafkaConfig kafka_config;
        kafka_config.brokers = "localhost:9092";
        kafka_config.client_id = "matching-engine-consumer";
        kafka_config.orders_new_topic = "orders.new";
        kafka_config.trades_topic = "trades";

        // Override with command line arguments or environment variables
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--brokers" && i + 1 < argc) {
                kafka_config.brokers = argv[++i];
            } else if (arg == "--orders-topic" && i + 1 < argc) {
                kafka_config.orders_new_topic = argv[++i];
            } else if (arg == "--trades-topic" && i + 1 < argc) {
                kafka_config.trades_topic = argv[++i];
            }
        }

        std::cout << "Quasar Matching Engine Kafka Consumer" << std::endl;
        std::cout << "====================================" << std::endl;
        std::cout << "Kafka Brokers: " << kafka_config.brokers << std::endl;
        std::cout << "Orders Topic: " << kafka_config.orders_new_topic << std::endl;
        std::cout << "Trades Topic: " << kafka_config.trades_topic << std::endl;
        std::cout << "====================================" << std::endl;

        // Create and run consumer
        g_consumer = std::make_unique<MatchingEngineConsumer>(kafka_config);
        g_consumer->run();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error" << std::endl;
        return 1;
    }
}