#pragma once
#include "../system/error_code.hpp"
namespace boost { namespace asio {
class signal_set {
public:
    signal_set(io_context&, int, int) {}
    template<typename F> void async_wait(F) {}
};
}}
