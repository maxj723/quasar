#pragma once
#include "../system/error_code.hpp"
namespace boost { namespace asio { namespace error {
const boost::system::error_code eof{};
const boost::system::error_code connection_reset{};
const boost::system::error_code operation_aborted{};
const boost::system::error_code invalid_argument{};
}}}
