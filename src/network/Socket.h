#pragma once
#ifndef SOCKET_H
#define SOCKET_H

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
using ssize_t = SSIZE_T;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
constexpr int kSocketError = SOCKET_ERROR;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> // Ajout pour getaddrinfo / getnameinfo
#include <unistd.h>
#include <fcntl.h>
using socket_t = int;
using ssize_t = ::ssize_t;
constexpr socket_t kInvalidSocket = -1;
constexpr int kSocketError = -1;
#endif

#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

// Encapsulation RAII de la socket
class Socket {
public:
    Socket() noexcept : handle_(kInvalidSocket) {}
    ~Socket() { close(); }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept : handle_(other.release()) {}
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.release();
        }
        return *this;
    }

    bool initialize(uint16_t port);
    void close();
    bool send_packet(const uint8_t *data, size_t size, const std::string &dest_ip, uint16_t dest_port);
    int receive_packet(uint8_t *buffer, size_t max_size, std::string &out_sender_ip, uint16_t &out_sender_port);

    [[nodiscard]] bool is_valid() const noexcept { return handle_ != kInvalidSocket; }
    socket_t release() noexcept {
        socket_t tmp = handle_;
        handle_ = kInvalidSocket;
        return tmp;
    }

    // Résout n'importe quelle adresse (DNS, IPv4, IPv6) en structure Dual-Stack (sockaddr_in6)
    static bool resolve_address(const std::string &host, uint16_t port, sockaddr_storage &out_addr, socklen_t &out_len);

private:
    socket_t handle_{kInvalidSocket};
    void _set_non_blocking();
    void close_socket(socket_t s);
};

#endif // SOCKET_H