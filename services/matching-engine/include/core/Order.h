#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <iostream>

namespace quasar {

enum class Side {
    BUY,
    SELL
};

enum class OrderType {
    LIMIT,
    MARKET
};

enum class OrderStatus {
    NEW,
    PARTIALLY_FILLED,
    FILLED,
    CANCELLED,
    REJECTED
};

struct Order {
    // Order identification
    uint64_t order_id{0};
    uint64_t client_id{0};
    std::string symbol;

    // Order details
    Side side{Side::BUY};
    OrderType type{OrderType::LIMIT};
    double price{0.0};
    uint64_t quantity{0};
    uint64_t filled_quantity{0};

    // Status and timestamps
    OrderStatus status{OrderStatus::NEW};
    std::chrono::system_clock::time_point created_time;
    std::chrono::system_clock::time_point updated_time;
    uint64_t timestamp{0}; // Microseconds since epoch for performance

    // Constructor
    Order() = default;

    Order(uint64_t id, uint64_t client, const std::string& sym,
          Side s, double p, uint64_t q)
        : order_id(id), client_id(client), symbol(sym),
          side(s), price(p), quantity(q), filled_quantity(0),
          status(OrderStatus::NEW) {
        created_time = std::chrono::system_clock::now();
        updated_time = created_time;
        timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            created_time.time_since_epoch()).count();    
    }

    // Helper methods
    uint64_t remaining_quantity() const {
        return quantity - filled_quantity;
    }

    bool is_filled() const {
        return filled_quantity >= quantity;
    }

    bool is_buy() const {
        return side == Side::BUY;
    }

    bool is_sell() const {
        return side == Side::SELL;
    }

    void fill(uint64_t fill_quantity);

    void cancel();

    void reject();

    // Additional utility methods
    double fill_percentage() const;
    double get_notional() const;
    double get_filled_notional() const;
    double get_remaining_notional() const;
    uint64_t get_age_micros() const;
    bool can_match_with(const Order& order) const;
    std::string to_string() const;

private:
    void update_timestamp();
};

struct BuyOrderComparator {
    bool operator()(const Order* a, const Order* b) const {
        // For buy orders: higher price has priority
        // If prices are equal, earlier order has priority (FIFO)
        if (a->price != b->price) {
            return a->price < b->price;
        }
        return a->order_id > b->order_id;
    }
};

struct SellOrderComparator {
    bool operator()(const Order* a, const Order* b) const {
        // For sell orders: lower price has priority
        // If prices are equal, earlier order has priority (FIFO)
        if (a->price != b->price) {
            return a->price > b->price;
        }
        return a->order_id > b->order_id;
    }
};

// Utility functions for enum conversions
std::string to_string(Side side);
std::string to_string(OrderType type);
std::string to_string(OrderStatus status);

// Stream output operators
std::ostream& operator<<(std::ostream& os, const Order& order);
std::ostream& operator<<(std::ostream& os, Side side);
std::ostream& operator<<(std::ostream& os, OrderType type);
std::ostream& operator<<(std::ostream& os, OrderStatus status);

} // namespace quasar