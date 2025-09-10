#pragma once

#include "Order.h"
#include "Trade.h"
#include <queue>
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>
#include <map>
#include <list>

namespace quasar {

class OrderBook {
public:
    OrderBook(const std::string& symbol, bool use_map__implementation = false);
    ~OrderBook() = default;

    // Add a new order to the book
    void add_order(std::unique_ptr<Order> order);

    // Cancel an existing order
    bool cancel_order(uint64_t order_id);

    // Process incoming order and return generated trades
    std::vector<Trade> process_order(std::unique_ptr<Order> order);

    // Get order book state (for market data)
    struct BookLevel {
        double price;
        uint64_t quantity;
        uint32_t order_count;
    };

    std::vector<BookLevel> get_bid_levels(size_t max_levels = 10) const;
    std::vector<BookLevel> get_ask_levels(size_t max_levels = 10) const;

    // Get best bid/ask
    double get_best_bid() const;
    double get_best_ask() const;

    // Get spread
    double get_spread() const;

    // Get total volume at each side
    uint64_t get_bid_volume() const;
    uint64_t get_ask_volume() const;

    // Get symbol
    const std::string& get_symbol() const { return symbol_; }

private:
    std::string symbol_;

    // Order storage - owns all orders
    std::unordered_map<uint64_t, std::unique_ptr<Order>> orders_;

    // Priority queues for bid and ask sides (current high-performance implementation)
    std::priority_queue<Order*, std::vector<Order*>, BuyOrderComparator> bids_;
    std::priority_queue<Order*, std::vector<Order*>, SellOrderComparator> asks_;

    // Alternative map-based implementation
    std::map<double, std::list<Order*>, std::greater<double>> bid_levels_;
    std::map<double, std::list<Order*>, std::less<double>> ask_levels_;

    // Order ID to iterator mapping for O(1) cancellation
    std::unordered_map<uint64_t, std::list<Order*>::iterator> order_iterators_;

    // Trade ID generator
    uint64_t net_trade_id{1};

    // Thread safety
    mutable std::mutex mutex_;

    // Configuration flag for choosing implementation
    bool use_map_implementation_{false};

    // Helper methods
    std::vector<Trade> match_order(Order* order);
    void clean_filled_orders();
    void add_order_unlocked(std::unique_ptr<Order> order);

    // Helper to aggreagate orders at same price level
    std::vector<BookLevel> aggregate_levels(
        const std::priority_queue<Order*, std::vector<Order*>, BuyOrderComparator>& orders,
        size_t max_levels) const;
    
    std::vector<BookLevel> aggregate_levels(
        const std::priority_queue<Order*, std::vector<Order*>, SellOrderComparator>& orders,
        size_t max_levels) const;
};

} // namespace quasar