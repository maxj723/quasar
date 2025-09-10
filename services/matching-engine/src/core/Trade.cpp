#include "services/matching-engine/include/core/Trade.h"
#include <sstream>
#include <iomanip>

namespace quasar {

// Create a trade with automatic timestamp
Trade Trade::create(uint64_t trade_id, uint64_t taker_order_id, uint64_t maker_order_id,
                    uint64_t taker_client_id, uint64_t maker_client_id,
                    const std::string& symbol, double price, uint64_t quantity) {
    return Trade(trade_id, taker_order_id, maker_order_id,
                 taker_client_id, maker_client_id, symbol, price, quantity);
}

// Get trade value in monetary terms
double Trade::get_value() const {
    return price * static_cast<double>(quantity);
}

// Get age of trade in microseconds
uint64_t Trade::get_age_micros() const {
    auto now = std::chrono::system_clock::now();
    auto age = now - timestamp;
    return std::chrono::duration_cast<std::chrono::microseconds>(age).count();
}

// Get age of trade in milliseconds
uint64_t Trade::get_age_millis() const {
    auto now = std::chrono::system_clock::now();
    auto age = now - timestamp;
    return std::chrono::duration_cast<std::chrono::milliseconds>(age).count();
}

// Formate timestamp as ISO string
std::string Trade::format_timestamp() const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

// Convert trade to string representation
std::string Trade::to_string() const {
    std::ostringstream oss;
    oss << "Trade{"
        << "id=" << trade_id
        << ", symbol=" << symbol
        << ", price=" << std::fixed << std::setprecision(2) << price
        << ", qty=" << quantity
        << ", value=" << std::fixed << std::setprecision(2) << get_value()
        << ", taker_order=" << taker_order_id
        << ", maker_order=" << maker_order_id
        << ", taker_client=" << taker_client_id
        << ", maker_client=" << maker_client_id
        << ", timestamp=" << timestamp_micros()
        << "}";
    return oss.str();
}

// Convert trade to JSON format
std::string Trade::to_json() const {
    std::ostringstream oss;
    oss << "{"
        << "\"trade_id\":" << trade_id
        << "\"symbol\":" << symbol
        << "\"price\":" << std::fixed << std::setprecision(2) << price
        << "\"quantity\":" << quantity
        << "\"value\":" << std::fixed << std::setprecision(2) << get_value()
        << "\"taker_order_id\":" << taker_order_id
        << "\"maker_order_id\":" << maker_order_id
        << "\"taker_client_id\":" << taker_client_id
        << "\"maker_client_id\":" << maker_client_id
        << "\"timestamp_micros\":" << timestamp_micros()
        << "\"timestamp_iso\":\"" << format_timestamp() << "\""
        << "}";
    return oss.str();
}

// Convert trade to CSV format
std::string Trade::to_csv() const {
    std::ostringstream oss;
    oss << trade_id << ","
        << symbol << ","
        << std::fixed << std::setprecision(2) << price << ","
        << quantity << ","
        << std::fixed << std::setprecision(2) << get_value() << ","
        << taker_order_id << ","
        << maker_order_id << ","
        << taker_client_id << ","
        << maker_client_id  << ","
        << timestamp_micros() << ","
        << format_timestamp();
    return oss.str();
}

// Get CSV header
std::string Trade::csv_header() {
    return "trade_id,symbol,price,quantity,value,taker_order_id,maker_order_id,"
           "taker_client_id,maker_client_id,timestamp_micros,timestamp_iso";
}

// Check if this trade involves a specific order
bool Trade::involves_order(uint64_t order_id) const {
    return taker_order_id == order_id || maker_order_id == order_id;
}

// Check if this trade involves a specific client
bool Trade::involves_client(uint64_t client_id) const {
    return taker_client_id == client_id || maker_client_id == client_id;
}

uint64_t Trade::get_client_for_side(bool is_taker) const {
    return is_taker ? taker_client_id : maker_client_id;
}

uint64_t Trade::get_order_for_side(bool is_taker) const {
    return is_taker ? taker_order_id : maker_order_id;
}

// Compare trades by timestamp (for sorting)
bool Trade::operator<(const Trade& other) const {
    return timestamp < other.timestamp;
}

bool Trade::operator>(const Trade& other) const {
    return timestamp > other.timestamp;
}

bool Trade::operator==(const Trade& other) const {
    return trade_id == other.trade_id;
}

bool Trade::operator!=(const Trade& other) const {
    return !(*this == other);
}

// Stream output operator
std::ostream& operator<<(std::ostream& os, const Trade& trade) {
    return os << trade.to_string();
}

} // namespace quasar