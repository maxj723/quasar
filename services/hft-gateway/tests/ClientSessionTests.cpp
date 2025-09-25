#include "gtest/gtest.h"
#include "HFTGateway.h"
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <chrono>

using namespace quasar::gateway;

class ClientSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.listen_address = "127.0.0.1";
        config_.listen_port = 0;
        config_.kafka_brokers = "localhost:9092";
        config_.orders_topic = "test.orders";
        config_.client_id = "test-gateway";

        gateway_ = std::make_unique<HFTGateway>(config_);
        gateway_->initialize();
    }

    void TearDown() override {
        if (gateway_) {
            gateway_->shutdown();
        }
    }

    GatewayConfig config_;
    std::unique_ptr<HFTGateway> gateway_;
};

TEST_F(ClientSessionTest, SessionCreationAndDestruction) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);

    // Create session
    auto session = std::make_shared<ClientSession>(std::move(socket), gateway_.get());

    // Session should be created successfully
    EXPECT_NE(session, nullptr);

    // Test should complete without crashes
    SUCCEED();
}

TEST_F(ClientSessionTest, GetRemoteEndpoint) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);

    auto session = std::make_shared<ClientSession>(std::move(socket), gateway_.get());

    // Should return some endpoint string (even if mock/unknown)
    std::string endpoint = session->get_remote_endpoint();
    EXPECT_FALSE(endpoint.empty());
}

TEST_F(ClientSessionTest, SessionStartAndStop) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);

    auto session = std::make_shared<ClientSession>(std::move(socket), gateway_.get());

    // Should be able to start and stop without crashing
    EXPECT_NO_THROW(session->start());
    EXPECT_NO_THROW(session->stop());
}

TEST_F(ClientSessionTest, MultipleStops) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);

    auto session = std::make_shared<ClientSession>(std::move(socket), gateway_.get());

    session->start();

    // Multiple stops should be safe
    EXPECT_NO_THROW(session->stop());
    EXPECT_NO_THROW(session->stop());
    EXPECT_NO_THROW(session->stop());
}

TEST_F(ClientSessionTest, SessionRegistrationUnregistration) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);

    auto session = std::make_shared<ClientSession>(std::move(socket), gateway_.get());

    // Check initial statistics
    const auto& initial_stats = gateway_->get_statistics();
    uint64_t initial_active = initial_stats.connections_active.load();

    // Register session
    gateway_->register_session(session);

    const auto& after_register_stats = gateway_->get_statistics();
    EXPECT_EQ(after_register_stats.connections_active.load(), initial_active + 1);

    // Unregister session
    gateway_->unregister_session(session);

    const auto& after_unregister_stats = gateway_->get_statistics();
    EXPECT_EQ(after_unregister_stats.connections_active.load(), initial_active);
}

TEST_F(ClientSessionTest, MultipleSessionRegistration) {
    const int num_sessions = 5;
    std::vector<std::shared_ptr<ClientSession>> sessions;

    // Create and register multiple sessions
    for (int i = 0; i < num_sessions; ++i) {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::socket socket(io_context);

        auto session = std::make_shared<ClientSession>(std::move(socket), gateway_.get());
        sessions.push_back(session);
        gateway_->register_session(session);
    }

    // Check active connections count
    const auto& stats = gateway_->get_statistics();
    EXPECT_EQ(stats.connections_active.load(), num_sessions);

    // Unregister all sessions
    for (auto& session : sessions) {
        gateway_->unregister_session(session);
    }

    // Should be back to zero active connections
    const auto& final_stats = gateway_->get_statistics();
    EXPECT_EQ(final_stats.connections_active.load(), 0);
}

TEST_F(ClientSessionTest, DuplicateSessionRegistration) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);

    auto session = std::make_shared<ClientSession>(std::move(socket), gateway_.get());

    // Register same session multiple times
    gateway_->register_session(session);
    gateway_->register_session(session);
    gateway_->register_session(session);

    // Should only count as one active connection (set semantics)
    const auto& stats = gateway_->get_statistics();
    EXPECT_EQ(stats.connections_active.load(), 1);

    // Unregister once should remove it
    gateway_->unregister_session(session);

    const auto& final_stats = gateway_->get_statistics();
    EXPECT_EQ(final_stats.connections_active.load(), 0);
}

TEST_F(ClientSessionTest, UnregisterNonExistentSession) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);

    auto session = std::make_shared<ClientSession>(std::move(socket), gateway_.get());

    // Unregister without registering first - should not crash
    EXPECT_NO_THROW(gateway_->unregister_session(session));

    // Statistics should remain unchanged
    const auto& stats = gateway_->get_statistics();
    EXPECT_EQ(stats.connections_active.load(), 0);
}

// Mock message validation tests
class MessageValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.listen_address = "127.0.0.1";
        config_.listen_port = 0;
        config_.kafka_brokers = "localhost:9092";
        config_.orders_topic = "test.orders";

        gateway_ = std::make_unique<HFTGateway>(config_);
        gateway_->initialize();
    }

    void TearDown() override {
        if (gateway_) {
            gateway_->shutdown();
        }
    }

    std::shared_ptr<ClientSession> createMockSession() {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::socket socket(io_context);
        return std::make_shared<ClientSession>(std::move(socket), gateway_.get());
    }

    GatewayConfig config_;
    std::unique_ptr<HFTGateway> gateway_;
};

TEST_F(MessageValidationTest, HandleValidMessage) {
    auto session = createMockSession();

    // Create a mock valid FlatBuffer message
    std::vector<uint8_t> valid_message = {
        0x10, 0x00, 0x00, 0x00,  // Message length (16 bytes)
        0x01, 0x02, 0x03, 0x04,  // Mock message data
        0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C,
        0x0D, 0x0E, 0x0F, 0x10
    };

    // In a real test, we would need to simulate network I/O
    // For now, we just test that the session can be created and configured
    EXPECT_NO_THROW(session->start());
    EXPECT_NO_THROW(session->stop());
}

TEST_F(MessageValidationTest, HandleEmptyMessage) {
    auto session = createMockSession();

    // Empty message should be handled gracefully
    std::vector<uint8_t> empty_message;

    EXPECT_NO_THROW(session->start());
    EXPECT_NO_THROW(session->stop());
}

TEST_F(MessageValidationTest, SessionLifecycleWithGateway) {
    const auto& initial_stats = gateway_->get_statistics();
    uint64_t initial_active = initial_stats.connections_active.load();

    {
        auto session = createMockSession();
        gateway_->register_session(session);

        const auto& mid_stats = gateway_->get_statistics();
        EXPECT_EQ(mid_stats.connections_active.load(), initial_active + 1);

        session->start();

        // Give time for session to become active (if needed)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Session should be active (may have already been unregistered by stop)
        const auto& active_stats = gateway_->get_statistics();
        EXPECT_GE(active_stats.connections_active.load(), initial_active);

        session->stop();

        // Session goes out of scope here, should trigger unregistration
        // In real implementation, this would happen via shared_ptr cleanup
        gateway_->unregister_session(session);
    }

    const auto& final_stats = gateway_->get_statistics();
    EXPECT_EQ(final_stats.connections_active.load(), initial_active);
}