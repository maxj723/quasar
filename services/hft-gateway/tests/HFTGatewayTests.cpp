#include "gtest/gtest.h"
#include "HFTGateway.h"
#include <chrono>
#include <thread>

using namespace quasar::gateway;

class HFTGatewayTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.listen_address = "127.0.0.1";
        config_.listen_port = 0; // Let OS choose available port
        config_.kafka_brokers = "localhost:9092";
        config_.orders_topic = "test.orders";
        config_.client_id = "test-gateway";
    }

    GatewayConfig config_;
};

TEST_F(HFTGatewayTest, ConstructorInitializesCorrectly) {
    HFTGateway gateway(config_);

    // Test should complete without exceptions
    SUCCEED();
}

TEST_F(HFTGatewayTest, InitializeSuccess) {
    HFTGateway gateway(config_);

    bool result = gateway.initialize();
    EXPECT_TRUE(result);

    // Clean shutdown
    gateway.shutdown();
}

TEST_F(HFTGatewayTest, InitializeMultipleCalls) {
    HFTGateway gateway(config_);

    // First initialization should succeed
    bool result1 = gateway.initialize();
    EXPECT_TRUE(result1);

    // Second initialization should also succeed (idempotent)
    bool result2 = gateway.initialize();
    EXPECT_TRUE(result2);

    gateway.shutdown();
}

TEST_F(HFTGatewayTest, ShutdownWithoutInitialize) {
    HFTGateway gateway(config_);

    // Shutdown without initialize should not crash
    EXPECT_NO_THROW(gateway.shutdown());
}

TEST_F(HFTGatewayTest, MultipleShutdowns) {
    HFTGateway gateway(config_);
    gateway.initialize();

    // Multiple shutdowns should be safe
    EXPECT_NO_THROW(gateway.shutdown());
    EXPECT_NO_THROW(gateway.shutdown());
}

TEST_F(HFTGatewayTest, PublishOrderBeforeInitialize) {
    HFTGateway gateway(config_);

    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
    bool result = gateway.publish_order(test_data, "BTC-USD");

    // Should fail when not initialized
    EXPECT_FALSE(result);
}

TEST_F(HFTGatewayTest, PublishOrderAfterInitialize) {
    HFTGateway gateway(config_);
    gateway.initialize();

    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
    bool result = gateway.publish_order(test_data, "BTC-USD");

    // Should succeed when initialized (mock implementation)
    EXPECT_TRUE(result);

    gateway.shutdown();
}

TEST_F(HFTGatewayTest, PublishOrderEmptyData) {
    HFTGateway gateway(config_);
    gateway.initialize();

    std::vector<uint8_t> empty_data;
    bool result = gateway.publish_order(empty_data, "BTC-USD");

    // Should handle empty data gracefully
    EXPECT_TRUE(result); // Mock implementation always succeeds

    gateway.shutdown();
}

TEST_F(HFTGatewayTest, PublishOrderEmptyTradingPair) {
    HFTGateway gateway(config_);
    gateway.initialize();

    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
    bool result = gateway.publish_order(test_data, "");

    // Should handle empty trading pair (should use default)
    EXPECT_TRUE(result);

    gateway.shutdown();
}

TEST_F(HFTGatewayTest, StatisticsInitiallyZero) {
    HFTGateway gateway(config_);

    const auto& stats = gateway.get_statistics();

    EXPECT_EQ(stats.connections_accepted.load(), 0);
    EXPECT_EQ(stats.connections_active.load(), 0);
    EXPECT_EQ(stats.messages_received.load(), 0);
    EXPECT_EQ(stats.messages_published.load(), 0);
    EXPECT_EQ(stats.bytes_received.load(), 0);
    EXPECT_EQ(stats.bytes_published.load(), 0);
    EXPECT_EQ(stats.protocol_errors.load(), 0);
    EXPECT_EQ(stats.kafka_errors.load(), 0);
    EXPECT_EQ(stats.validation_errors.load(), 0);
}

TEST_F(HFTGatewayTest, StatisticsUpdateOnPublish) {
    HFTGateway gateway(config_);
    gateway.initialize();

    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    gateway.publish_order(test_data, "BTC-USD");

    // Give some time for async operations (if any)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const auto& stats = gateway.get_statistics();

    // In mock implementation, messages_published might be updated via callback
    // bytes_published should definitely be updated
    EXPECT_GE(stats.bytes_published.load(), test_data.size());

    gateway.shutdown();
}

TEST_F(HFTGatewayTest, ConfigurationAccess) {
    // Test that gateway preserves configuration
    config_.listen_port = 12345;
    config_.kafka_brokers = "test-broker:9092";
    config_.orders_topic = "test.topic";

    HFTGateway gateway(config_);

    // Gateway should initialize with the provided config
    EXPECT_TRUE(gateway.initialize());

    gateway.shutdown();
}

class HFTGatewayStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.listen_address = "127.0.0.1";
        config_.listen_port = 0;
        config_.kafka_brokers = "localhost:9092";
        config_.orders_topic = "stress.test.orders";
        config_.client_id = "stress-test-gateway";
    }

    GatewayConfig config_;
};

TEST_F(HFTGatewayStressTest, MultiplePublishesRapidly) {
    HFTGateway gateway(config_);
    gateway.initialize();

    const int num_messages = 100;
    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_messages; ++i) {
        bool result = gateway.publish_order(test_data, "BTC-USD");
        EXPECT_TRUE(result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete reasonably quickly (< 1 second for 100 messages)
    EXPECT_LT(duration.count(), 1000);

    // Verify statistics
    const auto& stats = gateway.get_statistics();
    EXPECT_GE(stats.bytes_published.load(), num_messages * test_data.size());

    gateway.shutdown();
}

TEST_F(HFTGatewayStressTest, ConcurrentPublishes) {
    HFTGateway gateway(config_);
    gateway.initialize();

    const int num_threads = 4;
    const int messages_per_thread = 25;
    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};

    std::vector<std::thread> threads;
    std::atomic<int> successful_publishes{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < messages_per_thread; ++i) {
                std::string trading_pair = "THREAD" + std::to_string(t) + "-USD";
                bool result = gateway.publish_order(test_data, trading_pair);
                if (result) {
                    successful_publishes.fetch_add(1);
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All publishes should succeed in mock implementation
    EXPECT_EQ(successful_publishes.load(), num_threads * messages_per_thread);

    gateway.shutdown();
}