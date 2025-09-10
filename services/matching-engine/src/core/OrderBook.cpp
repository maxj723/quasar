#include "services/matching-engine/include/core/OrderBook.h"
#include <algorithm>
#include <map>

namespace quasar {

OrderBook::OrderBook(const std::string& symbol, bool use_map_implementation)
    : symbol_(symbol), use_map_implementation_(use_map_implementation) {}

void OrderBook::add_order(std::unique_ptr<Order> order) {
    std::lock_guard<std::mutex> lock(mutex_);
    add_order_unlocked(std::move(order));
}

void OrderBook::add_order_unlocked(std::unique_ptr<Order> order) {
    Order* order_ptr = order.get();
    uint64_t order_id = order->order_id;

    // Store the order
    orders_[order_id] = std::move(order);

    // Add to appropriate heap
    if (order_ptr->is_buy()) {
        bids_.push(order_ptr);
    } else {
        asks_.push(order_ptr);
    }
}

bool OrderBook::cancel_order(uint64_t order_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }

    it->second->cancel();
    return true;
}

std::vector<Trade> OrderBook::process_order(std::unique_ptr<Order> order) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // First try to match the order
    std::vector<Trade> trades = match_order(order.get());

    // If order is not fully filled, add it to the book (without acquiring lock again)
    if (!order->is_filled() && order->status != OrderStatus::CANCELLED) {
        add_order_unlocked(std::move(order));
    }

    // Clean up any fully filled orders from the heaps
    clean_filled_orders();

    return trades;
}

std::vector<Trade> OrderBook::match_order(Order* incoming_order) {
    std::vector<Trade> trades;

    // Match against opposite side
    if (incoming_order->is_buy()) {
        // Buy order matches against asks
        while (!asks_.empty() && incoming_order->remaining_quantity() > 0) {
            Order* top_order = asks_.top();

            // Skip cancelled orders
            if (top_order->status == OrderStatus::CANCELLED) {
                asks_.pop();
                continue;
            }

            // Check if prices cross (buy price >= ask price)
            if (incoming_order->price < top_order->price) {
                break; // No more matches possible
            }

            // Calculate trade quantity
            uint64_t trade_quantity = std::min(
                incoming_order->remaining_quantity(),
                top_order->remaining_quantity()
            );

            // Create trade
            Trade trade(
                next_trade_id_++,
                incoming_order->order_id,
                top_order->order_id,
                incoming_order->client_id,
                top_order->client_id,
                symbol_,
                top_order->price, // Trade at maker's price
                trade_quantity
            );
            trades.push_back(trade);

            // Update order quantities
            incoming_order->fill(trade_quantity);
            top_order->fill(trade_quantity);

            // Remove fully filled orders
            if (top_order->is_filled()) {
                asks_.pop();
            }
        }
    } else {
        // Sell order matches against bids
        while (!bids_.empty() && incoming_order->remaining_quantity() > 0) {
            Order* top_order = bids_.top();

            // Skip cancelled orders
            if (top_order->status == OrderStatus::CANCELLED) {
                bids_.pop();
                continue;
            }

            // Check if prices cross (sell price <= bid price)
            if (incoming_order->price > top_order->price) {
                break; // No more matches possible
            }

            // Calculate trade quantity
            uint64_t trade_quantity = std::min(
                incoming_order->remaining_quantity(),
                top_order->remaining_quantity()
            );

            // Create trade
            Trade trade(
                next_trade_id_++,
                incoming_order->order_id,
                top_order->order_id,
                incoming_order->client_id,
                top_order->client_id,
                symbol_,
                top_order->price, // Trade at maker's price
                trade_quantity
            );
            trades.push_back(trade);

            // Update order quantities
            incoming_order->fill(trade_quantity);
            top_order->fill(trade_quantity);

            // Remove fully filled orders
            if (top_order->is_filled()) {
                bids_.pop();
            }
        }
    }

    return trades;
}

void OrderBook::clean_filled_orders() {
    // Clean bid side
    while (!bids_.empty() &&
           (bids_.top()->is_filled() || bids_.top()->status == OrderStatus::CANCELLED)) {
        bids_.pop();
    }

    // Clean ask side
    while (!asks_.empty() &&
           (asks_.top()->is_filled() || asks_.top()->status == OrderStatus::CANCELLED)) {
        asks_.pop();
    }
}

double OrderBook::get_best_bid() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find first non-cancelled order
    auto temp_bids = bids_;
    while (!temp_bids.empty()) {
        Order* order = temp_bids.top();
        if (order->status != OrderStatus::CANCELLED && !order->is_filled()) {
            return order->price;
        }
        temp_bids.pop();
    }

    return 0.0;
}

double OrderBook::get_best_ask() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find first non-cancelled order
    auto temp_asks = asks_;
    while (!temp_asks.empty()) {
        Order* order = temp_asks.top();
        if (order->status != OrderStatus::CANCELLED && !order->is_filled()) {
            return order->price;
        }
        temp_asks.pop();
    }

    return 0.0;
}

double OrderBook::get_spread() const {
    double best_bid = get_best_bid();
    double best_ask = get_best_ask();

    if (best_bid == 0.0 || best_ask == 0.0) {
        return 0.0;
    }

    return best_ask - best_bid;
}

std::vector<OrderBook::BookLevel> OrderBook::get_bid_levels(size_t max_levels) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return aggregate_levels(bids_, max_levels);
}

std::vector<OrderBook::BookLevel> OrderBook::get_ask_levels(size_t max_levels) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return aggregate_levels(asks_, max_levels);
}

template<typename Container>
std::vector<OrderBook::BookLevel> aggregate_levels_impl(Container container, size_t max_levels) {
    std::map<double, OrderBook::BookLevel> level_map;

    while (!container.empty() && level_map.size() < max_levels) {
        Order* order = container.top();
        container.pop();

        if (order->status == OrderStatus::CANCELLED || order->is_filled()) {
            continue;
        }

        auto& level = level_map[order->price];
        level.price = order->price;
        level.quantity += order->remaining_quantity();
        level.order_count++;
    }

    std::vector<OrderBook::BookLevel> result;
    result.reserve(level_map.size());

    for (const auto& [price, level] : level_map) {
        result.push_back(level);
    }

    return result;
}

std::vector<OrderBook::BookLevel> OrderBook::aggregate_levels(
    const std::priority_queue<Order*, std::vector<Order*>, BuyOrderComparator>& orders,
    size_t max_levels) const {
    return aggregate_levels_impl(orders, max_levels);
}

std::vector<OrderBook::BookLevel> OrderBook::aggregate_levels(
    const std::priority_queue<Order*, std::vector<Order*>, SellOrderComparator>& orders,
    size_t max_levels) const {
    return aggregate_levels_impl(orders, max_levels);
}

uint64_t OrderBook::get_bid_volume() const {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t total_volume = 0;
    auto temp_bids = bids_;

    while (!temp_bids.empty()) {
        Order* order = temp_bids.top();
        temp_bids.pop();

        if (order->status != OrderStatus::CANCELLED && !order->is_filled()) {
            total_volume += order->remaining_quantity();
        }
    }

    return total_volume;
}

uint64_t OrderBook::get_ask_volume() const {
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t total_volume = 0;
    auto temp_asks = asks_;

    while (!temp_asks.empty()) {
        Order* order = temp_asks.top();
        temp_asks.pop();

        if (order->status != OrderStatus::CANCELLED && !order->is_filled()) {
            total_volume += order->remaining_quantity();
        }
    }

    return total_volume;
}

} // namespace quasar