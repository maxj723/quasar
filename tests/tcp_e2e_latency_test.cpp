#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <iomanip>

struct LatencyMeasurement {
    uint64_t order_id;
    std::chrono::high_resolution_clock::time_point submit_time;
    std::chrono::high_resolution_clock::time_point response_time;
    uint64_t latency_us;
    bool success;
};

class TCPEndToEndLatencyTest {
private:
    int sockfd_;
    std::string host_;
    int port_;
    std::vector<LatencyMeasurement> measurements_;

public:
    TCPEndToEndLatencyTest(const std::string& host, int port)
        : sockfd_(-1), host_(host), port_(port) {}

    ~TCPEndToEndLatencyTest() {
        if (sockfd_ != -1) {
            close(sockfd_);
        }
    }

    bool connect() {
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ == -1) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        // Enable TCP_NODELAY for minimum latency
        int flag = 1;
        setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);
        inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr);

        if (::connect(sockfd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            std::cerr << "Failed to connect to " << host_ << ":" << port_ << std::endl;
            return false;
        }

        std::cout << "[TCP E2E] Connected to HFT Gateway at " << host_ << ":" << port_ << std::endl;
        return true;
    }

    bool send_order_and_measure_latency(uint64_t order_id, const std::string& symbol,
                                       const std::string& side, double price, uint32_t quantity) {
        LatencyMeasurement measurement;
        measurement.order_id = order_id;
        measurement.success = false;

        // Create order message (simple text protocol for testing)
        std::stringstream order_msg;
        order_msg << "ORDER," << order_id << "," << symbol << "," << side
                  << "," << std::fixed << std::setprecision(2) << price
                  << "," << quantity << "\n";

        std::string msg = order_msg.str();

        // Record submit time
        measurement.submit_time = std::chrono::high_resolution_clock::now();

        // Send order
        if (send(sockfd_, msg.c_str(), msg.length(), 0) == -1) {
            std::cerr << "Failed to send order " << order_id << std::endl;
            measurements_.push_back(measurement);
            return false;
        }

        // Wait for response (acknowledgment or trade notification)
        char buffer[1024];
        ssize_t bytes_received = recv(sockfd_, buffer, sizeof(buffer)-1, 0);

        // Record response time
        measurement.response_time = std::chrono::high_resolution_clock::now();

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            measurement.success = true;

            // Calculate latency in microseconds
            auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                measurement.response_time - measurement.submit_time);
            measurement.latency_us = latency.count();

            std::cout << "[TCP E2E] Order " << order_id << " -> "
                      << measurement.latency_us << "μs (Response: " << buffer << ")" << std::endl;
        } else {
            std::cerr << "Failed to receive response for order " << order_id << std::endl;
        }

        measurements_.push_back(measurement);
        return measurement.success;
    }

    void run_latency_test(int num_orders = 1000) {
        std::cout << "\n=== TCP END-TO-END LATENCY TEST ===" << std::endl;
        std::cout << "Testing " << num_orders << " orders through complete pipeline:" << std::endl;
        std::cout << "TCP Client -> HFT Gateway -> Kafka -> Matching Engine -> Response" << std::endl;
        std::cout << std::endl;

        measurements_.clear();
        measurements_.reserve(num_orders);

        std::vector<std::string> symbols = {"BTC-USD", "ETH-USD", "ADA-USD", "SOL-USD"};
        std::vector<std::string> sides = {"BUY", "SELL"};

        auto test_start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_orders; ++i) {
            uint64_t order_id = 1000000 + i;
            std::string symbol = symbols[i % symbols.size()];
            std::string side = sides[i % sides.size()];
            double price = 50000.0 + (i % 1000) * 10.0;  // Vary price
            uint32_t quantity = 1 + (i % 10);  // Vary quantity

            send_order_and_measure_latency(order_id, symbol, side, price, quantity);

            // Progress update every 100 orders
            if ((i + 1) % 100 == 0) {
                std::cout << "[TCP E2E] Processed " << (i + 1) << "/" << num_orders << " orders" << std::endl;
            }

            // Small delay to avoid overwhelming the system
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        auto test_end = std::chrono::high_resolution_clock::now();
        auto total_test_time = std::chrono::duration_cast<std::chrono::milliseconds>(test_end - test_start);

        std::cout << "\n[TCP E2E] Test completed in " << total_test_time.count() << "ms" << std::endl;
    }

    void generate_latency_report(const std::string& output_file) {
        if (measurements_.empty()) {
            std::cerr << "No measurements to report" << std::endl;
            return;
        }

        // Filter successful measurements
        std::vector<uint64_t> successful_latencies;
        int successful_orders = 0;

        for (const auto& m : measurements_) {
            if (m.success) {
                successful_latencies.push_back(m.latency_us);
                successful_orders++;
            }
        }

        if (successful_latencies.empty()) {
            std::cerr << "No successful measurements to report" << std::endl;
            return;
        }

        // Sort latencies for percentile calculations
        std::sort(successful_latencies.begin(), successful_latencies.end());

        // Calculate statistics
        uint64_t min_latency = successful_latencies.front();
        uint64_t max_latency = successful_latencies.back();
        uint64_t sum_latency = std::accumulate(successful_latencies.begin(), successful_latencies.end(), 0ULL);
        double avg_latency = static_cast<double>(sum_latency) / successful_latencies.size();

        auto p50_idx = successful_latencies.size() * 50 / 100;
        auto p95_idx = successful_latencies.size() * 95 / 100;
        auto p99_idx = successful_latencies.size() * 99 / 100;

        uint64_t p50_latency = successful_latencies[p50_idx];
        uint64_t p95_latency = successful_latencies[p95_idx];
        uint64_t p99_latency = successful_latencies[p99_idx];

        // Write CSV report
        std::ofstream csv_file(output_file);
        csv_file << "metric,value_us,description\n";
        csv_file << "total_orders," << measurements_.size() << ",Total orders submitted\n";
        csv_file << "successful_orders," << successful_orders << ",Orders with successful responses\n";
        csv_file << "success_rate," << (100.0 * successful_orders / measurements_.size()) << ",Success percentage\n";
        csv_file << "min_latency," << min_latency << ",Minimum end-to-end latency (microseconds)\n";
        csv_file << "avg_latency," << std::fixed << std::setprecision(2) << avg_latency << ",Average end-to-end latency (microseconds)\n";
        csv_file << "p50_latency," << p50_latency << ",50th percentile latency (microseconds)\n";
        csv_file << "p95_latency," << p95_latency << ",95th percentile latency (microseconds)\n";
        csv_file << "p99_latency," << p99_latency << ",99th percentile latency (microseconds)\n";
        csv_file << "max_latency," << max_latency << ",Maximum end-to-end latency (microseconds)\n";
        csv_file.close();

        // Print summary to console
        std::cout << "\n=== TCP END-TO-END LATENCY ANALYSIS ===" << std::endl;
        std::cout << "📊 Total Orders: " << measurements_.size() << std::endl;
        std::cout << "✅ Successful: " << successful_orders << " ("
                  << std::fixed << std::setprecision(1) << (100.0 * successful_orders / measurements_.size()) << "%)" << std::endl;
        std::cout << "⚡ Latency Metrics (End-to-End TCP → Matching Engine → Response):" << std::endl;
        std::cout << "   Min:     " << std::setw(8) << min_latency << " μs" << std::endl;
        std::cout << "   Average: " << std::setw(8) << std::fixed << std::setprecision(2) << avg_latency << " μs" << std::endl;
        std::cout << "   P50:     " << std::setw(8) << p50_latency << " μs" << std::endl;
        std::cout << "   P95:     " << std::setw(8) << p95_latency << " μs" << std::endl;
        std::cout << "   P99:     " << std::setw(8) << p99_latency << " μs" << std::endl;
        std::cout << "   Max:     " << std::setw(8) << max_latency << " μs" << std::endl;
        std::cout << "\n📄 Detailed CSV Report: " << output_file << std::endl;

        // Write detailed measurements
        std::string detailed_file = output_file.substr(0, output_file.find_last_of('.')) + "_detailed.csv";
        std::ofstream detailed_csv(detailed_file);
        detailed_csv << "order_id,submit_time_us,response_time_us,latency_us,success\n";

        for (const auto& m : measurements_) {
            auto submit_us = std::chrono::duration_cast<std::chrono::microseconds>(
                m.submit_time.time_since_epoch()).count();
            auto response_us = std::chrono::duration_cast<std::chrono::microseconds>(
                m.response_time.time_since_epoch()).count();

            detailed_csv << m.order_id << "," << submit_us << "," << response_us
                        << "," << m.latency_us << "," << (m.success ? "1" : "0") << "\n";
        }
        detailed_csv.close();

        std::cout << "📊 Detailed Measurements: " << detailed_file << std::endl;
    }
};

int main() {
    std::cout << "🚀 TCP END-TO-END LATENCY TEST STARTING" << std::endl;
    std::cout << "Testing complete pipeline: TCP → HFT Gateway → Kafka → Matching Engine" << std::endl;
    std::cout << std::endl;

    TCPEndToEndLatencyTest test("127.0.0.1", 31337);

    if (!test.connect()) {
        std::cerr << "❌ Failed to connect to HFT Gateway" << std::endl;
        return 1;
    }

    // Run the latency test with 500 orders
    test.run_latency_test(500);

    // Generate timestamp for results
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");

    std::string results_dir = "/Users/maxjohnson/Documents/quasar/results/tcp_e2e_latency_" + timestamp.str();
    system(("mkdir -p " + results_dir).c_str());

    std::string report_file = results_dir + "/tcp_e2e_latency_results.csv";
    test.generate_latency_report(report_file);

    std::cout << "\n🎯 TCP END-TO-END LATENCY TEST COMPLETE!" << std::endl;
    std::cout << "📂 Results saved to: " << results_dir << std::endl;

    return 0;
}