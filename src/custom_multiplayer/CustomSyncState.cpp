//
// Created by leoul on 2026-10-26.
//

#include "CustomSyncState.h"
#include <godot_cpp/classes/stream_peer_buffer.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "godot_cpp/classes/engine.hpp"

using namespace godot;

void CustomSyncState::_bind_methods() {
    ClassDB::bind_method(D_METHOD("send_sync_state", "target_peer_id", "target_node", "state_data"), &CustomSyncState::send_sync_state);
    ClassDB::bind_method(D_METHOD("broadcast_sync_state", "target_node", "state_data", "exclude_peer"), &CustomSyncState::broadcast_sync_state, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("_on_raw_packet_received", "sender_id", "payload", "is_reliable"), &CustomSyncState::_on_raw_packet_received);
}

CustomSyncState::CustomSyncState() = default;
CustomSyncState::~CustomSyncState() = default;

void CustomSyncState::send_sync_state(int target_peer_id, Node *target_node, const Dictionary &state_data) {
    if (!target_node) return;

    Ref<StreamPeerBuffer> buffer;
    buffer.instantiate();

    buffer->put_u8(PACKET_TYPE_SYNC);
    buffer->put_var(target_node->get_path());
    buffer->put_var(state_data);

    // Envoi NON-FIABLE (is_reliable = false)
    Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
    if (net_manager) {
        net_manager->call("send_packet", target_peer_id, buffer->get_data_array(), false);
    }
}

void CustomSyncState::broadcast_sync_state(Node *target_node, const Dictionary &state_data, int exclude_peer) {
    Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
    if (!net_manager) return;

    bool is_server = net_manager->get("is_server");

    // Si on est serveur, on diffuse à tous sauf l'exclu. Sinon, on envoie au serveur (ID 1)
    if (is_server) {
        Dictionary peers = net_manager->get("peers");
        Array keys = peers.keys();
        for (int i = 0; i < keys.size(); i++) {
            int peer_id = keys[i];
            if (peer_id != exclude_peer) {
                send_sync_state(peer_id, target_node, state_data);
            }
        }
    } else {
        send_sync_state(1, target_node, state_data);
    }
}

void CustomSyncState::_on_raw_packet_received(int sender_id, const PackedByteArray &payload, bool is_reliable) {
    // Le SyncState ignore les paquets TCP/fiables
    if (is_reliable) return;

    Ref<StreamPeerBuffer> buffer;
    buffer.instantiate();
    buffer->set_data_array(payload);

    if (buffer->get_u8() == PACKET_TYPE_SYNC) {
        String node_path = buffer->get_var();
        Dictionary state_data = buffer->get_var();

        _apply_sync_state(node_path, state_data);

        // Si le serveur reçoit un état, il le relaye aux autres clients
        Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
        if (net_manager && (bool)net_manager->get("is_server")) {
            SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
            if (tree) {
                Node *target_node = tree->get_root()->get_node_or_null(NodePath(node_path));
                if (target_node) {
                    broadcast_sync_state(target_node, state_data, sender_id);
                }
            }
        }
    }
}

void CustomSyncState::_apply_sync_state(const String &parent_path, const Dictionary &state_data) {
    SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (!tree) return;

    Node *parent_node = tree->get_root()->get_node_or_null(NodePath(parent_path));
    if (!parent_node) return;

    Array keys = state_data.keys();
    for (int i = 0; i < keys.size(); i++) {
        String path_str = keys[i];
        String node_path = ".";
        String prop = path_str;

        // Gestion de la notation Noeud:Propriété
        if (path_str.contains(":")) {
            PackedStringArray parts = path_str.split(":");
            node_path = parts[0];
            prop = parts[1];
        }

        Node *target = parent_node->get_node_or_null(NodePath(node_path));
        if (target) {
            target->set(prop, state_data[path_str]);
        }
    }
}