#pragma once

#include "OrderBook.h"
#include "Order.h"
#include "Trade.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>

namespace quasar {

class MatchingEngine {
public:
    MatchingEngine();
    ~MatchingEngine() = default;

    // Order management
    uint64_t submit_order(uint64_t client_id, const std::string& symbol,
                         Side side, double price, uint64_t quantity);

    bool cancel_order(uint64_t order_id);

    double get_best_bid(const std::string& symbol) const;
    double get_best_ask(const std::string& symbol) const;
    double get_spread(const std::string& symbol) const;

    std::vector<OrderBook::BookLevel> get_bid_levels(const std::string& symbol,
                                                    size_t max_levels = 10) const;

    std::vector<OrderBook::BookLevel> get_ask_levels(const std::string& symbol,
                                                    size_t max_levels = 10) const;

    std::vector<Trade> get_trades(const std::string& symbol, size_t num_trades) const;

    std::vector<Order> get_open_orders(const std::string& symbol) const;

    // Statistics
    struct EngineStats {
        uint64_t total_orders{0};
        uint64_t active_orders{0};
        uint64_t total_trades{0};
        uint64_t cancelled_orders{0};
        uint64_t rejected_orders{0};
    };

    EngineStats get_stats() const;

    // Callbacks for trade notifications
    using TradeCallback = std::function<void(const Trade&)>;
    void set_trade_callback(TradeCallback callback);

    // Get all symbols
    std::vector<std::string> get_all_symbols() const;

private:
    // Order books by symbol
    mutable std::mutex order_books_mutex_;
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> order_books_;

    // Order ID to symbol mapping for cancellations
    mutable std::mutex order_map_mutex_;
    std::unordered_map<uint64_t, std::string> order_to_symbol_;

    // Order ID generator
    std::atomic<uint64_t> next_order_id_{1};

    // Statistics
    mutable std::mutex stats_mutex_;
    EngineStats stats_;

    // Trade callback
    std::mutex callback_mutex_;
    TradeCallback trade_callback_;

    // Helper methods
    OrderBook* get_or_create_book(const std::string& symbol);
    void notify_trade(const Trade& trade);
    void update_stats_for_trade(const Trade& trade, OrderBook* book);
};

} // namespace quasar