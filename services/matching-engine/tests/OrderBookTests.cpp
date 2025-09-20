#include "gtest/gtest.h"
#include "core/OrderBook.h"
#include "core/Order.h"

using namespace quasar;

// Test fixture for OrderBook tests
class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        orderBook = std::make_unique<OrderBook>("BTC-USD");
    }

    std::unique_ptr<OrderBook> orderBook;
};

// Test that a single buy order is added correctly
TEST_F(OrderBookTest, AddSingleBuyOrder) {
    auto order = std::make_unique<Order>(1, 100, "BTC-USD", Side::BUY, 50000.0, 10);
    orderBook->add_order(std::move(order));

    EXPECT_EQ(orderBook->get_best_bid(), 50000.0);
    EXPECT_EQ(orderBook->get_best_ask(), 0.0);
}

// Test that a single sell order is added correctly
TEST_F(OrderBookTest, AddSingleSellOrder) {
    auto order = std::make_unique<Order>(1, 100, "BTC-USD", Side::SELL, 50100.0, 10);
    orderBook->add_order(std::move(order));

    EXPECT_EQ(orderBook->get_best_bid(), 0.0);
    EXPECT_EQ(orderBook->get_best_ask(), 50100.0);
}

// Test that adding non-matching buy and sell orders results in a correct spread
TEST_F(OrderBookTest, AddBuyAndSellNoMatch) {
    auto buyOrder = std::make_unique<Order>(1, 100, "BTC-USD", Side::BUY, 50000.0, 10);
    auto sellOrder = std::make_unique<Order>(2, 101, "BTC-USD", Side::SELL, 50100.0, 5);

    orderBook->add_order(std::move(buyOrder));
    orderBook->add_order(std::move(sellOrder));

    EXPECT_EQ(orderBook->get_best_bid(), 50000.0);
    EXPECT_EQ(orderBook->get_best_ask(), 50100.0);
    EXPECT_EQ(orderBook->get_spread(), 100.0);
}

// Test a simple order match
TEST_F(OrderBookTest, SimpleMatch) {
    auto buyOrder = std::make_unique<Order>(1, 100, "BTC-USD", Side::BUY, 50000.0, 10);
    orderBook->add_order(std::move(buyOrder));

    EXPECT_EQ(orderBook->get_best_bid(), 50000.0);

    auto sellOrder = std::make_unique<Order>(2, 101, "BTC-USD", Side::SELL, 50000.0, 5);
    std::vector<Trade> trades = orderBook->process_order(std::move(sellOrder));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, 5);
    EXPECT_EQ(trades[0].price, 50000.0);

    // The buy order should be partially filled, so it remains on the book
    EXPECT_EQ(orderBook->get_best_bid(), 50000.0);
    EXPECT_EQ(orderBook->get_bid_volume(), 5);
    EXPECT_EQ(orderBook->get_best_ask(), 0.0); // The sell order should be fully filled
}

// Main function to run all tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
