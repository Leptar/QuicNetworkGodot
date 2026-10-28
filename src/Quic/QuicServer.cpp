//
// Created by leoul on 2026-10-14.
//

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WS2tcpip.h>
#endif

#include "QuicServer.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <vector>

#include "ngtcp2/ngtcp2_crypto_boringssl.h"
#include "openssl/rand.h"

using namespace godot;

void QuicServer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start", "port", "cert_path", "key_path"), &QuicServer::start);
    ClassDB::bind_method(D_METHOD("stop"), &QuicServer::stop);
    ClassDB::bind_method(D_METHOD("send_packet", "peer_id", "channel_id", "data", "reliable"), &QuicServer::send_packet);
    ClassDB::bind_method(D_METHOD("broadcast_packet", "channel_id", "data", "reliable"), &QuicServer::broadcast_packet);

    ADD_SIGNAL(MethodInfo("peer_connected", PropertyInfo(Variant::INT, "peer_id")));
    ADD_SIGNAL(MethodInfo("peer_disconnected", PropertyInfo(Variant::INT, "peer_id")));
    ADD_SIGNAL(MethodInfo("packet_received",
                          PropertyInfo(Variant::INT, "peer_id"),
                          PropertyInfo(Variant::INT, "channel_id"),
                          PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data"),
                          PropertyInfo(Variant::BOOL, "is_reliable")));
}

QuicServer::QuicServer() {
#if defined(_WIN32)
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
#endif
}

QuicServer::~QuicServer() {
    stop();
}


void QuicServer::on_transmit(const uint8_t *data, size_t len, const ngtcp2_path &path) {
    if (socket.is_valid()) {
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

void QuicServer::on_packet_received(int peer_id, int channel_id, const uint8_t *data, size_t len, bool reliable) {
    PackedByteArray arr;
    arr.resize(len);
    memcpy(arr.ptrw(), data, len);

    emit_signal("packet_received", peer_id, channel_id, arr, reliable);
}

void QuicServer::on_handshake_completed(int peer_id) {
    UtilityFunctions::print("[QuicServer] Joueur ", peer_id, " connecte !");
    emit_signal("peer_connected", peer_id);
}

static int alpn_select_cb(SSL *ssl, const uint8_t **out, uint8_t *outlen, const uint8_t *in, unsigned int inlen, void *arg) {
    static const uint8_t alpn_proto[] = "godot-quic";
    for (unsigned int i = 0; i < inlen;) {
        uint8_t len = in[i++];
        if (len == sizeof(alpn_proto) - 1 && memcmp(&in[i], alpn_proto, len) == 0) {
            *out = &in[i];
            *outlen = len;
            return SSL_TLSEXT_ERR_OK;
        }
        i += len;
    }
    return SSL_TLSEXT_ERR_NOACK;
}

bool QuicServer::init_ssl_ctx(const String &cert_path, const String &key_path) {
    if (ssl_ctx) return true;

    ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) return false;

    SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_3_VERSION);
    SSL_CTX_set1_curves_list(ssl_ctx, "X25519:P-256");

    if (ngtcp2_crypto_boringssl_configure_server_context(ssl_ctx) != 0) return false;

    SSL_CTX_set_alpn_select_cb(ssl_ctx, alpn_select_cb, nullptr);

    String global_cert = ProjectSettings::get_singleton()->globalize_path(cert_path);
    String global_key = ProjectSettings::get_singleton()->globalize_path(key_path);

    if (SSL_CTX_use_certificate_chain_file(ssl_ctx, global_cert.utf8().get_data()) != 1 ||
        SSL_CTX_use_PrivateKey_file(ssl_ctx, global_key.utf8().get_data(), SSL_FILETYPE_PEM) != 1) {
        UtilityFunctions::printerr("[QuicServer] Erreur chargement certificats.");
        return false;
    }
    return true;
}

bool QuicServer::extract_dcid(const uint8_t *data, size_t datalen, ngtcp2_cid &dest_cid) {
    ngtcp2_version_cid vcid;

    // 16 = taille attendue pour nos CIDs
    int rv = ngtcp2_pkt_decode_version_cid(&vcid, data, datalen, 16);

    if (rv == 0) {
        ngtcp2_cid_init(&dest_cid, vcid.dcid, vcid.dcidlen);
        return true;
    }
    return false;
}

void QuicServer::start(int port, const String &cert_path, const String &key_path) {
    local_port = static_cast<uint16_t>(port);
    next_peer_id = 2;

    if (!socket.initialize(port)) {
        UtilityFunctions::printerr("[QuicServer] Impossible de bind le port ", port);
        return;
    }

    if (!init_ssl_ctx(cert_path, key_path)) return;

    UtilityFunctions::print("[QuicServer] En ecoute sur le port UDP ", port);
}

void QuicServer::stop() {
    if (socket.is_valid()) {
        ngtcp2_tstamp ts = static_cast<ngtcp2_tstamp>(Time::get_singleton()->get_ticks_usec()) * 1000;
        for (auto &pair : peers) {
            if (pair.second && !pair.second->is_closed()) {
                pair.second->close_connection(ts);
            }
        }
    }

    peers.clear();
    cid_to_peer.clear();
    socket.close();
}

void QuicServer::remove_peer(int peer_id) {
    if (peers.find(peer_id) != peers.end()) {
        peers.erase(peer_id);

        // Nettoyage de la table de routage CID
        for (auto it = cid_to_peer.begin(); it != cid_to_peer.end();) {
            if (it->second == peer_id) {
                it = cid_to_peer.erase(it);
            } else {
                ++it;
            }
        }

        emit_signal("peer_disconnected", peer_id);
        UtilityFunctions::print("[QuicServer] Joueur ", peer_id, " supprime.");
    }
}

void QuicServer::send_packet(int peer_id, int channel_id, const PackedByteArray &data, bool reliable) {
    auto it = peers.find(peer_id);
    if (it != peers.end() && !it->second->is_closed()) {
        ngtcp2_tstamp ts = static_cast<ngtcp2_tstamp>(Time::get_singleton()->get_ticks_usec()) * 1000;
        it->second->send_packet(channel_id, data.ptr(), data.size(), reliable, ts);
        it->second->write_packets(ts);
    }
}

void QuicServer::broadcast_packet(int channel_id, const PackedByteArray &data, bool reliable) {
    ngtcp2_tstamp ts = static_cast<ngtcp2_tstamp>(Time::get_singleton()->get_ticks_usec()) * 1000;
    for (auto &pair : peers) {
        if (!pair.second->is_closed()) {
            pair.second->send_packet(channel_id, data.ptr(), data.size(), reliable, ts);
            pair.second->write_packets(ts);
        }
    }
}

void QuicServer::_process(double delta) {
    if (!socket.is_valid()) return;

    uint8_t buffer[65536];
    std::string sender_ip;
    uint16_t sender_port = 0;
    ngtcp2_tstamp ts = static_cast<ngtcp2_tstamp>(Time::get_singleton()->get_ticks_usec()) * 1000;

    while (true) {
        int bytes_received = socket.receive_packet(buffer, sizeof(buffer), sender_ip, sender_port);
        if (bytes_received <= 0) break;

        ts = static_cast<ngtcp2_tstamp>(Time::get_singleton()->get_ticks_usec()) * 1000;

        ngtcp2_cid dcid;
        if (!extract_dcid(buffer, bytes_received, dcid)) {
            continue; // Paquet corrompu ou illisible
        }

        sockaddr_storage client_addr{}, local_addr{};
        socklen_t client_len = 0, local_len = 0;

        if (!Socket::resolve_address(sender_ip, sender_port, client_addr, client_len) ||
            !Socket::resolve_address("::", local_port, local_addr, local_len)) {
            continue;
            }

        ngtcp2_path path;
        ngtcp2_addr_init(&path.local, reinterpret_cast<const sockaddr*>(&local_addr), local_len);
        ngtcp2_addr_init(&path.remote, reinterpret_cast<const sockaddr*>(&client_addr), client_len);

        std::string dcid_str(reinterpret_cast<const char*>(dcid.data), dcid.datalen);
        auto it = cid_to_peer.find(dcid_str);

        if (it != cid_to_peer.end()) {
            // cas 1 : joueur existe
            int peer_id = it->second;
            peers[peer_id]->process_datagram(path, buffer, bytes_received, ts);
        } else {
            // cas 2 : nouveau joueur potentiel : check paquet Initial valide
            ngtcp2_pkt_hd hd;
            if (ngtcp2_accept(&hd, buffer, bytes_received) == 0) {
                int new_peer_id = next_peer_id++;

                UtilityFunctions::print("[QuicServer] Creation de session pour le Joueur ", new_peer_id);

                ngtcp2_cid scid; // L'ID du serveur pour ce joueur
                scid.datalen = 16;
                RAND_bytes(scid.data, scid.datalen);

                auto new_conn = std::make_shared<QuicConnection>(
                    true, ssl_ctx, hd.scid, scid, hd.dcid, path, ts,
                    [this](const uint8_t *d, size_t l, const ngtcp2_path &p) { this->on_transmit(d, l, p); },
                    [this, new_peer_id](int ch_id, const uint8_t *d, size_t l, bool r) { this->on_packet_received(new_peer_id, ch_id, d, l, r); },
                    [this, new_peer_id]() { this->on_handshake_completed(new_peer_id); }
                );

                peers[new_peer_id] = new_conn;

                // On enregistre les deux CIDs initiaux dans la table de routage
                std::string original_dcid_str(reinterpret_cast<const char*>(hd.dcid.data), hd.dcid.datalen);
                std::string server_scid_str(reinterpret_cast<const char*>(scid.data), scid.datalen);
                cid_to_peer[original_dcid_str] = new_peer_id;
                cid_to_peer[server_scid_str] = new_peer_id;

                new_conn->process_datagram(path, buffer, bytes_received, ts);
            }
        }
    }

    ts = static_cast<ngtcp2_tstamp>(Time::get_singleton()->get_ticks_usec()) * 1000;

    // Maintenance et timeout
    std::vector<int> dead_peers;
    for (auto &pair : peers) {
        int peer_id = pair.first;
        auto &conn = pair.second;

        if (conn->is_closed()) {
            dead_peers.push_back(peer_id);
            continue;
        }

        conn->handle_expiry(ts);
        conn->write_packets(ts);
    }

    // nettoyage
    for (int peer_id : dead_peers) {
        remove_peer(peer_id);
    }
}
