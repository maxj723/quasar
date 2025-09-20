#include "core/MatchingEngine.h"
#include "core/Trade.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace quasar;

class PerformanceBenchmark {
private:
    std::unique_ptr<MatchingEngine> engine_;
    std::vector<double> order_latencies_;
    std::atomic<uint64_t> trade_count_{0};
    std::mt19937 rng_;

public:
    PerformanceBenchmark() : engine_(std::make_unique<MatchingEngine>()) {
        rng_.seed(std::chrono::steady_clock::now().time_since_epoch().count());
        engine_->set_trade_callback([this](const Trade& trade) {
            trade_count_.fetch_add(1);
        });
    }

    struct BenchmarkConfig {
        std::string test_name;
        uint64_t total_orders;
        double target_rate;
        std::string symbol;
        double mid_price;
        double spread;
        bool aggressive_mode;
        bool warmup_book;
    };

    struct BenchmarkResults {
        std::string test_name;
        uint64_t total_orders;
        uint64_t total_trades;
        double duration_seconds;
        double actual_rate;
        double trades_per_second;

        double min_latency_us;
        double avg_latency_us;
        double p50_latency_us;
        double p95_latency_us;
        double p99_latency_us;
        double max_latency_us;

        MatchingEngine::EngineStats engine_stats;
    };

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
        double price = (side == Side::BUY) ? best_ask + 1.0 : best_bid - 1.0;
        uint64_t quantity = quantity_dist(rng_);

        return {symbol, side, price, quantity};
    }

    void warmup_order_book(const std::string& symbol, double mid_price, double spread, int num_orders = 100) {
        std::cout << "Warming up order book with " << num_orders << " orders..." << std::endl;
        for (int i = 0; i < num_orders; ++i) {
            auto order_spec = generate_market_making_order(symbol, mid_price, spread);
            engine_->submit_order(i, order_spec.symbol, order_spec.side, order_spec.price, order_spec.quantity);
        }
    }

    BenchmarkResults run_benchmark(const BenchmarkConfig& config) {
        std::cout << "\n=== " << config.test_name << " ===" << std::endl;
        std::cout << "Target: " << config.total_orders << " orders at " << config.target_rate << " orders/sec" << std::endl;

        // Reset state
        order_latencies_.clear();
        order_latencies_.reserve(config.total_orders);
        trade_count_.store(0);

        // Warmup if requested
        if (config.warmup_book) {
            warmup_order_book(config.symbol, config.mid_price, config.spread * 2.0);
        }

        auto start_time = std::chrono::steady_clock::now();
        auto inter_order_delay = std::chrono::nanoseconds(static_cast<long>(1e9 / config.target_rate));

        // Progress tracking
        uint64_t progress_interval = std::max(config.total_orders / 20, static_cast<uint64_t>(1));

        for (uint64_t i = 0; i < config.total_orders; ++i) {
            auto order_start = std::chrono::steady_clock::now();

            // Generate order based on mode
            OrderSpec order_spec;
            if (config.aggressive_mode && config.warmup_book) {
                double best_bid = engine_->get_best_bid(config.symbol);
                double best_ask = engine_->get_best_ask(config.symbol);
                if (best_bid > 0 && best_ask > 0) {
                    order_spec = generate_aggressive_order(config.symbol, best_bid, best_ask);
                } else {
                    order_spec = generate_market_making_order(config.symbol, config.mid_price, config.spread);
                }
            } else {
                order_spec = generate_market_making_order(config.symbol, config.mid_price, config.spread);
            }

            // Submit order
            uint64_t order_id = engine_->submit_order(i, order_spec.symbol, order_spec.side, order_spec.price, order_spec.quantity);

            auto order_end = std::chrono::steady_clock::now();
            auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(order_end - order_start).count();
            order_latencies_.push_back(static_cast<double>(latency_ns));

            // Progress update
            if (i % progress_interval == 0) {
                double progress = (double(i) / config.total_orders) * 100.0;
                std::cout << "\rProgress: " << std::fixed << std::setprecision(1) << progress << "%" << std::flush;
            }

            // Rate limiting
            if (i < config.total_orders - 1) {
                std::this_thread::sleep_for(inter_order_delay);
            }
        }

        auto end_time = std::chrono::steady_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        std::cout << "\rProgress: 100.0%" << std::endl;

        return calculate_results(config, total_duration.count() / 1e6);
    }

    BenchmarkResults calculate_results(const BenchmarkConfig& config, double duration_seconds) {
        BenchmarkResults results;
        results.test_name = config.test_name;
        results.total_orders = config.total_orders;
        results.total_trades = trade_count_.load();
        results.duration_seconds = duration_seconds;
        results.actual_rate = results.total_orders / duration_seconds;
        results.trades_per_second = results.total_trades / duration_seconds;

        // Calculate latency statistics
        if (!order_latencies_.empty()) {
            std::sort(order_latencies_.begin(), order_latencies_.end());

            results.min_latency_us = order_latencies_.front() / 1000.0;
            results.max_latency_us = order_latencies_.back() / 1000.0;

            double sum = 0.0;
            for (double latency : order_latencies_) {
                sum += latency;
            }
            results.avg_latency_us = (sum / order_latencies_.size()) / 1000.0;

            // Percentiles
            auto p50_idx = order_latencies_.size() * 50 / 100;
            auto p95_idx = order_latencies_.size() * 95 / 100;
            auto p99_idx = order_latencies_.size() * 99 / 100;

            results.p50_latency_us = order_latencies_[p50_idx] / 1000.0;
            results.p95_latency_us = order_latencies_[p95_idx] / 1000.0;
            results.p99_latency_us = order_latencies_[p99_idx] / 1000.0;
        }

        results.engine_stats = engine_->get_stats();
        return results;
    }

    void print_results(const BenchmarkResults& results) {
        std::cout << "\n=== Results for " << results.test_name << " ===" << std::endl;

        std::cout << "Performance:" << std::endl;
        std::cout << "  Orders Processed: " << results.total_orders << std::endl;
        std::cout << "  Trades Generated: " << results.total_trades << std::endl;
        std::cout << "  Duration: " << std::fixed << std::setprecision(2) << results.duration_seconds << " seconds" << std::endl;
        std::cout << "  Actual Rate: " << std::fixed << std::setprecision(0) << results.actual_rate << " orders/sec" << std::endl;
        std::cout << "  Trade Rate: " << std::fixed << std::setprecision(0) << results.trades_per_second << " trades/sec" << std::endl;

        std::cout << "\nLatency (μs):" << std::endl;
        std::cout << "  Min: " << std::fixed << std::setprecision(2) << results.min_latency_us << std::endl;
        std::cout << "  Avg: " << std::fixed << std::setprecision(2) << results.avg_latency_us << std::endl;
        std::cout << "  P50: " << std::fixed << std::setprecision(2) << results.p50_latency_us << std::endl;
        std::cout << "  P95: " << std::fixed << std::setprecision(2) << results.p95_latency_us << std::endl;
        std::cout << "  P99: " << std::fixed << std::setprecision(2) << results.p99_latency_us << std::endl;
        std::cout << "  Max: " << std::fixed << std::setprecision(2) << results.max_latency_us << std::endl;

        std::cout << "\nEngine State:" << std::endl;
        std::cout << "  Active Orders: " << results.engine_stats.active_orders << std::endl;
        std::cout << "  Total Trades: " << results.engine_stats.total_trades << std::endl;
        std::cout << "  Cancelled Orders: " << results.engine_stats.cancelled_orders << std::endl;
    }

    // Generate timestamped filename
    std::string generate_timestamped_filename(const std::string& base_name, const std::string& extension = "csv") {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << "../results/" << base_name << "_"
           << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
           << "_" << std::setfill('0') << std::setw(3) << ms.count()
           << "." << extension;
        return ss.str();
    }

    void print_csv_header(std::ostream& out = std::cout) {
        out << "test_name,total_orders,total_trades,duration_seconds,actual_rate,trades_per_second,";
        out << "min_latency_us,avg_latency_us,p50_latency_us,p95_latency_us,p99_latency_us,max_latency_us,";
        out << "active_orders,engine_total_trades,cancelled_orders" << std::endl;
    }

    void print_csv_row(const BenchmarkResults& results, std::ostream& out = std::cout) {
        out << results.test_name << ","
            << results.total_orders << ","
            << results.total_trades << ","
            << std::fixed << std::setprecision(2) << results.duration_seconds << ","
            << std::fixed << std::setprecision(0) << results.actual_rate << ","
            << std::fixed << std::setprecision(0) << results.trades_per_second << ","
            << std::fixed << std::setprecision(2) << results.min_latency_us << ","
            << std::fixed << std::setprecision(2) << results.avg_latency_us << ","
            << std::fixed << std::setprecision(2) << results.p50_latency_us << ","
            << std::fixed << std::setprecision(2) << results.p95_latency_us << ","
            << std::fixed << std::setprecision(2) << results.p99_latency_us << ","
            << std::fixed << std::setprecision(2) << results.max_latency_us << ","
            << results.engine_stats.active_orders << ","
            << results.engine_stats.total_trades << ","
            << results.engine_stats.cancelled_orders << std::endl;
    }

    // Auto-save results to timestamped CSV file
    void auto_save_results(const std::vector<BenchmarkResults>& all_results, const std::string& suite_name) {
        std::string filename = generate_timestamped_filename("benchmark_" + suite_name);
        std::ofstream file(filename);

        if (file.is_open()) {
            print_csv_header(file);
            for (const auto& results : all_results) {
                print_csv_row(results, file);
            }
            file.close();
            std::cout << "\nResults saved to: " << filename << std::endl;
        } else {
            std::cerr << "Failed to save results to: " << filename << std::endl;
        }
    }

    void reset() {
        engine_ = std::make_unique<MatchingEngine>();
        engine_->set_trade_callback([this](const Trade& trade) {
            trade_count_.fetch_add(1);
        });
        order_latencies_.clear();
        trade_count_.store(0);
    }
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --help                    Show this help message" << std::endl;
    std::cout << "  --quick                   Run quick benchmark suite (default)" << std::endl;
    std::cout << "  --full                    Run full benchmark suite" << std::endl;
    std::cout << "  --extreme                 Run extreme stress tests" << std::endl;
    std::cout << "  --csv                     Output results in CSV format" << std::endl;
    std::cout << "  --custom N R              Run custom test with N orders at R orders/sec" << std::endl;
    std::cout << "  --symbol SYM              Use symbol SYM (default: BTC-USD)" << std::endl;
    std::cout << "  --mid-price P             Use mid price P (default: 50000)" << std::endl;
    std::cout << "  --spread S                Use spread S (default: 10)" << std::endl;
}

int main(int argc, char* argv[]) {
    PerformanceBenchmark benchmark;
    std::vector<PerformanceBenchmark::BenchmarkConfig> configs;
    bool csv_output = false;
    std::string symbol = "BTC-USD";
    double mid_price = 50000.0;
    double spread = 10.0;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--csv") {
            csv_output = true;
        } else if (arg == "--symbol" && i + 1 < argc) {
            symbol = argv[++i];
        } else if (arg == "--mid-price" && i + 1 < argc) {
            mid_price = std::stod(argv[++i]);
        } else if (arg == "--spread" && i + 1 < argc) {
            spread = std::stod(argv[++i]);
        } else if (arg == "--custom" && i + 2 < argc) {
            uint64_t orders = std::stoull(argv[++i]);
            double rate = std::stod(argv[++i]);
            configs.push_back({"Custom", orders, rate, symbol, mid_price, spread, false, false});
        } else if (arg == "--quick") {
            configs = {
                {"Quick_LowVolume", 1000, 100.0, symbol, mid_price, spread, false, false},
                {"Quick_MediumVolume", 5000, 500.0, symbol, mid_price, spread, false, false},
                {"Quick_Aggressive", 2000, 200.0, symbol, mid_price, spread, true, true}
            };
        } else if (arg == "--full") {
            configs = {
                {"LowVolume_MarketMaking", 1000, 100.0, symbol, mid_price, spread, false, false},
                {"MediumVolume_MarketMaking", 5000, 500.0, symbol, mid_price, spread, false, false},
                {"HighVolume_MarketMaking", 10000, 1000.0, symbol, mid_price, spread, false, false},
                {"Aggressive_Trading", 5000, 500.0, symbol, mid_price, spread, true, true},
                {"HighFrequency_Burst", 20000, 2000.0, symbol, mid_price, spread, false, false}
            };
        } else if (arg == "--extreme") {
            configs = {
                {"Extreme_HighFrequency", 50000, 5000.0, symbol, mid_price, spread, false, false},
                {"Extreme_Aggressive", 25000, 2500.0, symbol, mid_price, spread, true, true},
                {"Extreme_Sustained", 100000, 10000.0, symbol, mid_price, spread, false, false}
            };
        }
    }

    // Default to quick if no specific suite chosen
    if (configs.empty()) {
        configs = {
            {"Quick_LowVolume", 1000, 100.0, symbol, mid_price, spread, false, false},
            {"Quick_MediumVolume", 5000, 500.0, symbol, mid_price, spread, false, false},
            {"Quick_Aggressive", 2000, 200.0, symbol, mid_price, spread, true, true}
        };
    }

    std::cout << "Quasar Matching Engine Performance Benchmark" << std::endl;
    std::cout << "=============================================" << std::endl;

    std::vector<PerformanceBenchmark::BenchmarkResults> all_results;
    std::string suite_name = "custom";

    // Determine suite name for auto-saving
    if (configs.size() > 1) {
        if (configs[0].test_name.find("Quick") != std::string::npos) {
            suite_name = "quick";
        } else if (configs[0].test_name.find("Extreme") != std::string::npos) {
            suite_name = "extreme";
        } else if (configs.size() >= 5) {
            suite_name = "full";
        }
    }

    if (csv_output) {
        benchmark.print_csv_header();
    }

    // Run benchmarks
    for (const auto& config : configs) {
        auto results = benchmark.run_benchmark(config);
        all_results.push_back(results);

        if (csv_output) {
            benchmark.print_csv_row(results);
        } else {
            benchmark.print_results(results);
        }

        benchmark.reset(); // Reset for next test

        if (!csv_output && &config != &configs.back()) {
            std::cout << "\nPausing 2 seconds before next test...\n" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    // Auto-save results unless user requested CSV to stdout
    if (!csv_output && !all_results.empty()) {
        benchmark.auto_save_results(all_results, suite_name);
    }

    return 0;
}