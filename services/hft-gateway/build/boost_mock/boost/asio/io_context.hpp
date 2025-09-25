#pragma once
#include "../system/error_code.hpp"
#include <atomic>
#include <thread>
#include <chrono>

namespace boost { namespace asio {
class io_context {
public:
    io_context() : should_run_(true) {}

    void run() {
        // Simple event loop - blocks until stop() is called
        while (should_run_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // In a real implementation, this would process I/O events
        }
    }

    void stop() {
        should_run_.store(false);
    }

private:
    std::atomic<bool> should_run_;
};
}}
