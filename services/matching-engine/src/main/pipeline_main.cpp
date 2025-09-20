#include "core/MatchingEngine.h"
#include "core/Trade.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

// Helper to print trades when they occur
void on_trade(const quasar::Trade& trade) {
    std::cout << "TRADE: " << trade.to_string() << std::endl;
}

// Helper to split a string by a delimiter
std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

int main() {
    auto engine = std::make_unique<quasar::MatchingEngine>();
    engine->set_trade_callback(on_trade);

    std::cout << "--- Quasar Matching Engine CLI ---" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  SUBMIT <symbol> <side> <price> <quantity> (e.g., SUBMIT BTC-USD BUY 50000 10)" << std::endl;
    std::cout << "  CANCEL <order_id>" << std::endl;
    std::cout << "  BOOK <symbol>" << std::endl;
    std::cout << "  EXIT" << std::endl << std::endl;

    for (std::string line; std::getline(std::cin, line);) {
        if (line == "EXIT") {
            break;
        }

        auto tokens = split(line, ' ');
        if (tokens.empty()) {
            continue;
        }

        const std::string& command = tokens[0];

        try {
            if (command == "SUBMIT" && tokens.size() == 5) {
                const std::string& symbol = tokens[1];
                const std::string& side_str = tokens[2];
                double price = std::stod(tokens[3]);
                uint64_t quantity = std::stoull(tokens[4]);
                
                quasar::Side side = (side_str == "BUY") ? quasar::Side::BUY : quasar::Side::SELL;
                
                uint64_t order_id = engine->submit_order(0, symbol, side, price, quantity);
                std::cout << "SUBMITTED order_id: " << order_id << std::endl;

            } else if (command == "CANCEL" && tokens.size() == 2) {
                uint64_t order_id = std::stoull(tokens[1]);
                if (engine->cancel_order(order_id)) {
                    std::cout << "CANCELLED order_id: " << order_id << std::endl;
                } else {
                    std::cout << "FAILED to cancel order_id: " << order_id << std::endl;
                }
            } else if (command == "BOOK" && tokens.size() == 2) {
                const std::string& symbol = tokens[1];
                std::cout << "--- Order Book: " << symbol << " ---" << std::endl;
                auto asks = engine->get_ask_levels(symbol);
                auto bids = engine->get_bid_levels(symbol);

                std::cout << "ASKS:" << std::endl;
                for (const auto& level : asks) {
                    std::cout << "  " << level.price << " | " << level.quantity << std::endl;
                }
                std::cout << "BIDS:" << std::endl;
                for (const auto& level : bids) {
                    std::cout << "  " << level.price << " | " << level.quantity << std::endl;
                }
                std::cout << "--------------------" << std::endl;
            }
            else {
                std::cout << "Invalid command or arguments." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing command: " << e.what() << std::endl;
        }
    }

    return 0;
}
