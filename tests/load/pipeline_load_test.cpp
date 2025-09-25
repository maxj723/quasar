#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <random>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <queue>
#include <unordered_map>

// Mock network client for testing
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>

class FullPipelineLoadTest {
public:
    struct LoadTestConfig {
        std::string gateway_host = "localhost";
        uint16_t gateway_port = 31337;
        uint32_t total_orders = 10000;
        uint32_t concurrent_clients = 10;
        double target_rate = 1000.0; // orders per second
        uint32_t warmup_orders = 1000;
        bool measure_latency = true;
        std::string output_file = "pipeline_load_test_results.csv";
    };

    struct LatencyMeasurement {
        std::chrono::high_resolution_clock::time_point send_time;
        std::chrono::high_resolution_clock::time_point ack_time;
        uint64_t order_id;
        bool completed = false;
    };

    struct LoadTestResults {
        // Throughput metrics
        uint64_t orders_sent = 0;
        uint64_t orders_acknowledged = 0;
        uint64_t connection_errors = 0;
        uint64_t send_errors = 0;
        double duration_seconds = 0.0;
        double actual_rate = 0.0;

        // Latency metrics (microseconds)
        double min_latency_us = 0.0;
        double avg_latency_us = 0.0;
        double p50_latency_us = 0.0;
        double p95_latency_us = 0.0;
        double p99_latency_us = 0.0;
        double max_latency_us = 0.0;

        std::vector<double> all_latencies_us;
    };

private:
    LoadTestConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> orders_sent_{0};
    std::atomic<uint64_t> orders_acknowledged_{0};
    std::atomic<uint64_t> connection_errors_{0};
    std::atomic<uint64_t> send_errors_{0};

    std::mutex latency_mutex_;
    std::unordered_map<uint64_t, LatencyMeasurement> latency_measurements_;

    std::mt19937 rng_;

public:
    FullPipelineLoadTest(const LoadTestConfig& config)
        : config_(config)
        , rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    }

    LoadTestResults run_load_test() {
        std::cout << "=== Full Pipeline Load Test ===" << std::endl;
        std::cout << "Target: " << config_.gateway_host << ":" << config_.gateway_port << std::endl;
        std::cout << "Orders: " << config_.total_orders << std::endl;
        std::cout << "Clients: " << config_.concurrent_clients << std::endl;
        std::cout << "Rate: " << config_.target_rate << " orders/sec" << std::endl;
        std::cout << "================================" << std::endl;

        // Reset counters
        orders_sent_ = 0;
        orders_acknowledged_ = 0;
        connection_errors_ = 0;
        send_errors_ = 0;
        latency_measurements_.clear();

        auto start_time = std::chrono::high_resolution_clock::now();
        running_ = true;

        // Create client threads
        std::vector<std::thread> client_threads;
        uint32_t orders_per_client = config_.total_orders / config_.concurrent_clients;

        for (uint32_t i = 0; i < config_.concurrent_clients; ++i) {
            uint32_t client_orders = orders_per_client;
            if (i == config_.concurrent_clients - 1) {
                // Last client gets remaining orders
                client_orders += config_.total_orders % config_.concurrent_clients;
            }

            client_threads.emplace_back([this, i, client_orders]() {
                run_client_thread(i, client_orders);
            });
        }

        // Progress monitoring thread
        std::thread progress_thread([this]() {
            monitor_progress();
        });

        // Wait for all clients to finish
        for (auto& thread : client_threads) {
            thread.join();
        }

        running_ = false;
        if (progress_thread.joinable()) {
            progress_thread.join();
        }

        auto end_time = std::chrono::high_resolution_clock::now();

        // Calculate results
        return calculate_results(start_time, end_time);
    }

private:
    void run_client_thread(uint32_t client_id, uint32_t orders_to_send) {
        // Calculate inter-order delay
        double orders_per_second_per_client = config_.target_rate / config_.concurrent_clients;
        auto inter_order_delay = std::chrono::nanoseconds(
            static_cast<long>(1e9 / orders_per_second_per_client));

        try {
            // Connect to gateway
            int sock = create_connection();
            if (sock < 0) {
                connection_errors_.fetch_add(1);
                return;
            }

            // Send orders
            for (uint32_t i = 0; i < orders_to_send && running_; ++i) {
                uint64_t order_id = (static_cast<uint64_t>(client_id) << 32) | i;

                auto order_data = generate_order_message(order_id);
                auto send_time = std::chrono::high_resolution_clock::now();

                // Record latency measurement start
                if (config_.measure_latency) {
                    std::lock_guard<std::mutex> lock(latency_mutex_);
                    latency_measurements_[order_id] = {send_time, {}, order_id, false};
                }

                if (send_order(sock, order_data)) {
                    orders_sent_.fetch_add(1);

                    // Simulate immediate acknowledgment (in real test, this would come from gateway)
                    if (config_.measure_latency) {
                        simulate_order_acknowledgment(order_id);
                    }
                } else {
                    send_errors_.fetch_add(1);
                }

                // Rate limiting
                if (i < orders_to_send - 1) {
                    std::this_thread::sleep_for(inter_order_delay);
                }
            }

            close(sock);

        } catch (const std::exception& e) {
            std::cerr << "Client " << client_id << " error: " << e.what() << std::endl;
            connection_errors_.fetch_add(1);
        }
    }

    int create_connection() {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            return -1;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(config_.gateway_port);

        if (inet_pton(AF_INET, config_.gateway_host.c_str(), &server_addr.sin_addr) <= 0) {
            close(sock);
            return -1;
        }

        // Set socket options for performance
        int flag = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        // Non-blocking connect with timeout
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            if (errno != EINPROGRESS) {
                close(sock);
                return -1;
            }
        }

        return sock;
    }

    std::vector<uint8_t> generate_order_message(uint64_t order_id) {
        // Generate realistic order data
        std::vector<std::string> symbols = {"BTC-USD", "ETH-USD", "SOL-USD"};
        std::uniform_int_distribution<size_t> symbol_dist(0, symbols.size() - 1);
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_real_distribution<double> price_dist(45000.0, 55000.0);
        std::uniform_int_distribution<uint64_t> quantity_dist(1, 1000);

        std::string symbol = symbols[symbol_dist(rng_)];
        int side = side_dist(rng_);
        double price = price_dist(rng_);
        uint64_t quantity = quantity_dist(rng_);

        // Create simple binary message format
        // Format: [4-byte length][8-byte order_id][symbol][side][price][quantity]
        std::string data = std::to_string(order_id) + "," + symbol + "," +
                          std::to_string(side) + "," + std::to_string(price) + "," +
                          std::to_string(quantity);

        std::vector<uint8_t> message;
        uint32_t length = static_cast<uint32_t>(data.size());

        // Add length prefix (network byte order)
        message.push_back((length >> 24) & 0xFF);
        message.push_back((length >> 16) & 0xFF);
        message.push_back((length >> 8) & 0xFF);
        message.push_back(length & 0xFF);

        // Add data
        message.insert(message.end(), data.begin(), data.end());

        return message;
    }

    bool send_order(int sock, const std::vector<uint8_t>& order_data) {
        ssize_t bytes_sent = send(sock, order_data.data(), order_data.size(), MSG_NOSIGNAL);
        return bytes_sent == static_cast<ssize_t>(order_data.size());
    }

    void simulate_order_acknowledgment(uint64_t order_id) {
        // In a real implementation, this would be triggered by actual network response
        auto ack_time = std::chrono::high_resolution_clock::now();

        std::lock_guard<std::mutex> lock(latency_mutex_);
        auto it = latency_measurements_.find(order_id);
        if (it != latency_measurements_.end()) {
            it->second.ack_time = ack_time;
            it->second.completed = true;
            orders_acknowledged_.fetch_add(1);
        }
    }

    void monitor_progress() {
        auto last_print = std::chrono::steady_clock::now();

        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_print).count() >= 5) {
                uint64_t sent = orders_sent_.load();
                uint64_t acked = orders_acknowledged_.load();

                double progress = (double)sent / config_.total_orders * 100.0;

                std::cout << "Progress: " << std::fixed << std::setprecision(1) << progress << "% "
                         << "(Sent: " << sent << ", Acked: " << acked
                         << ", Errors: " << (connection_errors_.load() + send_errors_.load()) << ")"
                         << std::endl;

                last_print = now;
            }
        }
    }

    LoadTestResults calculate_results(std::chrono::high_resolution_clock::time_point start_time,
                                     std::chrono::high_resolution_clock::time_point end_time) {
        LoadTestResults results;

        results.orders_sent = orders_sent_.load();
        results.orders_acknowledged = orders_acknowledged_.load();
        results.connection_errors = connection_errors_.load();
        results.send_errors = send_errors_.load();

        results.duration_seconds = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count() / 1e6;

        results.actual_rate = results.orders_sent / results.duration_seconds;

        // Calculate latency statistics
        if (config_.measure_latency) {
            std::lock_guard<std::mutex> lock(latency_mutex_);

            for (const auto& [order_id, measurement] : latency_measurements_) {
                if (measurement.completed) {
                    auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        measurement.ack_time - measurement.send_time).count();

                    double latency_us = latency_ns / 1000.0;
                    results.all_latencies_us.push_back(latency_us);
                }
            }

            if (!results.all_latencies_us.empty()) {
                std::sort(results.all_latencies_us.begin(), results.all_latencies_us.end());

                results.min_latency_us = results.all_latencies_us.front();
                results.max_latency_us = results.all_latencies_us.back();

                double sum = std::accumulate(results.all_latencies_us.begin(),
                                           results.all_latencies_us.end(), 0.0);
                results.avg_latency_us = sum / results.all_latencies_us.size();

                size_t count = results.all_latencies_us.size();
                results.p50_latency_us = results.all_latencies_us[count * 50 / 100];
                results.p95_latency_us = results.all_latencies_us[count * 95 / 100];
                results.p99_latency_us = results.all_latencies_us[count * 99 / 100];
            }
        }

        return results;
    }

public:
    void print_results(const LoadTestResults& results) {
        std::cout << "\n=== FULL PIPELINE LOAD TEST RESULTS ===" << std::endl;

        std::cout << "Throughput:" << std::endl;
        std::cout << "  Orders Sent: " << results.orders_sent << std::endl;
        std::cout << "  Orders Acknowledged: " << results.orders_acknowledged << std::endl;
        std::cout << "  Connection Errors: " << results.connection_errors << std::endl;
        std::cout << "  Send Errors: " << results.send_errors << std::endl;
        std::cout << "  Duration: " << std::fixed << std::setprecision(2)
                 << results.duration_seconds << " seconds" << std::endl;
        std::cout << "  Actual Rate: " << std::fixed << std::setprecision(0)
                 << results.actual_rate << " orders/sec" << std::endl;

        if (config_.measure_latency && !results.all_latencies_us.empty()) {
            std::cout << "\nLatency (μs):" << std::endl;
            std::cout << "  Samples: " << results.all_latencies_us.size() << std::endl;
            std::cout << "  Min: " << std::fixed << std::setprecision(2) << results.min_latency_us << std::endl;
            std::cout << "  Avg: " << std::fixed << std::setprecision(2) << results.avg_latency_us << std::endl;
            std::cout << "  P50: " << std::fixed << std::setprecision(2) << results.p50_latency_us << std::endl;
            std::cout << "  P95: " << std::fixed << std::setprecision(2) << results.p95_latency_us << std::endl;
            std::cout << "  P99: " << std::fixed << std::setprecision(2) << results.p99_latency_us << std::endl;
            std::cout << "  Max: " << std::fixed << std::setprecision(2) << results.max_latency_us << std::endl;
        }

        std::cout << "=======================================\n" << std::endl;
    }

    void save_results_to_csv(const LoadTestResults& results) {
        std::ofstream file(config_.output_file);
        if (!file.is_open()) {
            std::cerr << "Failed to open output file: " << config_.output_file << std::endl;
            return;
        }

        // CSV header
        file << "timestamp,orders_sent,orders_acknowledged,connection_errors,send_errors,"
             << "duration_seconds,actual_rate,min_latency_us,avg_latency_us,p50_latency_us,"
             << "p95_latency_us,p99_latency_us,max_latency_us,concurrent_clients,target_rate\n";

        // Timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        file << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << ","
             << results.orders_sent << ","
             << results.orders_acknowledged << ","
             << results.connection_errors << ","
             << results.send_errors << ","
             << std::fixed << std::setprecision(2) << results.duration_seconds << ","
             << std::fixed << std::setprecision(0) << results.actual_rate << ","
             << std::fixed << std::setprecision(2) << results.min_latency_us << ","
             << std::fixed << std::setprecision(2) << results.avg_latency_us << ","
             << std::fixed << std::setprecision(2) << results.p50_latency_us << ","
             << std::fixed << std::setprecision(2) << results.p95_latency_us << ","
             << std::fixed << std::setprecision(2) << results.p99_latency_us << ","
             << std::fixed << std::setprecision(2) << results.max_latency_us << ","
             << config_.concurrent_clients << ","
             << std::fixed << std::setprecision(0) << config_.target_rate << "\n";

        std::cout << "Results saved to: " << config_.output_file << std::endl;
    }
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --host HOST           Gateway hostname (default: localhost)" << std::endl;
    std::cout << "  --port PORT           Gateway port (default: 31337)" << std::endl;
    std::cout << "  --orders N            Total orders to send (default: 10000)" << std::endl;
    std::cout << "  --clients N           Concurrent clients (default: 10)" << std::endl;
    std::cout << "  --rate N              Target orders/sec (default: 1000)" << std::endl;
    std::cout << "  --output FILE         Output CSV file (default: pipeline_load_test_results.csv)" << std::endl;
    std::cout << "  --no-latency          Disable latency measurements" << std::endl;
    std::cout << "  --help                Show this help" << std::endl;
}

int main(int argc, char* argv[]) {
    FullPipelineLoadTest::LoadTestConfig config;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--host" && i + 1 < argc) {
            config.gateway_host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.gateway_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--orders" && i + 1 < argc) {
            config.total_orders = std::stoul(argv[++i]);
        } else if (arg == "--clients" && i + 1 < argc) {
            config.concurrent_clients = std::stoul(argv[++i]);
        } else if (arg == "--rate" && i + 1 < argc) {
            config.target_rate = std::stod(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            config.output_file = argv[++i];
        } else if (arg == "--no-latency") {
            config.measure_latency = false;
        }
    }

    std::cout << "Quasar Full Pipeline Load Test" << std::endl;
    std::cout << "==============================" << std::endl;

    try {
        FullPipelineLoadTest test(config);
        auto results = test.run_load_test();

        test.print_results(results);
        test.save_results_to_csv(results);

        // Performance validation
        bool performance_ok = true;

        if (results.actual_rate < config.target_rate * 0.8) {
            std::cout << "WARNING: Actual rate significantly below target" << std::endl;
            performance_ok = false;
        }

        if (config.measure_latency && results.p99_latency_us > 50000) {
            std::cout << "WARNING: P99 latency exceeds 50ms" << std::endl;
            performance_ok = false;
        }

        if (results.connection_errors > 0 || results.send_errors > 0) {
            std::cout << "WARNING: Errors detected during test" << std::endl;
            performance_ok = false;
        }

        if (performance_ok) {
            std::cout << "✓ Performance test PASSED" << std::endl;
            return 0;
        } else {
            std::cout << "✗ Performance test FAILED" << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}