#include "CustomSpawnManager.h"
#include <godot_cpp/classes/stream_peer_buffer.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include "godot_cpp/classes/engine.hpp"

using namespace godot;

void CustomSpawnManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_scene", "scene_id", "packed_scene"), &CustomSpawnManager::register_scene);
    
    ClassDB::bind_method(D_METHOD("send_spawn", "target_peer_id", "parent_node_path", "scene_id", "node_name", "initial_position"), &CustomSpawnManager::send_spawn, DEFVAL(Vector3(0, 0, 0)));
    ClassDB::bind_method(D_METHOD("broadcast_spawn", "parent_node_path", "scene_id", "node_name", "initial_position", "exclude_peer"), &CustomSpawnManager::broadcast_spawn, DEFVAL(Vector3(0, 0, 0)), DEFVAL(0));
    ClassDB::bind_method(D_METHOD("broadcast_despawn", "node_path_to_remove", "exclude_peer"), &CustomSpawnManager::broadcast_despawn, DEFVAL(0));
    
    ClassDB::bind_method(D_METHOD("_on_raw_packet_received", "sender_id", "payload", "is_reliable"), &CustomSpawnManager::_on_raw_packet_received);
}

CustomSpawnManager::CustomSpawnManager() = default;
CustomSpawnManager::~CustomSpawnManager() = default;

void CustomSpawnManager::register_scene(uint32_t scene_id, const Ref<PackedScene> &packed_scene) {
    if (spawnable_scenes.has(scene_id)) {
        UtilityFunctions::printerr("[CustomSpawnManager] Attention : La scène ID ", scene_id, " est déjà enregistrée.");
    }
    spawnable_scenes[scene_id] = packed_scene;
}

void CustomSpawnManager::send_spawn(int target_peer_id, const String &parent_node_path, uint32_t scene_id, const String &node_name, const Vector3 &initial_position) {
    Ref<StreamPeerBuffer> buffer;
    buffer.instantiate();
    
    buffer->put_u8(PACKET_TYPE_SPAWN);
    buffer->put_var(parent_node_path);
    buffer->put_u32(scene_id);
    buffer->put_var(node_name);
    buffer->put_var(initial_position);
    
    Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
    if (net_manager) {
        net_manager->call("send_packet", target_peer_id, buffer->get_data_array(), true);
    }
}

void CustomSpawnManager::broadcast_spawn(const String &parent_node_path, uint32_t scene_id, const String &node_name, const Vector3 &initial_position, int exclude_peer) {
    Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
    if (!net_manager) return;

    Dictionary peers = net_manager->get("peers");
    Array keys = peers.keys();
    
    for (int i = 0; i < keys.size(); i++) {
        int peer_id = keys[i];
        if (peer_id != exclude_peer) {
            send_spawn(peer_id, parent_node_path, scene_id, node_name, initial_position);
        }
    }
}

void CustomSpawnManager::broadcast_despawn(const String &node_path_to_remove, int exclude_peer) {
    Ref<StreamPeerBuffer> buffer;
    buffer.instantiate();
    
    buffer->put_u8(PACKET_TYPE_DESPAWN);
    buffer->put_var(node_path_to_remove);
    
    Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
    if (!net_manager) return;

    Dictionary peers = net_manager->get("peers");
    Array keys = peers.keys();
    
    for (int i = 0; i < keys.size(); i++) {
        int peer_id = keys[i];
        if (peer_id != exclude_peer) {
            net_manager->call("send_packet", peer_id, buffer->get_data_array(), true);
        }
    }
}

void CustomSpawnManager::_on_raw_packet_received(int sender_id, const PackedByteArray &payload, bool is_reliable) {
    if (!is_reliable) return;
        
    Ref<StreamPeerBuffer> buffer;
    buffer.instantiate();
    buffer->set_data_array(payload);
    
    uint8_t packet_type = buffer->get_u8();
    
    if (packet_type == PACKET_TYPE_SPAWN) {
        String parent_path = buffer->get_var();
        uint32_t scene_id = buffer->get_u32();
        String node_name = buffer->get_var();
        Vector3 position = buffer->get_var();
        
        _execute_spawn(parent_path, scene_id, node_name, position);
        
    } else if (packet_type == PACKET_TYPE_DESPAWN) {
        String node_path = buffer->get_var();
        _execute_despawn(node_path);
    }
}

void CustomSpawnManager::_execute_spawn(const String &parent_path, uint32_t scene_id, const String &node_name, const Vector3 &position) {
    SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (!tree) return;

    Node *parent_node = tree->get_root()->get_node_or_null(NodePath(parent_path));
    
    if (!parent_node) {
        UtilityFunctions::printerr("[CustomSpawnManager] Échec : Parent introuvable -> ", parent_path);
        return;
    }
        
    if (!spawnable_scenes.has(scene_id)) {
        UtilityFunctions::printerr("[CustomSpawnManager] Échec : ID de scène inconnu -> ", scene_id);
        return;
    }
        
    if (parent_node->has_node(NodePath(node_name))) {
        return; // Évite les doublons
    }
        
    Node *instance = spawnable_scenes[scene_id]->instantiate();
    instance->set_name(node_name);
    
    // Cast vers Node3D pour assigner la position
    Node3D *node3d = Object::cast_to<Node3D>(instance);
    if (node3d) {
        node3d->set_position(position);
    }
        
    parent_node->add_child(instance);
}

void CustomSpawnManager::_execute_despawn(const String &node_path) {
    SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (!tree) return;

    Node *node_to_remove = tree->get_root()->get_node_or_null(NodePath(node_path));
    if (node_to_remove) {
        node_to_remove->queue_free(); // Détruit le nœud
    }
}