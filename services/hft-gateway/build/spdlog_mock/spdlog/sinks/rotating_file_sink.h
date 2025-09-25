#pragma once
#include "../spdlog.h"
namespace spdlog { namespace sinks {
class rotating_file_sink_mt {
public:
    rotating_file_sink_mt(const std::string&, size_t, size_t) {}
    void set_level(level) {}
    void set_pattern(const std::string&) {}
};
}}
