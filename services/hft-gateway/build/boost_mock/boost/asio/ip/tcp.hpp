#pragma once
#include "../../system/error_code.hpp"
#include "../io_context.hpp"
#include <string>
#include <memory>
#include <thread>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

namespace boost { namespace asio { namespace ip {

struct address {
    static address from_string(const std::string& addr) {
        address a;
        a.addr_str = addr;
        return a;
    }
    std::string to_string() const { return addr_str; }
    std::string addr_str = "0.0.0.0";
};

struct endpoint {
    endpoint() : port_(0) {}
    endpoint(const struct address& addr, uint16_t port) : addr_(addr), port_(port) {}
    struct address address() const { return addr_; }
    uint16_t port() const { return port_; }
    int protocol() const { return AF_INET; }

    struct address addr_;
    uint16_t port_;
};

class tcp {
public:
    typedef endpoint endpoint;

    class socket {
    public:
        socket(io_context& ctx) : io_ctx_(ctx), sockfd_(-1) {}

        socket(socket&& other) : io_ctx_(other.io_ctx_), sockfd_(other.sockfd_), remote_ep_(other.remote_ep_) {
            other.sockfd_ = -1;  // Transfer ownership
        }

        ~socket() {
            if (sockfd_ != -1) {
                ::close(sockfd_);
            }
        }

        endpoint remote_endpoint() const { return remote_ep_; }

        void close(boost::system::error_code& ec) {
            if (sockfd_ != -1) {
                ::close(sockfd_);
                sockfd_ = -1;
            }
            ec = boost::system::error_code();
        }

        template<typename T>
        void set_option(T opt, boost::system::error_code& ec) {
            // For tcp::no_delay
            if (sockfd_ != -1) {
                int flag = 1;
                setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
            }
            ec = boost::system::error_code();
        }

        // Internal methods for acceptor to use
        void set_socket_fd(int fd, const endpoint& ep) {
            sockfd_ = fd;
            remote_ep_ = ep;
        }

    private:
        io_context& io_ctx_;
        int sockfd_;
        endpoint remote_ep_;
    };

    class acceptor {
    public:
        acceptor(io_context& ctx) : io_ctx_(ctx), sockfd_(-1), listening_(false) {}

        ~acceptor() {
            if (sockfd_ != -1) {
                ::close(sockfd_);
            }
        }

        void open(int protocol) {
            sockfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd_ == -1) {
                std::cerr << "[TCP Mock] Failed to create socket" << std::endl;
            }
        }

        template<typename T>
        void set_option(T opt) {
            if (sockfd_ != -1) {
                int reuse = 1;
                setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
            }
        }

        void bind(const endpoint& ep) {
            if (sockfd_ == -1) return;

            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(ep.port());

            if (ep.address().to_string() == "0.0.0.0") {
                addr.sin_addr.s_addr = INADDR_ANY;
            } else {
                inet_pton(AF_INET, ep.address().to_string().c_str(), &addr.sin_addr);
            }

            if (::bind(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
                std::cerr << "[TCP Mock] Failed to bind to port " << ep.port() << std::endl;
                return;
            }

            bound_port_ = ep.port();
            std::cout << "[TCP Mock] Successfully bound to port " << bound_port_ << std::endl;
        }

        void listen() {
            if (sockfd_ == -1) return;

            if (::listen(sockfd_, 10) == -1) {
                std::cerr << "[TCP Mock] Failed to listen" << std::endl;
                return;
            }

            listening_ = true;
            std::cout << "[TCP Mock] Listening for connections on port " << bound_port_ << std::endl;
        }

        void close(boost::system::error_code& ec) {
            if (sockfd_ != -1) {
                ::close(sockfd_);
                sockfd_ = -1;
                listening_ = false;
            }
            ec = boost::system::error_code();
        }

        template<typename S, typename F>
        void async_accept(S& new_socket, F callback) {
            if (!listening_ || sockfd_ == -1) {
                callback(boost::system::make_error_code(boost::system::errc::operation_not_supported));
                return;
            }

            // Start accepting in a separate thread
            std::thread([this, &new_socket, callback]() {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);

                std::cout << "[TCP Mock] Waiting for connection..." << std::endl;

                int client_fd = ::accept(sockfd_, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) {
                    std::cerr << "[TCP Mock] Accept failed" << std::endl;
                    callback(boost::system::make_error_code(boost::system::errc::connection_aborted));
                    return;
                }

                // Create endpoint for remote client
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                endpoint remote_ep(address::from_string(client_ip), ntohs(client_addr.sin_port));

                // Set up the socket
                new_socket.set_socket_fd(client_fd, remote_ep);

                std::cout << "[TCP Mock] Accepted connection from " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;

                // Success callback
                callback(boost::system::error_code());
            }).detach();
        }

    private:
        io_context& io_ctx_;
        int sockfd_;
        uint16_t bound_port_;
        bool listening_;
    };

    struct no_delay {
        no_delay(bool enable) : value(enable) {}
        bool value;
    };
};

}}} // namespace boost::asio::ip
