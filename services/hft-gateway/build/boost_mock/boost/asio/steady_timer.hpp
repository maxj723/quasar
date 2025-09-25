#pragma once
#include "../system/error_code.hpp"
#include <chrono>
namespace boost { namespace asio {
class steady_timer {
public:
    steady_timer(io_context&) {}
    void expires_after(std::chrono::seconds) {}
    template<typename F> void async_wait(F) {}
};
}}
