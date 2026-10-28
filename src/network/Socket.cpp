#include "Socket.h"
#include <cstring>
#include <iostream>

void Socket::close_socket(socket_t s) {
#if defined(_WIN32)
    ::closesocket(s);
#else
    ::close(s);
#endif
}

void Socket::_set_non_blocking() {
    if (!is_valid()) return;
#if defined(_WIN32)
    u_long mode = 1;
    if (ioctlsocket(handle_, FIONBIO, &mode) != 0) {
        std::cerr << "[Socket] Erreur activation mode non-bloquant : " << WSAGetLastError() << std::endl;
        close();
    }
#else
    int flags = fcntl(handle_, F_GETFL, 0);
    if (flags == -1) flags = 0;
    if (fcntl(handle_, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "[Socket] Erreur activation mode non-bloquant" << std::endl;
        close();
    }
#endif
}

bool Socket::initialize(uint16_t port) {
    handle_ = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (!is_valid()) {
        std::cerr << "[Socket] Erreur creation socket" << std::endl;
        return false;
    }

    // active le Dual-Stack (accepte IPv4 et IPv6)
    int no = 0;
    if (setsockopt(handle_, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&no), sizeof(no)) == kSocketError) {
        std::cerr << "[Socket] Avertissement: Impossible d'activer le mode Dual-Stack" << std::endl;
    }

    // Protection anti-crash pour macOS et iOS
#if defined(__APPLE__)
    int nosigpipe = 1;
    setsockopt(handle_, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const void*>(&nosigpipe), sizeof(nosigpipe));
#endif

    _set_non_blocking();

    // Bind sur in6addr_any (Écoute toutes les interfaces en IPv4 et IPv6)
    sockaddr_in6 server_addr{};
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any;
    server_addr.sin6_port = htons(port);

    if (bind(handle_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) == kSocketError) {
        std::cerr << "[Socket] Erreur bind sur le port " << port << std::endl;
        close();
        return false;
    }

    std::cout << "[Socket] Initialise avec succes (Dual-Stack IPv4/IPv6) sur le port " << port << std::endl;
    return true;
}

void Socket::close() {
    if (is_valid()) {
        close_socket(handle_);
        handle_ = kInvalidSocket;
    }
}

bool Socket::send_packet(const uint8_t *data, size_t size, const std::string &dest_ip, uint16_t dest_port) {
    if (!is_valid()) return false;

    // getaddrinfo : gere IPs et les noms de domaine
    sockaddr_storage dest_addr{};
    socklen_t dest_len = 0;
    if (!resolve_address(dest_ip, dest_port, dest_addr, dest_len)) {
        return false;
    }

    int bytes_sent = sendto(handle_, reinterpret_cast<const char*>(data),
                            static_cast<int>(size), 0,
                            reinterpret_cast<struct sockaddr*>(&dest_addr), dest_len);

    if (bytes_sent == kSocketError) {
#if defined(_WIN32)
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) std::cerr << "[Socket] Erreur sendto : " << err << std::endl;
#else
        if (errno != EWOULDBLOCK && errno != EAGAIN) std::cerr << "[Socket] Erreur sendto : " << errno << std::endl;
#endif
        return false;
    }
    return true;
}

int Socket::receive_packet(uint8_t *buffer, size_t max_size, std::string &out_sender_ip, uint16_t &out_sender_port) {
    if (!is_valid()) return -1;

    // sockaddr_storage : Structure polymorphe assez grande pour contenir une IPv4 ou une IPv6
    sockaddr_storage sender_addr{};

#if defined(_WIN32)
    int sender_addr_size = sizeof(sender_addr);
#else
    socklen_t sender_addr_size = sizeof(sender_addr);
#endif

    int bytes_received = recvfrom(handle_, reinterpret_cast<char*>(buffer),
                                  static_cast<int>(max_size), 0,
                                  reinterpret_cast<struct sockaddr*>(&sender_addr),
                                  &sender_addr_size);

    if (bytes_received == kSocketError) {
#if defined(_WIN32)
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return 0;
        std::cerr << "[Socket] Erreur recvfrom : " << err << std::endl;
#else
        if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
        std::cerr << "[Socket] Erreur recvfrom : " << errno << std::endl;
#endif
        return -1;
    }

    // getnameinfo : Extrait l'IP en texte (que ce soit une IPv4 ou IPv6) sans se soucier du format
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];

    if (getnameinfo(reinterpret_cast<struct sockaddr*>(&sender_addr), sender_addr_size,
                    host, sizeof(host), serv, sizeof(serv),
                    NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
        out_sender_ip = host;
        out_sender_port = static_cast<uint16_t>(std::stoi(serv));
    }

    return bytes_received;
}

bool Socket::resolve_address(const std::string &host, uint16_t port, sockaddr_storage &out_addr, socklen_t &out_len) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_V4MAPPED | AI_ALL;

    std::string port_str = std::to_string(port);

    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || res == nullptr) {
        std::cerr << "[Socket] Impossible de resoudre l'adresse : " << host << std::endl;
        return false;
    }

    memcpy(&out_addr, res->ai_addr, res->ai_addrlen);
    out_len = static_cast<socklen_t>(res->ai_addrlen);

    freeaddrinfo(res);
    return true;
}