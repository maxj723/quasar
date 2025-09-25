#pragma once
#include <vector>
namespace boost { namespace asio {
template<typename T> auto buffer(T& t) { return &t; }
}}
