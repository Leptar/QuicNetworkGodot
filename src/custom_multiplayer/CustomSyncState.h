//
// Created by leoul on 2026-10-26.
//

#pragma once

#ifndef CUSTOM_SYNC_STATE_H
#define CUSTOM_SYNC_STATE_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

namespace godot {

    class CustomSyncState : public Node {
        GDCLASS(CustomSyncState, Node)

    private:
        // Identifiant du type de paquet pour la synchronisation
        const uint8_t PACKET_TYPE_SYNC = 2;

        // Fonction interne pour appliquer les propriétés dynamiquement
        void _apply_sync_state(const String &parent_path, const Dictionary &state_data);

    protected:
        static void _bind_methods();

    public:
        CustomSyncState();
        ~CustomSyncState();

        void send_sync_state(int target_peer_id, Node *target_node, const Dictionary &state_data);
        void broadcast_sync_state(Node *target_node, const Dictionary &state_data, int exclude_peer = 0);

        // Écoute des paquets bruts
        void _on_raw_packet_received(int sender_id, const PackedByteArray &payload, bool is_reliable);
    };

} // namespace godot

#endif // CUSTOM_SYNC_STATE_H
