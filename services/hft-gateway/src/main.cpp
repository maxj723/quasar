#include "HFTGateway.h"

#include <iostream>
#include <signal.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main(int, char**) {
    try {
        // Setup logging (simplified for mock)
        auto logger = spdlog::default_logger();
        spdlog::set_default_logger(logger);

        spdlog::info("Starting Quasar HFT Gateway");

        // Load configuration from environment
        auto config = quasar::gateway::GatewayConfig::from_environment();

        // Create and initialize gateway
        quasar::gateway::HFTGateway gateway(config);

        if (!gateway.initialize()) {
            spdlog::error("Failed to initialize HFT Gateway");
            return 1;
        }

        // Run the gateway (this blocks until shutdown)
        gateway.run();

        spdlog::info("HFT Gateway exiting normally");
        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    } catch (...) {
        spdlog::error("Unknown fatal error");
        return 1;
    }
}