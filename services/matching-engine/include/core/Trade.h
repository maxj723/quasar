#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <iostream>

namespace quasar {

struct Trade {
    uint64_t trade_id{0};
    uint64_t taker_order_id{0};
    uint64_t maker_order_id{0};
    uint64_t taker_client_id{0};
    uint64_t maker_client_id{0};
    std::string symbol;
    double price{0.0};
    uint64_t quantity{0};
    std::chrono::system_clock::time_point timestamp;

    Trade() = default;

    Trade(uint64_t id, uint64_t taker_id, uint64_t maker_id,
          uint64_t taker_client, uint64_t maker_client,
          const std::string& sym, double p, uint64_t q)
        : trade_id(id), taker_order_id(taker_id), maker_order_id(maker_id),
          taker_client_id(taker_client), maker_client_id(maker_client),
          symbol(sym), price(p), quantity(q) {
        timestamp = std::chrono::system_clock::now();
    }

    // Helper method to get notional value (alias for get_value)
    double get_notional() const {
        return price * static_cast<double>(quantity);
    }

    // Helper method to get timestamp as microseconds since epoch
    uint64_t timestamp_micros() const {
        auto duration = timestamp.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }

    // Additional utility methods
    static Trade create(uint64_t trade_id, uint64_t taker_order_id, uint64_t maker_order_id,
                        uint64_t taker_client_id, uint64_t maker_client_id,
                        const std::string& symbol, double price, uint64_t quantity);

    double get_value() const;
    uint64_t get_age_micros() const;
    uint64_t get_age_millis() const;
    std::string format_timestamp() const;
    std::string to_string() const;
    std::string to_json() const;
    std::string to_csv() const;
    static std::string csv_header();

    bool involves_order(uint64_t order_id) const;
    bool involves_client(uint64_t client_id) const;
    uint64_t get_client_for_side(bool is_taker) const;
    uint64_t get_order_for_side(bool is_taker) const;

    // Comparison operators
    bool operator<(const Trade& other) const;
    bool operator>(const Trade& other) const;
    bool operator==(const Trade& other) const;
    bool operator!=(const Trade& other) const;
};

// Stream output operator
std::ostream& operator<<(std::ostream& os, const Trade& trade);

} // namespace quasar