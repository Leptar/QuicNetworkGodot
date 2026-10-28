//
// Created by leoul on 2026-10-14.
//
#pragma once

#ifndef NETWORK_RAN_QUICCLIENT_H
#define NETWORK_RAN_QUICCLIENT_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <memory>
#include <string>

#include "QuicConnection.h"
#include "../network/Socket.h"

namespace godot {
    class QuicClient : public Node {
        GDCLASS(QuicClient, Node)

    private:
        Socket socket;
        std::string target_ip;
        uint16_t target_port = 0;

        // Le contexte TLS client réutilisable
        SSL_CTX *ssl_ctx = nullptr;

        std::shared_ptr<QuicConnection> connection;

        bool init_ssl_ctx();

        // Les fonctions qui seront passées en délégués à QuicConnection
        void on_transmit(const uint8_t *data, size_t len, const ngtcp2_path &path);
        void on_packet_received(int channel_id, const uint8_t *data, size_t len, bool reliable);
        void on_handshake_completed();

    protected:
        static void _bind_methods();

    public:
        QuicClient();
        ~QuicClient();

        // API publique simplifiée pour le GDScript
        bool connect_to_host(const String &ip, int port);
        void disconnect_from_host();
        void send_packet(int channel_id, const PackedByteArray &data, bool reliable);

        void _process(double delta) override;
    };
} // godot

#endif //NETWORK_RAN_QUICCLIENT_H
