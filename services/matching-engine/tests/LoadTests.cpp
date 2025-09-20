#include "gtest/gtest.h"
#include "core/MatchingEngine.h"
#include "core/Order.h"
#include "core/Trade.h"
#include <memory>
#include <vector>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace quasar;

class LoadTestFramework {
public:
    struct LatencyStats {
        double min_ns = std::numeric_limits<double>::max();
        double max_ns = 0.0;
        double avg_ns = 0.0;
        double p50_ns = 0.0;
        double p95_ns = 0.0;
        double p99_ns = 0.0;
        std::vector<double> all_latencies;
    };

    struct ThroughputStats {
        uint64_t total_orders = 0;
        uint64_t total_trades = 0;
        double duration_seconds = 0.0;
        double orders_per_second = 0.0;
        double trades_per_second = 0.0;
    };

    struct LoadTestResults {
        std::string test_name;
        LatencyStats latency;
        ThroughputStats throughput;
        MatchingEngine::EngineStats engine_stats;
    };

private:
    std::unique_ptr<MatchingEngine> engine_;
    std::vector<double> order_latencies_;
    std::atomic<uint64_t> trade_count_{0};

public:
    std::mt19937 rng_;
    LoadTestFramework() : engine_(std::make_unique<MatchingEngine>()) {
        rng_.seed(std::chrono::steady_clock::now().time_since_epoch().count());

        // Set up trade callback to count trades
        engine_->set_trade_callback([this](const Trade& trade) {
            trade_count_.fetch_add(1);
        });
    }

    // Generate a random order with controlled price distribution
    struct OrderSpec {
        std::string symbol;
        Side side;
        double price;
        uint64_t quantity;
    };

    OrderSpec generate_market_making_order(const std::string& symbol, double mid_price, double spread) {
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_real_distribution<double> price_offset_dist(0.0, spread / 2.0);
        std::uniform_int_distribution<uint64_t> quantity_dist(1, 100);

        Side side = (side_dist(rng_) == 0) ? Side::BUY : Side::SELL;
        double price_offset = price_offset_dist(rng_);
        double price = (side == Side::BUY) ? mid_price - price_offset : mid_price + price_offset;
        uint64_t quantity = quantity_dist(rng_);

        return {symbol, side, price, quantity};
    }

    OrderSpec generate_aggressive_order(const std::string& symbol, double best_bid, double best_ask) {
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_int_distribution<uint64_t> quantity_dist(1, 50);

        Side side = (side_dist(rng_) == 0) ? Side::BUY : Side::SELL;
        // Aggressive orders cross the spread to cause immediate matches
        double price = (side == Side::BUY) ? best_ask + 1.0 : best_bid - 1.0;
        uint64_t quantity = quantity_dist(rng_);

        return {symbol, side, price, quantity};
    }

    // Run a load test with specified parameters
    LoadTestResults run_load_test(
        const std::string& test_name,
        uint64_t num_orders,
        double orders_per_second,
        const std::function<OrderSpec()>& order_generator
    ) {
        std::cout << "\n=== Running Load Test: " << test_name << " ===" << std::endl;
        std::cout << "Orders: " << num_orders << ", Rate: " << orders_per_second << " orders/sec" << std::endl;

        order_latencies_.clear();
        order_latencies_.reserve(num_orders);
        trade_count_.store(0);

        auto start_time = std::chrono::steady_clock::now();
        auto inter_order_delay = std::chrono::nanoseconds(static_cast<long>(1e9 / orders_per_second));

        for (uint64_t i = 0; i < num_orders; ++i) {
            auto order_start = std::chrono::steady_clock::now();

            // Generate and submit order
            auto order_spec = order_generator();
            uint64_t order_id = engine_->submit_order(
                i, // client_id
                order_spec.symbol,
                order_spec.side,
                order_spec.price,
                order_spec.quantity
            );

            auto order_end = std::chrono::steady_clock::now();
            auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(order_end - order_start).count();
            order_latencies_.push_back(static_cast<double>(latency_ns));

            // Rate limiting
            if (i < num_orders - 1) {
                std::this_thread::sleep_for(inter_order_delay);
            }
        }

        auto end_time = std::chrono::steady_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        return calculate_results(test_name, total_duration.count() / 1e6);
    }

    // Calculate comprehensive statistics from collected data
    LoadTestResults calculate_results(const std::string& test_name, double duration_seconds) {
        LoadTestResults results;
        results.test_name = test_name;

        // Calculate latency statistics
        if (!order_latencies_.empty()) {
            std::sort(order_latencies_.begin(), order_latencies_.end());

            results.latency.min_ns = order_latencies_.front();
            results.latency.max_ns = order_latencies_.back();

            double sum = 0.0;
            for (double latency : order_latencies_) {
                sum += latency;
            }
            results.latency.avg_ns = sum / order_latencies_.size();

            // Percentiles
            auto p50_idx = order_latencies_.size() * 50 / 100;
            auto p95_idx = order_latencies_.size() * 95 / 100;
            auto p99_idx = order_latencies_.size() * 99 / 100;

            results.latency.p50_ns = order_latencies_[p50_idx];
            results.latency.p95_ns = order_latencies_[p95_idx];
            results.latency.p99_ns = order_latencies_[p99_idx];
            results.latency.all_latencies = order_latencies_;
        }

        // Calculate throughput statistics
        results.throughput.total_orders = order_latencies_.size();
        results.throughput.total_trades = trade_count_.load();
        results.throughput.duration_seconds = duration_seconds;
        results.throughput.orders_per_second = results.throughput.total_orders / duration_seconds;
        results.throughput.trades_per_second = results.throughput.total_trades / duration_seconds;

        // Get engine statistics
        results.engine_stats = engine_->get_stats();

        return results;
    }

    // Print detailed results
    void print_results(const LoadTestResults& results, const std::string& test_name) {
        std::cout << "\n--- Results for " << test_name << " ---" << std::endl;

        // Latency stats
        std::cout << "Latency Statistics:" << std::endl;
        std::cout << "  Min: " << results.latency.min_ns / 1000.0 << " μs" << std::endl;
        std::cout << "  Avg: " << results.latency.avg_ns / 1000.0 << " μs" << std::endl;
        std::cout << "  P50: " << results.latency.p50_ns / 1000.0 << " μs" << std::endl;
        std::cout << "  P95: " << results.latency.p95_ns / 1000.0 << " μs" << std::endl;
        std::cout << "  P99: " << results.latency.p99_ns / 1000.0 << " μs" << std::endl;
        std::cout << "  Max: " << results.latency.max_ns / 1000.0 << " μs" << std::endl;

        // Throughput stats
        std::cout << "Throughput Statistics:" << std::endl;
        std::cout << "  Orders: " << results.throughput.total_orders << std::endl;
        std::cout << "  Trades: " << results.throughput.total_trades << std::endl;
        std::cout << "  Duration: " << results.throughput.duration_seconds << " seconds" << std::endl;
        std::cout << "  Orders/sec: " << static_cast<int>(results.throughput.orders_per_second) << std::endl;
        std::cout << "  Trades/sec: " << static_cast<int>(results.throughput.trades_per_second) << std::endl;

        // Engine stats
        std::cout << "Engine Statistics:" << std::endl;
        std::cout << "  Total Orders: " << results.engine_stats.total_orders << std::endl;
        std::cout << "  Active Orders: " << results.engine_stats.active_orders << std::endl;
        std::cout << "  Total Trades: " << results.engine_stats.total_trades << std::endl;
        std::cout << "  Cancelled Orders: " << results.engine_stats.cancelled_orders << std::endl;
        std::cout << "  Rejected Orders: " << results.engine_stats.rejected_orders << std::endl;
    }

    // Generate timestamped filename
    std::string generate_timestamped_filename(const std::string& base_name, const std::string& extension = "csv") {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << "results/" << base_name << "_"
           << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
           << "_" << std::setfill('0') << std::setw(3) << ms.count()
           << "." << extension;
        return ss.str();
    }

    // Save results to CSV for further analysis
    void save_results_to_csv(const LoadTestResults& results, const std::string& filename) {
        std::ofstream file(filename);
        file << "test_name,total_orders,total_trades,duration_seconds,orders_per_second,trades_per_second,";
        file << "min_latency_us,avg_latency_us,p50_latency_us,p95_latency_us,p99_latency_us,max_latency_us,";
        file << "engine_total_orders,engine_active_orders,engine_total_trades,engine_cancelled,engine_rejected\n";

        file << results.test_name << ","
             << results.throughput.total_orders << ","
             << results.throughput.total_trades << ","
             << results.throughput.duration_seconds << ","
             << results.throughput.orders_per_second << ","
             << results.throughput.trades_per_second << ","
             << results.latency.min_ns / 1000.0 << ","
             << results.latency.avg_ns / 1000.0 << ","
             << results.latency.p50_ns / 1000.0 << ","
             << results.latency.p95_ns / 1000.0 << ","
             << results.latency.max_ns / 1000.0 << ","
             << results.engine_stats.total_orders << ","
             << results.engine_stats.active_orders << ","
             << results.engine_stats.total_trades << ","
             << results.engine_stats.cancelled_orders << ","
             << results.engine_stats.rejected_orders << "\n";
    }

    // Auto-save results with timestamped filename
    void auto_save_results(const LoadTestResults& results, const std::string& test_name) {
        std::string safe_test_name = test_name;
        std::replace(safe_test_name.begin(), safe_test_name.end(), ' ', '_');
        std::string filename = generate_timestamped_filename(safe_test_name);
        save_results_to_csv(results, filename);
        std::cout << "Results saved to: " << filename << std::endl;
    }

    MatchingEngine* get_engine() { return engine_.get(); }
    void reset_engine() {
        engine_ = std::make_unique<MatchingEngine>();
        engine_->set_trade_callback([this](const Trade& trade) {
            trade_count_.fetch_add(1);
        });
    }
};

class MatchingEngineLoadTest : public ::testing::Test {
protected:
    void SetUp() override {
        framework = std::make_unique<LoadTestFramework>();
    }

    std::unique_ptr<LoadTestFramework> framework;
};

TEST_F(MatchingEngineLoadTest, LowVolumeMarketMaking) {
    auto order_generator = [this]() {
        return framework->generate_market_making_order("BTC-USD", 50000.0, 20.0);
    };

    auto results = framework->run_load_test("Low Volume Market Making", 1000, 100.0, order_generator);
    framework->print_results(results, "Low Volume Market Making");
    framework->auto_save_results(results, "Low_Volume_Market_Making");

    // Basic performance assertions
    EXPECT_GT(results.throughput.orders_per_second, 75.0); // Allow for timing variations
    EXPECT_LT(results.latency.p95_ns / 1000.0, 1000.0); // P95 < 1ms
    EXPECT_EQ(results.engine_stats.total_orders, 1000);
}

TEST_F(MatchingEngineLoadTest, HighVolumeMarketMaking) {
    auto order_generator = [this]() {
        return framework->generate_market_making_order("BTC-USD", 50000.0, 20.0);
    };

    auto results = framework->run_load_test("High Volume Market Making", 10000, 1000.0, order_generator);
    framework->print_results(results, "High Volume Market Making");
    framework->auto_save_results(results, "High_Volume_Market_Making");

    // Performance assertions for high volume
    EXPECT_GT(results.throughput.orders_per_second, 800.0); // Allow for timing variations
    EXPECT_LT(results.latency.p99_ns / 1000.0, 5000.0); // P99 < 5ms
    EXPECT_EQ(results.engine_stats.total_orders, 10000);
}

TEST_F(MatchingEngineLoadTest, AggressiveTradingScenario) {
    // First, seed the book with some orders
    for (int i = 0; i < 100; ++i) {
        auto order_spec = framework->generate_market_making_order("BTC-USD", 50000.0, 100.0);
        framework->get_engine()->submit_order(i, order_spec.symbol, order_spec.side, order_spec.price, order_spec.quantity);
    }

    double best_bid = framework->get_engine()->get_best_bid("BTC-USD");
    double best_ask = framework->get_engine()->get_best_ask("BTC-USD");

    auto order_generator = [this, best_bid, best_ask]() {
        return framework->generate_aggressive_order("BTC-USD", best_bid, best_ask);
    };

    auto results = framework->run_load_test("Aggressive Trading", 5000, 500.0, order_generator);
    framework->print_results(results, "Aggressive Trading");
    framework->auto_save_results(results, "Aggressive_Trading");

    // Should generate significant trading activity
    EXPECT_GT(results.throughput.trades_per_second, 100.0);
    EXPECT_GT(results.engine_stats.total_trades, 1000);
}

TEST_F(MatchingEngineLoadTest, MultiSymbolLoadTest) {
    std::vector<std::string> symbols = {"BTC-USD", "ETH-USD", "ADA-USD", "SOL-USD"};
    std::vector<double> mid_prices = {50000.0, 4000.0, 2.0, 100.0};

    auto order_generator = [this, symbols, mid_prices]() {
        std::uniform_int_distribution<size_t> symbol_dist(0, symbols.size() - 1);
        size_t idx = symbol_dist(framework->rng_);
        return framework->generate_market_making_order(symbols[idx], mid_prices[idx], mid_prices[idx] * 0.01);
    };

    auto results = framework->run_load_test("Multi-Symbol Load", 5000, 1000.0, order_generator);
    framework->print_results(results, "Multi-Symbol Load");
    framework->auto_save_results(results, "Multi_Symbol_Load");

    // Verify orders are distributed across symbols
    auto all_symbols = framework->get_engine()->get_all_symbols();
    EXPECT_GE(all_symbols.size(), 3); // At least 3 symbols should have orders
}

TEST_F(MatchingEngineLoadTest, SustainedHighFrequencyTest) {
    auto order_generator = [this]() {
        return framework->generate_market_making_order("BTC-USD", 50000.0, 10.0);
    };

    auto results = framework->run_load_test("Sustained High Frequency", 50000, 5000.0, order_generator);
    framework->print_results(results, "Sustained High Frequency");
    framework->auto_save_results(results, "Sustained_High_Frequency");

    // Very demanding performance requirements
    EXPECT_GT(results.throughput.orders_per_second, 3500.0); // Allow for timing variations
    EXPECT_LT(results.latency.p99_ns / 1000.0, 15000.0); // P99 < 15ms under extreme load
    EXPECT_EQ(results.engine_stats.total_orders, 50000);
}