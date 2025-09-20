#include "gtest/gtest.h"
#include "core/MatchingEngine.h"
#include "core/Order.h"
#include "core/Trade.h"
#include <memory>
#include <vector>

using namespace quasar;

class MatchingEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<MatchingEngine>();
    }

    std::unique_ptr<MatchingEngine> engine;
};

TEST_F(MatchingEngineTest, SubmitSingleOrder) {
    uint64_t order_id = engine->submit_order(100, "BTC-USD", Side::BUY, 50000.0, 10);
    EXPECT_GT(order_id, 0);

    auto stats = engine->get_stats();
    EXPECT_EQ(stats.total_orders, 1);
    EXPECT_EQ(stats.active_orders, 1);

    EXPECT_EQ(engine->get_best_bid("BTC-USD"), 50000.0);
}

TEST_F(MatchingEngineTest, SubmitAndCancelOrder) {
    uint64_t order_id = engine->submit_order(100, "BTC-USD", Side::BUY, 50000.0, 10);
    EXPECT_GT(order_id, 0);

    bool success = engine->cancel_order(order_id);
    EXPECT_TRUE(success);

    auto stats = engine->get_stats();
    EXPECT_EQ(stats.total_orders, 1);
    EXPECT_EQ(stats.active_orders, 0);
    EXPECT_EQ(stats.cancelled_orders, 1);

    // The order should be gone from the book
    EXPECT_EQ(engine->get_best_bid("BTC-USD"), 0.0);
}

TEST_F(MatchingEngineTest, CancelNonExistentOrder) {
    bool success = engine->cancel_order(999);
    EXPECT_FALSE(success);
}

TEST_F(MatchingEngineTest, SimpleTradeAndCallback) {
    std::vector<Trade> received_trades;
    engine->set_trade_callback([&](const Trade& trade) {
        received_trades.push_back(trade);
    });

    uint64_t buy_order_id = engine->submit_order(100, "BTC-USD", Side::BUY, 50000.0, 10);
    uint64_t sell_order_id = engine->submit_order(101, "BTC-USD", Side::SELL, 50000.0, 5);

    auto stats = engine->get_stats();
    EXPECT_EQ(stats.total_orders, 2);
    EXPECT_EQ(stats.total_trades, 1);
    // One order fully filled, one partially filled
    EXPECT_EQ(stats.active_orders, 1);

    // Check the trade callback
    ASSERT_EQ(received_trades.size(), 1);
    EXPECT_EQ(received_trades[0].quantity, 5);
    EXPECT_EQ(received_trades[0].price, 50000.0);
    EXPECT_EQ(received_trades[0].taker_order_id, sell_order_id);
    EXPECT_EQ(received_trades[0].maker_order_id, buy_order_id);
}

TEST_F(MatchingEngineTest, MultipleTradesFromSingleTaker) {
    // Setup the book with multiple maker orders
    engine->submit_order(101, "BTC-USD", Side::SELL, 50000.0, 3); // Maker 1
    engine->submit_order(102, "BTC-USD", Side::SELL, 50001.0, 4); // Maker 2
    engine->submit_order(103, "BTC-USD", Side::SELL, 50002.0, 5); // Maker 3

    std::vector<Trade> received_trades;
    engine->set_trade_callback([&](const Trade& trade) {
        received_trades.push_back(trade);
    });

    // Submit a large taker order that will match all three makers
    engine->submit_order(100, "BTC-USD", Side::BUY, 50003.0, 15);

    // Check trades
    ASSERT_EQ(received_trades.size(), 3);
    EXPECT_EQ(received_trades[0].quantity, 3);
    EXPECT_EQ(received_trades[0].price, 50000.0);

    EXPECT_EQ(received_trades[1].quantity, 4);
    EXPECT_EQ(received_trades[1].price, 50001.0);

    EXPECT_EQ(received_trades[2].quantity, 5);
    EXPECT_EQ(received_trades[2].price, 50002.0);

    // Check stats
    auto stats = engine->get_stats();
    EXPECT_EQ(stats.total_orders, 4);
    EXPECT_EQ(stats.total_trades, 3);
    // Taker order has 15 - 3 - 4 - 5 = 3 remaining. All makers filled.
    EXPECT_EQ(stats.active_orders, 1);
    EXPECT_EQ(engine->get_best_bid("BTC-USD"), 50003.0);
    EXPECT_EQ(engine->get_best_ask("BTC-USD"), 0.0);
}

TEST_F(MatchingEngineTest, PartialFillThenCancel) {
    uint64_t maker_id = engine->submit_order(101, "BTC-USD", Side::BUY, 50000.0, 10);
    engine->submit_order(100, "BTC-USD", Side::SELL, 50000.0, 4); // Taker fills 4

    auto stats_after_fill = engine->get_stats();
    EXPECT_EQ(stats_after_fill.total_trades, 1);
    EXPECT_EQ(stats_after_fill.active_orders, 1); // Maker is partially filled

    // Cancel the remainder of the maker order
    bool success = engine->cancel_order(maker_id);
    EXPECT_TRUE(success);

    auto stats_after_cancel = engine->get_stats();
    EXPECT_EQ(stats_after_cancel.active_orders, 0);
    EXPECT_EQ(stats_after_cancel.cancelled_orders, 1);

    EXPECT_EQ(engine->get_best_bid("BTC-USD"), 0.0);
}

TEST_F(MatchingEngineTest, MultipleSymbolsIsolation) {
    // Submit orders for BTC-USD
    engine->submit_order(100, "BTC-USD", Side::BUY, 50000.0, 1);
    engine->submit_order(101, "BTC-USD", Side::SELL, 50001.0, 2);

    // Submit orders for ETH-USD
    engine->submit_order(200, "ETH-USD", Side::BUY, 4000.0, 10);
    engine->submit_order(201, "ETH-USD", Side::SELL, 4001.0, 20);

    // Check BTC book
    EXPECT_EQ(engine->get_best_bid("BTC-USD"), 50000.0);
    EXPECT_EQ(engine->get_best_ask("BTC-USD"), 50001.0);

    // Check ETH book
    EXPECT_EQ(engine->get_best_bid("ETH-USD"), 4000.0);
    EXPECT_EQ(engine->get_best_ask("ETH-USD"), 4001.0);

    // Make a trade on BTC-USD, should not affect ETH-USD
    std::vector<Trade> received_trades;
    engine->set_trade_callback([&](const Trade& trade) {
        received_trades.push_back(trade);
    });
    engine->submit_order(102, "BTC-USD", Side::SELL, 50000.0, 1);

    ASSERT_EQ(received_trades.size(), 1);
    EXPECT_EQ(received_trades[0].symbol, "BTC-USD");

    // Verify ETH-USD book is unchanged
    EXPECT_EQ(engine->get_best_bid("ETH-USD"), 4000.0);
    EXPECT_EQ(engine->get_best_ask("ETH-USD"), 4001.0);

    auto stats = engine->get_stats();
    EXPECT_EQ(stats.total_orders, 5);
    EXPECT_EQ(stats.total_trades, 1);
    // BTC: 1 active (sell @ 50001), ETH: 2 active
    EXPECT_EQ(stats.active_orders, 3);
}
