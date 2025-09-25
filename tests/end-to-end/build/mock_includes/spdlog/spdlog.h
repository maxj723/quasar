#pragma once
namespace spdlog { 
enum class level { debug, info };
class logger { public: void set_level(level) {} };
inline void info(const std::string&) {}
inline void error(const std::string&) {}
}
