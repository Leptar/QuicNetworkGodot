//
// Created by leoul on 2026-10-14.
//
#pragma once

#ifndef NETWORK_RAN_QUICSERVER_H
#define NETWORK_RAN_QUICSERVER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <unordered_map>
#include <memory>
#include <string>

#include "QuicConnection.h"
#include "../network/Socket.h"

namespace godot {
    class QuicServer : public Node {
        GDCLASS(QuicServer, Node)

    private:
        Socket socket;
        uint16_t local_port = 0;

        SSL_CTX *ssl_ctx = nullptr;

        // générateur d'identifiants Godot
        int next_peer_id = 2;

        // Les deux tables de routage croisées
        // API Godot : Peer ID -> Machine à états QUIC
        std::unordered_map<int, std::shared_ptr<QuicConnection>> peers;

        // réseau UDP : Connection ID (converti en std::string) -> Peer ID
        std::unordered_map<std::string, int> cid_to_peer;

        bool init_ssl_ctx(const String &cert_path, const String &key_path);

        bool extract_dcid(const uint8_t *data, size_t datalen, ngtcp2_cid &dest_cid);

        void on_transmit(const uint8_t *data, size_t len, const ngtcp2_path &path);
        void on_packet_received(int peer_id, int channel_id, const uint8_t *data, size_t len, bool reliable);
        void on_handshake_completed(int peer_id);

        // si un joueur timeout
        void remove_peer(int peer_id);

    protected:
        static void _bind_methods();

    public:
        QuicServer();
        ~QuicServer();

        // API Publique GDScript
        void start(int port, const String &cert_path, const String &key_path);
        void stop();
        void send_packet(int peer_id, int channel_id, const PackedByteArray &data, bool reliable);

        void broadcast_packet(int channel_id, const PackedByteArray &data, bool reliable);

        void _process(double delta) override;
    };
} // godot

#endif //NETWORK_RAN_QUICSERVER_H
