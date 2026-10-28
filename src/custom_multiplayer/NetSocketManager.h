//
// Created by leoul on 2026-10-26.
//

#pragma once

#ifndef NET_SOCKET_MANAGER_H
#define NET_SOCKET_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

// Inclusion de tes classes QUIC C++
#include "../Quic/QuicServer.h"
#include "../Quic/QuicClient.h"

namespace godot {

    class NetSocketManager : public Node {
        GDCLASS(NetSocketManager, Node)

    private:
        bool is_server = false;
        int local_peer_id = 1;
        Dictionary peers;

        QuicServer *quic_server = nullptr;
        QuicClient *quic_client = nullptr;

    protected:
        static void _bind_methods();

    public:
        NetSocketManager();
        ~NetSocketManager();

        // Accesseurs pour l'état du nœud
        bool get_is_server() const;
        int get_local_peer_id() const;
        Dictionary get_peers() const;

        // Gestion de connexion
        void start_server(int port = 7070);
        bool join_server(const String &ip, int port = 7070);
        void leave_server();

        // Interface d'envoi
        void send_packet(int peer_id, const PackedByteArray &data, bool reliable);

        // Callbacks Serveur
        void _on_server_peer_connected(int peer_id);
        void _on_server_peer_disconnected(int peer_id);
        void _on_server_packet_received(int peer_id, int channel_id, const PackedByteArray &data, bool reliable);

        // Callbacks Client
        void _on_client_connected();
        void _on_client_disconnected();
        void _on_client_packet_received(int channel_id, const PackedByteArray &data, bool reliable);
    };

} // namespace godot

#endif // NET_SOCKET_MANAGER_H
