#pragma once
#include "../system/error_code.hpp"
namespace boost { namespace asio {
template<typename S, typename B, typename F> void async_read(S&, B, F f) {
    f(boost::system::error_code{}, 0);
}
}}
