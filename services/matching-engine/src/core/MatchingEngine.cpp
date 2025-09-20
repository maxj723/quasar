#include "core/MatchingEngine.h"
#include <iostream>

namespace quasar {

MatchingEngine::MatchingEngine() {}

uint64_t MatchingEngine::submit_order(uint64_t client_id, const std::string& symbol,
                                      Side side, double price, uint64_t quantity) {
    // Generate order ID
    uint64_t order_id = next_order_id_.fetch_add(1);

    // Create order
    auto order = std::make_unique<Order>(order_id, client_id, symbol, side, price, quantity);
    Order* order_ptr = order.get();

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_orders++;
        stats_.active_orders++;
    }

    // Track order to symbol mapping
    {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        order_to_symbol_[order_id] = symbol;
    }

    // Get or create order book
    OrderBook* book = get_or_create_book(symbol);

    // Process the order
    std::vector<Trade> trades = book->process_order(std::move(order));

    // Check if the submitted (taker) order was filled
    if (order_ptr->is_filled()) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.active_orders--;
    }

    //Process trades
    for (const auto& trade : trades) {
        notify_trade(trade);
        update_stats_for_trade(trade, book);
    }

    return order_id;
}

bool MatchingEngine::cancel_order(uint64_t order_id) {
    // Find symbol for this order
    std::string symbol;
    {
        std::lock_guard<std::mutex> lock(order_map_mutex_);
        auto it = order_to_symbol_.find(order_id);
        if (it == order_to_symbol_.end()) {
            return false;
        }
        symbol = it->second;
    }

    // Find order book
    OrderBook* book = nullptr;
    {
        std::lock_guard<std::mutex> lock(order_books_mutex_);
        auto it = order_books_.find(symbol);
        if (it != order_books_.end()) {
            book = it->second.get();
        }
    }

    if (!book) {
        return false;
    }

    // Cancel the order
    bool success = book->cancel_order(order_id);

    if (success) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.cancelled_orders++;
        stats_.active_orders--;
    }

    return success;
}

double MatchingEngine::get_best_bid(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(order_books_mutex_);
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second->get_best_bid();
    }
    return 0.0;
}

double MatchingEngine::get_best_ask(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(order_books_mutex_);
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second->get_best_ask();
    }
    return 0.0;
}

double MatchingEngine::get_spread(const std::string& symbol) const {
    std::lock_guard<std::mutex> lock(order_books_mutex_);
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second->get_spread();
    }
    return 0.0;
}

std::vector<OrderBook::BookLevel> MatchingEngine::get_bid_levels(const std::string& symbol,
                                                                 size_t max_levels) const {
    std::lock_guard<std::mutex> lock(order_books_mutex_);
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second->get_bid_levels(max_levels);
    }
    return {};
}

std::vector<OrderBook::BookLevel> MatchingEngine::get_ask_levels(const std::string& symbol,
                                                                 size_t max_levels) const {
    std::lock_guard<std::mutex> lock(order_books_mutex_);
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second->get_ask_levels(max_levels);
    }
    return {};
}

MatchingEngine::EngineStats MatchingEngine::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void MatchingEngine::set_trade_callback(TradeCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    trade_callback_ = callback;
}

std::vector<std::string> MatchingEngine::get_all_symbols() const {
    std::lock_guard<std::mutex> lock(order_books_mutex_);
    std::vector<std::string> symbols;
    symbols.reserve(order_books_.size());

    for (const auto& [symbol, book] : order_books_) {
        symbols.push_back(symbol);
    }

    return symbols;
}

OrderBook* MatchingEngine::get_or_create_book(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(order_books_mutex_);

    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second.get();
    }

    // Create new order book
    auto book = std::make_unique<OrderBook>(symbol);
    OrderBook* book_ptr = book.get();
    order_books_[symbol] = std::move(book);

    return book_ptr;
}

void MatchingEngine::notify_trade(const Trade& trade) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (trade_callback_) {
        trade_callback_(trade);
    }
}

void MatchingEngine::update_stats_for_trade(const Trade& trade, OrderBook* book) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_trades++;

    // Check if the maker order was filled
    const Order* maker_order = book->get_order(trade.maker_order_id);
    if (maker_order && maker_order->is_filled()) {
        stats_.active_orders--;
    }
}

} // namespace quasar