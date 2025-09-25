#pragma once
#include <string>
namespace boost { namespace system {
class error_code {
public:
    error_code() = default;
    error_code(int val, const std::string& msg) : value_(val), message_(msg) {}
    operator bool() const { return value_ != 0; }
    std::string message() const { return message_; }
    int value() const { return value_; }
private:
    int value_ = 0;
    std::string message_ = "no error";
};

namespace errc {
    enum error_condition {
        operation_not_supported = 95,
        connection_aborted = 103
    };
}

inline error_code make_error_code(errc::error_condition e) {
    return error_code(static_cast<int>(e), "error");
}

}}
