#pragma once
#include <memory>
#include <string>
#include <iostream>
namespace spdlog {
enum class level { debug, info, warn, error };
class logger {
public:
    logger(const std::string&, std::initializer_list<std::shared_ptr<void>>) {}
    template<typename... Args> void debug(const std::string& fmt, Args&&... args) { std::cout << "[DEBUG] " << fmt << std::endl; }
    template<typename... Args> void info(const std::string& fmt, Args&&... args) { std::cout << "[INFO] " << fmt << std::endl; }
    template<typename... Args> void warn(const std::string& fmt, Args&&... args) { std::cout << "[WARN] " << fmt << std::endl; }
    template<typename... Args> void error(const std::string& fmt, Args&&... args) { std::cout << "[ERROR] " << fmt << std::endl; }
    void set_level(level) {}
};
inline std::shared_ptr<logger> get(const std::string&) { return nullptr; }
inline std::shared_ptr<logger> default_logger() { static auto l = std::make_shared<logger>("", std::initializer_list<std::shared_ptr<void>>{}); return l; }
inline void register_logger(std::shared_ptr<logger>) {}
inline void set_default_logger(std::shared_ptr<logger>) {}
template<typename... Args> void info(const std::string& fmt, Args&&... args) { std::cout << "[INFO] " << fmt << std::endl; }
template<typename... Args> void error(const std::string& fmt, Args&&... args) { std::cout << "[ERROR] " << fmt << std::endl; }
typedef std::initializer_list<std::shared_ptr<void>> sinks_init_list;
}
