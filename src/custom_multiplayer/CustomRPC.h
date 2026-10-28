//
// Created by leoul on 2026-10-26.
//
#pragma once
#ifndef CUSTOMRPC_H
#define CUSTOMRPC_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/stream_peer_buffer.hpp>

namespace godot {

    class CustomRPC : public Node {
        GDCLASS(CustomRPC, Node)

    private:
        // Identifiant du type de paquet pour les RPC
        const uint8_t PACKET_TYPE_RPC = 1;

        // Fonction interne pour exécuter dynamiquement la méthode
        void _execute_rpc(int sender_id, const String &node_path, const String &method_name, const Array &args);

    protected:
        static void _bind_methods();

    public:
        CustomRPC();
        ~CustomRPC();

        // Méthodes publiques de l'API (SÉRIALISATION)
        void send_rpc(int target_peer_id, Node *target_node, const String &method_name, const Array &args);
        void broadcast_rpc(Node *target_node, const String &method_name, const Array &args, int exclude_peer = 0);

        // Fonction connectée au signal du NetSocketManager (ROUTAGE)
        void _on_raw_packet_received(int sender_id, const PackedByteArray &payload, bool is_reliable);
    };
} // godot

#endif //CUSTOMRPC_H
