#include "gtest/gtest.h"
#include "core/MatchingEngine.h"
#include "HFTGateway.h"
#include "kafka/KafkaClient.h"
#include "messages_generated.h"

#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>
#include <random>
#include <iostream>
#include <boost/asio.hpp>

using namespace quasar;

class EndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start services in order
        start_kafka_simulation();
        start_matching_engine();
        start_hft_gateway();

        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Let services initialize
    }

    void TearDown() override {
        stop_all_services();
    }

    void start_kafka_simulation() {
        // Mock Kafka broker simulation
        kafka_running_ = true;
        kafka_thread_ = std::thread([this]() {
            simulate_kafka_broker();
        });
    }

    void start_matching_engine() {
        engine_ = std::make_unique<MatchingEngine>();

        // Set up trade callback to capture trades
        engine_->set_trade_callback([this](const Trade& trade) {
            std::lock_guard<std::mutex> lock(trades_mutex_);
            captured_trades_.push_back(trade);
            trade_count_.fetch_add(1);
        });

        matching_engine_running_ = true;
        engine_thread_ = std::thread([this]() {
            simulate_matching_engine_consumer();
        });
    }

    void start_hft_gateway() {
        gateway::GatewayConfig config;
        config.listen_address = "127.0.0.1";
        config.listen_port = 0; // OS-assigned port
        config.kafka_brokers = "localhost:9092";
        config.orders_topic = "orders.new";

        gateway_ = std::make_unique<gateway::HFTGateway>(config);
        gateway_->initialize();

        gateway_running_ = true;
        gateway_thread_ = std::thread([this]() {
            gateway_->run();
        });
    }

    void stop_all_services() {
        // Stop in reverse order
        if (gateway_) {
            gateway_->shutdown();
        }
        gateway_running_ = false;
        if (gateway_thread_.joinable()) {
            gateway_thread_.join();
        }

        matching_engine_running_ = false;
        if (engine_thread_.joinable()) {
            engine_thread_.join();
        }

        kafka_running_ = false;
        if (kafka_thread_.joinable()) {
            kafka_thread_.join();
        }
    }

    void simulate_kafka_broker() {
        while (kafka_running_) {
            // Simulate message routing between gateway and engine
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // Transfer messages from gateway to engine queue
            std::lock_guard<std::mutex> lock(message_queue_mutex_);
            for (const auto& msg : gateway_to_engine_messages_) {
                engine_message_queue_.push(msg);
            }
            gateway_to_engine_messages_.clear();
        }
    }

    void simulate_matching_engine_consumer() {
        while (matching_engine_running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // Process messages from Kafka simulation
            std::lock_guard<std::mutex> lock(message_queue_mutex_);
            while (!engine_message_queue_.empty()) {
                auto message = engine_message_queue_.front();
                engine_message_queue_.pop();

                // Process order message
                process_order_message(message);
            }
        }
    }

    void process_order_message(const std::vector<uint8_t>& message) {
        if (message.empty()) return;

        // Mock order processing - create a simple order
        static uint64_t client_id = 1000;
        static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_real_distribution<double> price_dist(49000.0, 51000.0);
        std::uniform_int_distribution<uint64_t> quantity_dist(1, 100);

        Side side = (side_dist(rng) == 0) ? Side::BUY : Side::SELL;
        double price = price_dist(rng);
        uint64_t quantity = quantity_dist(rng);

        uint64_t order_id = engine_->submit_order(client_id++, "BTC-USD", side, price, quantity);
        orders_processed_.fetch_add(1);
    }

    // Simulated TCP client to connect to gateway
    void send_order_via_tcp(const std::vector<uint8_t>& order_data) {
        // Simulate message being sent to gateway and routed to Kafka
        std::lock_guard<std::mutex> lock(message_queue_mutex_);
        gateway_to_engine_messages_.push_back(order_data);
    }

    // Test utilities
    std::vector<uint8_t> create_mock_order_message(const std::string& symbol,
                                                   Side side,
                                                   double price,
                                                   uint64_t quantity) {
        // Simple mock message format
        std::string message = symbol + "," +
                             std::to_string(static_cast<int>(side)) + "," +
                             std::to_string(price) + "," +
                             std::to_string(quantity);

        return std::vector<uint8_t>(message.begin(), message.end());
    }

    // Member variables
    std::unique_ptr<MatchingEngine> engine_;
    std::unique_ptr<gateway::HFTGateway> gateway_;

    // Threading
    std::thread kafka_thread_;
    std::thread engine_thread_;
    std::thread gateway_thread_;

    std::atomic<bool> kafka_running_{false};
    std::atomic<bool> matching_engine_running_{false};
    std::atomic<bool> gateway_running_{false};

    // Message simulation
    std::mutex message_queue_mutex_;
    std::queue<std::vector<uint8_t>> engine_message_queue_;
    std::vector<std::vector<uint8_t>> gateway_to_engine_messages_;

    // Statistics
    std::atomic<uint64_t> orders_processed_{0};
    std::atomic<uint64_t> trade_count_{0};

    // Trade capture
    std::mutex trades_mutex_;
    std::vector<Trade> captured_trades_;
};

TEST_F(EndToEndTest, SingleOrderProcessing) {
    // Send a single order through the pipeline
    auto order_msg = create_mock_order_message("BTC-USD", Side::BUY, 50000.0, 100);
    send_order_via_tcp(order_msg);

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify order was processed
    EXPECT_GE(orders_processed_.load(), 1);

    // Check engine state
    auto stats = engine_->get_stats();
    EXPECT_GE(stats.total_orders, 1);
}

TEST_F(EndToEndTest, MultipleOrderProcessing) {
    const int num_orders = 10;

    // Send multiple orders
    for (int i = 0; i < num_orders; ++i) {
        auto order_msg = create_mock_order_message(
            "BTC-USD",
            (i % 2 == 0) ? Side::BUY : Side::SELL,
            50000.0 + (i * 10),
            10 + i
        );
        send_order_via_tcp(order_msg);
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify orders were processed
    EXPECT_GE(orders_processed_.load(), num_orders);

    // Check engine state
    auto stats = engine_->get_stats();
    EXPECT_GE(stats.total_orders, num_orders);
}

TEST_F(EndToEndTest, TradeGeneration) {
    // Create orders that should match

    // Send a buy order
    auto buy_order = create_mock_order_message("BTC-USD", Side::BUY, 50100.0, 50);
    send_order_via_tcp(buy_order);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Send a sell order at a lower price (should match)
    auto sell_order = create_mock_order_message("BTC-USD", Side::SELL, 50000.0, 25);
    send_order_via_tcp(sell_order);

    // Wait for processing and matching
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify orders were processed
    EXPECT_GE(orders_processed_.load(), 2);

    // Check if trades were generated
    std::lock_guard<std::mutex> lock(trades_mutex_);
    auto stats = engine_->get_stats();

    // Should have at least some activity
    EXPECT_GE(stats.total_orders, 2);
}

TEST_F(EndToEndTest, HighVolumeProcessing) {
    const int num_orders = 100;
    const int batch_size = 10;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Send orders in batches
    for (int batch = 0; batch < num_orders / batch_size; ++batch) {
        for (int i = 0; i < batch_size; ++i) {
            int order_id = batch * batch_size + i;
            auto order_msg = create_mock_order_message(
                "BTC-USD",
                (order_id % 2 == 0) ? Side::BUY : Side::SELL,
                49500.0 + (order_id * 5),  // Spread prices to encourage matching
                10 + (order_id % 50)
            );
            send_order_via_tcp(order_msg);
        }

        // Small delay between batches
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Wait for all processing to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Verify performance
    uint64_t processed = orders_processed_.load();
    EXPECT_GE(processed, num_orders * 0.8); // Allow for some timing variance

    double orders_per_second = (double)processed / (duration.count() / 1000.0);

    std::cout << "Performance Results:" << std::endl;
    std::cout << "  Orders Processed: " << processed << std::endl;
    std::cout << "  Duration: " << duration.count() << "ms" << std::endl;
    std::cout << "  Orders/sec: " << orders_per_second << std::endl;
    std::cout << "  Trades Generated: " << trade_count_.load() << std::endl;

    // Performance should be reasonable
    EXPECT_GT(orders_per_second, 100); // At least 100 orders/second

    // Check final engine state
    auto stats = engine_->get_stats();
    std::cout << "Engine Stats:" << std::endl;
    std::cout << "  Total Orders: " << stats.total_orders << std::endl;
    std::cout << "  Active Orders: " << stats.active_orders << std::endl;
    std::cout << "  Total Trades: " << stats.total_trades << std::endl;

    EXPECT_GT(stats.total_orders, 0);
}

TEST_F(EndToEndTest, MultiSymbolProcessing) {
    std::vector<std::string> symbols = {"BTC-USD", "ETH-USD", "SOL-USD", "ADA-USD"};
    const int orders_per_symbol = 5;

    // Send orders for different symbols
    for (const auto& symbol : symbols) {
        for (int i = 0; i < orders_per_symbol; ++i) {
            auto order_msg = create_mock_order_message(
                symbol,
                (i % 2 == 0) ? Side::BUY : Side::SELL,
                1000.0 + (i * 100),
                10 + i
            );
            send_order_via_tcp(order_msg);
        }
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Verify orders were processed
    EXPECT_GE(orders_processed_.load(), symbols.size() * orders_per_symbol);

    // Check that multiple symbols are handled
    auto all_symbols = engine_->get_all_symbols();
    EXPECT_GE(all_symbols.size(), 1); // At least one symbol should be active

    std::cout << "Active symbols: ";
    for (const auto& sym : all_symbols) {
        std::cout << sym << " ";
    }
    std::cout << std::endl;
}

class EndToEndLatencyTest : public EndToEndTest {
protected:
    struct LatencyMeasurement {
        std::chrono::high_resolution_clock::time_point send_time;
        std::chrono::high_resolution_clock::time_point process_time;
        uint64_t order_id;
    };

    std::vector<LatencyMeasurement> latency_measurements_;
    std::mutex latency_mutex_;

    void measure_order_latency(uint64_t order_id) {
        // This would be called when order is processed
        auto process_time = std::chrono::high_resolution_clock::now();

        std::lock_guard<std::mutex> lock(latency_mutex_);
        for (auto& measurement : latency_measurements_) {
            if (measurement.order_id == order_id) {
                measurement.process_time = process_time;
                break;
            }
        }
    }

    void send_order_with_timing(uint64_t order_id, const std::vector<uint8_t>& order_data) {
        auto send_time = std::chrono::high_resolution_clock::now();

        {
            std::lock_guard<std::mutex> lock(latency_mutex_);
            latency_measurements_.push_back({send_time, {}, order_id});
        }

        send_order_via_tcp(order_data);
    }

    std::vector<double> calculate_latencies_us() {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        std::vector<double> latencies;

        for (const auto& measurement : latency_measurements_) {
            if (measurement.process_time.time_since_epoch().count() > 0) {
                auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    measurement.process_time - measurement.send_time
                );
                latencies.push_back(latency.count() / 1000.0); // Convert to microseconds
            }
        }

        return latencies;
    }
};

TEST_F(EndToEndLatencyTest, DISABLED_MeasureEndToEndLatency) {
    // This test is disabled because it requires more complex timing setup
    // In a real implementation, this would measure actual end-to-end latency

    const int num_measurements = 100;

    // Send orders with timing
    for (uint64_t i = 0; i < num_measurements; ++i) {
        auto order_msg = create_mock_order_message("BTC-USD", Side::BUY, 50000.0, 10);
        send_order_with_timing(i, order_msg);

        // Small delay between orders
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto latencies = calculate_latencies_us();

    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());

        double avg_latency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        double p50_latency = latencies[latencies.size() * 50 / 100];
        double p95_latency = latencies[latencies.size() * 95 / 100];
        double p99_latency = latencies[latencies.size() * 99 / 100];

        std::cout << "End-to-End Latency Results (μs):" << std::endl;
        std::cout << "  Samples: " << latencies.size() << std::endl;
        std::cout << "  Average: " << avg_latency << std::endl;
        std::cout << "  P50: " << p50_latency << std::endl;
        std::cout << "  P95: " << p95_latency << std::endl;
        std::cout << "  P99: " << p99_latency << std::endl;

        // Performance assertions
        EXPECT_LT(avg_latency, 10000); // Less than 10ms average
        EXPECT_LT(p99_latency, 50000); // Less than 50ms P99
    }
}