//
// Created by leoul on 2026-10-14.
//
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WS2tcpip.h>
#endif

#include "QuicClient.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/time.hpp>

#include "ngtcp2/ngtcp2_crypto_boringssl.h"
#include "openssl/rand.h"

using namespace godot;


void QuicClient::_bind_methods() {
    ClassDB::bind_method(D_METHOD("connect_to_host", "ip", "port"), &QuicClient::connect_to_host);
    ClassDB::bind_method(D_METHOD("disconnect_from_host"), &QuicClient::disconnect_from_host);
    ClassDB::bind_method(D_METHOD("send_packet", "channel_id", "data", "reliable"), &QuicClient::send_packet);

    ADD_SIGNAL(MethodInfo("connected"));
    ADD_SIGNAL(MethodInfo("disconnected"));
    ADD_SIGNAL(MethodInfo("packet_received",
                          PropertyInfo(Variant::INT, "channel_id"),
                          PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data"),
                          PropertyInfo(Variant::BOOL, "is_reliable")));
}


QuicClient::QuicClient() {
    // debug C++ sous Windows
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
#endif
}

QuicClient::~QuicClient() {
    disconnect_from_host();
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = nullptr;
    }
}

bool QuicClient::init_ssl_ctx() {
    if (ssl_ctx) return true;

    ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx) return false;

    SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_3_VERSION);
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, nullptr);
    SSL_CTX_set1_curves_list(ssl_ctx, "X25519:P-256");

    if (ngtcp2_crypto_boringssl_configure_client_context(ssl_ctx) != 0) {
        UtilityFunctions::printerr("[QuicClient] Impossible de configurer le contexte QUIC BoringSSL.");
        return false;
    }
    return true;
}

void QuicClient::on_transmit(const uint8_t *data, size_t len, const ngtcp2_path &path) {
    if (socket.is_valid() && !target_ip.empty()) {
        char ip_str[NI_MAXHOST];
        char port_str[NI_MAXSERV];

        // Traduit le pointeur polymorphe (IPv4 ou IPv6) en chaîne de caractères
        if (getnameinfo(reinterpret_cast<const sockaddr*>(path.remote.addr),
                        static_cast<socklen_t>(path.remote.addrlen),
                        ip_str, sizeof(ip_str), port_str, sizeof(port_str),
                        NI_NUMERICHOST | NI_NUMERICSERV) == 0) {

            uint16_t port = static_cast<uint16_t>(std::stoi(port_str));
            socket.send_packet(data, len, std::string(ip_str), port);
                        }
    }
}

void QuicClient::on_packet_received(int channel_id, const uint8_t *data, size_t len, bool reliable) {
    PackedByteArray arr;
    arr.resize(len);
    memcpy(arr.ptrw(), data, len);

    // Remonte l'information au script GDScript de l'utilisateur
    emit_signal("packet_received", channel_id, arr, reliable);
}

void QuicClient::on_handshake_completed() {
    UtilityFunctions::print("[QuicClient] Connecté au serveur !");
    emit_signal("connected");
}

bool QuicClient::connect_to_host(const String &ip, int port) {
    target_ip = ip.utf8().get_data();
    target_port = static_cast<uint16_t>(port);

    if (!socket.initialize(0)) {
        UtilityFunctions::printerr("[QuicClient] Impossible d'initialiser le socket local.");
        return false;
    }

    if (!init_ssl_ctx()) return false;

    // Génération des IDs de connexion
    ngtcp2_cid dcid, scid;
    dcid.datalen = 16;
    scid.datalen = 16;
    RAND_bytes(dcid.data, dcid.datalen);
    RAND_bytes(scid.data, scid.datalen);

    // Configuration des adresses
    sockaddr_storage remote_addr{}, local_addr{};
    socklen_t remote_len = 0, local_len = 0;

    if (!Socket::resolve_address(target_ip, target_port, remote_addr, remote_len) ||
        !Socket::resolve_address("::", 0, local_addr, local_len)) {
        UtilityFunctions::printerr("[QuicClient] Résolution impossible de la cible.");
        return false;
        }

    ngtcp2_path path;
    ngtcp2_addr_init(&path.local, reinterpret_cast<const sockaddr*>(&local_addr), local_len);
    ngtcp2_addr_init(&path.remote, reinterpret_cast<const sockaddr*>(&remote_addr), remote_len);

    ngtcp2_tstamp ts = static_cast<ngtcp2_tstamp>(Time::get_singleton()->get_ticks_usec()) * 1000;
    connection = std::make_shared<QuicConnection>(
        false,
        ssl_ctx,
        dcid,
        scid,
        dcid,
        path,
        ts,
        [this](const uint8_t *data, size_t len, const ngtcp2_path &path) { this->on_transmit(data, len, path); },
        [this](int channel_id, const uint8_t *data, size_t len, bool reliable) { this->on_packet_received(channel_id, data, len, reliable); },
        [this]() { this->on_handshake_completed(); }
    );

    UtilityFunctions::print("[QuicClient] Lancement du Handshake...");
    connection->write_packets(ts);
    return true;
}

void QuicClient::disconnect_from_host() {
    if (connection) {
        if (!connection->is_closed()) {
            ngtcp2_tstamp ts = Time::get_singleton()->get_ticks_usec() * 1000;
            connection->close_connection(ts);
        }
        connection.reset();
        socket.close();
        emit_signal("disconnected");
        UtilityFunctions::print("[QuicClient] Déconnecté.");
    }
}

void QuicClient::send_packet(int channel_id, const PackedByteArray &data, bool reliable) {
    if (connection && !connection->is_closed()) {
        ngtcp2_tstamp ts = static_cast<ngtcp2_tstamp>(Time::get_singleton()->get_ticks_usec()) * 1000;
        connection->send_packet(channel_id, data.ptr(), data.size(), reliable, ts);
        connection->write_packets(ts);
    }
}

void QuicClient::_process(double delta) {
    if (!socket.is_valid() || !connection) return;

    uint8_t buffer[65536];
    std::string sender_ip;
    uint16_t sender_port = 0;
    ngtcp2_tstamp ts = static_cast<ngtcp2_tstamp>(Time::get_singleton()->get_ticks_usec()) * 1000;

    // Dépilage complet des paquets UDP entrants
    while (true) {
        int bytes_received = socket.receive_packet(buffer, sizeof(buffer), sender_ip, sender_port);
        if (bytes_received <= 0) break;

        ts = static_cast<ngtcp2_tstamp>(Time::get_singleton()->get_ticks_usec()) * 1000;

        struct addrinfo hints{}, *client_res = nullptr, *local_res = nullptr;
        sockaddr_storage client_addr{}, local_addr{};
        socklen_t client_len = 0, local_len = 0;

        if (!Socket::resolve_address(sender_ip, sender_port, client_addr, client_len) ||
            !Socket::resolve_address("::", 0, local_addr, local_len)) {
            continue;
            }

        ngtcp2_path path;
        ngtcp2_addr_init(&path.local, reinterpret_cast<const sockaddr*>(&local_addr), local_len);
        ngtcp2_addr_init(&path.remote, reinterpret_cast<const sockaddr*>(&client_addr), client_len);

        // On injecte les octets aveuglément dans la machine à états
        connection->process_datagram(path, buffer, bytes_received, ts);
    }

    if (connection->is_closed()) {
        disconnect_from_host();
        return;
    }

    ts = static_cast<ngtcp2_tstamp>(Time::get_singleton()->get_ticks_usec()) * 1000;

    // Gestion des retransmissions et génération des accusés de réception
    connection->handle_expiry(ts);

    // Si des choses ont bougé ou si l'horloge a expiré, on écrit sur le réseau
    connection->write_packets(ts);
}