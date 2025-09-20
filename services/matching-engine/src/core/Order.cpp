#include "core/Order.h"
#include <sstream>
#include <iomanip>

namespace quasar {

// Convert Side enum to string
std::string to_string(Side side) {
    switch (side) {
        case Side::BUY: return "BUY";
        case Side::SELL: return "SELL";
        default: return "UNKNOWN";
    }
}

// Convert OrderType enum to string
std::string to_string(OrderType type) {
    switch (type) {
        case OrderType::LIMIT: return "LIMIT";
        case OrderType::MARKET: return "MARKET";
        default: return "UNKNOWN";
    }
}

// Convert OrderStatus enum to string
std::string to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::NEW: return "NEW";
        case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
        case OrderStatus::REJECTED: return "REJECTED";
        default: return "UNKNOWN";
    }
}

// Update timestamp when order is modified
void Order::update_timestamp() {
    updated_time = std::chrono::system_clock::now();
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        updated_time.time_since_epoch()).count();
}

// Fill the order and update status
void Order::fill(uint64_t fill_quantity) {
    if (fill_quantity > remaining_quantity()) {
        fill_quantity = remaining_quantity();
    }

    filled_quantity += fill_quantity;

    if (is_filled()) {
        status = OrderStatus::FILLED;
    } else if (filled_quantity > 0) {
        status = OrderStatus::PARTIALLY_FILLED;
    }

    update_timestamp();
}

// Cancel the order
void Order::cancel() {
    status = OrderStatus::CANCELLED;
    update_timestamp();
}

// Reject the order
void Order::reject() {
    status = OrderStatus::REJECTED;
    update_timestamp();
}

// Calculate fill percentage
double Order::fill_percentage() const {
    if (quantity == 0) return 0.0;
    return (static_cast<double>(filled_quantity) / static_cast<double>(quantity)) * 100.0;
}

// Calculate notional value (price * quantity)
double Order::get_notional() const {
    return price * static_cast<double>(quantity);
}

// Calculate filled notional value
double Order::get_filled_notional() const {
    return price * static_cast<double>(filled_quantity);
}

// Calculate remaining notional value
double Order::get_remaining_notional() const {
    return price * static_cast<double>(remaining_quantity());
}

// Get age of order in microseconds
uint64_t Order::get_age_micros() const {
    auto now = std::chrono::system_clock::now();
    auto age = now - created_time;
    return std::chrono::duration_cast<std::chrono::microseconds>(age).count();
}

// Check if order can be matched against another order
bool Order::can_match_with(const Order& other) const {
    // Order must be for same symbol
    if (symbol != other.symbol) return false;

    // Orders must be on opposite sides
    if (side == other.side) return false;

    // Both orders must be active
    if (status != OrderStatus::NEW && status != OrderStatus::PARTIALLY_FILLED) return false;
    if (other.status != OrderStatus::NEW && other.status != OrderStatus::PARTIALLY_FILLED) return false;

    // Check price compatibility
    if (is_buy() && other.is_sell()) {
        return price >= other.price; // Buy price >= Sell price
    } else if (is_sell() && other.is_buy()) {
        return price <= other.price; // Sell price <= Buy price
    }

    return false;
}

// Convert order to string representation
std::string Order::to_string() const {
    std::ostringstream oss;
    oss << "Order{"
        << "id=" << order_id
        << ", client=" << client_id
        << ", symbol=" << symbol
        << ", side=" << quasar::to_string(side)
        << ", type=" << quasar::to_string(type)
        << ", price=" << std::fixed << std::setprecision(2) << price
        << ", qty=" << quantity
        << ", filled=" << filled_quantity
        << ", status=" << quasar::to_string(status)
        << ", timestamp=" << timestamp
        << "}";
    return oss.str();
}

// Stream output operator
std::ostream& operator<<(std::ostream& os, const Order& order) {
    return os << order.to_string();
}

// Stream output operators for enums
std::ostream& operator<<(std::ostream& os, Side side) {
    return os << to_string(side);
}

std::ostream& operator<<(std::ostream& os, OrderType type) {
    return os << to_string(type);
}

std::ostream& operator<<(std::ostream& os, OrderStatus status) {
    return os << to_string(status);
}

} // namespace quasar