#include "gtest/gtest.h"
#include "HFTGateway.h"
#include <cstdlib>

using namespace quasar::gateway;

class GatewayConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear environment variables
        unsetenv("LISTEN_ADDRESS");
        unsetenv("LISTEN_PORT");
        unsetenv("KAFKA_BROKERS");
        unsetenv("ORDERS_TOPIC");
        unsetenv("KAFKA_CLIENT_ID");
    }
};

TEST_F(GatewayConfigTest, DefaultConfiguration) {
    GatewayConfig config;

    EXPECT_EQ(config.listen_address, "0.0.0.0");
    EXPECT_EQ(config.listen_port, 31337);
    EXPECT_EQ(config.kafka_brokers, "localhost:9092");
    EXPECT_EQ(config.orders_topic, "orders.new");
    EXPECT_EQ(config.client_id, "hft-gateway");
    EXPECT_EQ(config.tcp_no_delay, 1);
    EXPECT_EQ(config.socket_buffer_size, 65536);
    EXPECT_EQ(config.max_message_size, 4096);
}

TEST_F(GatewayConfigTest, FromEnvironment) {
    // Set environment variables
    setenv("LISTEN_ADDRESS", "127.0.0.1", 1);
    setenv("LISTEN_PORT", "8080", 1);
    setenv("KAFKA_BROKERS", "broker1:9092,broker2:9092", 1);
    setenv("ORDERS_TOPIC", "custom.orders", 1);
    setenv("KAFKA_CLIENT_ID", "test-gateway", 1);

    GatewayConfig config = GatewayConfig::from_environment();

    EXPECT_EQ(config.listen_address, "127.0.0.1");
    EXPECT_EQ(config.listen_port, 8080);
    EXPECT_EQ(config.kafka_brokers, "broker1:9092,broker2:9092");
    EXPECT_EQ(config.orders_topic, "custom.orders");
    EXPECT_EQ(config.client_id, "test-gateway");
}

TEST_F(GatewayConfigTest, FromEnvironmentPartial) {
    // Set only some environment variables
    setenv("LISTEN_PORT", "9999", 1);
    setenv("KAFKA_BROKERS", "test-broker:9092", 1);

    GatewayConfig config = GatewayConfig::from_environment();

    // Changed values
    EXPECT_EQ(config.listen_port, 9999);
    EXPECT_EQ(config.kafka_brokers, "test-broker:9092");

    // Default values
    EXPECT_EQ(config.listen_address, "0.0.0.0");
    EXPECT_EQ(config.orders_topic, "orders.new");
    EXPECT_EQ(config.client_id, "hft-gateway");
}

TEST_F(GatewayConfigTest, FromFileSuccess) {
    // Create a temporary config file
    std::string config_content =
        "# Test configuration file\n"
        "listen_address = 192.168.1.100\n"
        "listen_port = 12345\n"
        "kafka_brokers = kafka.test.com:9092\n"
        "orders_topic = test.orders.new\n"
        "client_id = file-test-gateway\n";

    std::ofstream config_file("/tmp/test_gateway_config.txt");
    config_file << config_content;
    config_file.close();

    GatewayConfig config = GatewayConfig::from_file("/tmp/test_gateway_config.txt");

    EXPECT_EQ(config.listen_address, "192.168.1.100");
    EXPECT_EQ(config.listen_port, 12345);
    EXPECT_EQ(config.kafka_brokers, "kafka.test.com:9092");
    EXPECT_EQ(config.orders_topic, "test.orders.new");
    EXPECT_EQ(config.client_id, "file-test-gateway");

    // Clean up
    std::remove("/tmp/test_gateway_config.txt");
}

TEST_F(GatewayConfigTest, FromFileWithComments) {
    std::string config_content =
        "# This is a comment\n"
        "listen_address = 10.0.0.1  # inline comment\n"
        "\n"
        "# Another comment\n"
        "listen_port = 5555\n"
        "kafka_brokers = localhost:9092\n";

    std::ofstream config_file("/tmp/test_gateway_config2.txt");
    config_file << config_content;
    config_file.close();

    GatewayConfig config = GatewayConfig::from_file("/tmp/test_gateway_config2.txt");

    EXPECT_EQ(config.listen_address, "10.0.0.1");
    EXPECT_EQ(config.listen_port, 5555);
    EXPECT_EQ(config.kafka_brokers, "localhost:9092");

    // Clean up
    std::remove("/tmp/test_gateway_config2.txt");
}

TEST_F(GatewayConfigTest, FromFileNotFound) {
    EXPECT_THROW(
        GatewayConfig::from_file("/nonexistent/file.txt"),
        std::runtime_error
    );
}

TEST_F(GatewayConfigTest, FromFilePartialConfig) {
    std::string config_content =
        "listen_port = 7777\n"
        "kafka_brokers = partial.broker:9092\n";

    std::ofstream config_file("/tmp/test_gateway_config3.txt");
    config_file << config_content;
    config_file.close();

    GatewayConfig config = GatewayConfig::from_file("/tmp/test_gateway_config3.txt");

    // Changed values
    EXPECT_EQ(config.listen_port, 7777);
    EXPECT_EQ(config.kafka_brokers, "partial.broker:9092");

    // Default values should remain
    EXPECT_EQ(config.listen_address, "0.0.0.0");
    EXPECT_EQ(config.orders_topic, "orders.new");
    EXPECT_EQ(config.client_id, "hft-gateway");

    // Clean up
    std::remove("/tmp/test_gateway_config3.txt");
}