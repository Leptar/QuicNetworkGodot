//
// Created by leoul on 2026-10-26.
//

#pragma once

#ifndef CUSTOM_SPAWN_MANAGER_H
#define CUSTOM_SPAWN_MANAGER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

    class CustomSpawnManager : public Node {
        GDCLASS(CustomSpawnManager, Node)

        // Identifiants des types de paquets
        const uint8_t PACKET_TYPE_SPAWN = 3;
        const uint8_t PACKET_TYPE_DESPAWN = 4;

        // Registre des scènes : { "id_unique": Ref<PackedScene> }
        HashMap<uint32_t, Ref<PackedScene>> spawnable_scenes;

        void _execute_spawn(const String &parent_path, uint32_t scene_id, const String &node_name, const Vector3 &position);
        void _execute_despawn(const String &node_path);

    protected:
        static void _bind_methods();

    public:
        CustomSpawnManager();
        ~CustomSpawnManager();

        // Gestion du registre
        void register_scene(uint32_t scene_id, const Ref<PackedScene> &packed_scene);

        // Sérialisation et Envoi
        void send_spawn(int target_peer_id, const String &parent_node_path, uint32_t scene_id, const String &node_name, const Vector3 &initial_position);
        void broadcast_spawn(const String &parent_node_path, uint32_t scene_id, const String &node_name, const Vector3 &initial_position, int exclude_peer = 0);
        void broadcast_despawn(const String &node_path_to_remove, int exclude_peer = 0);

        // Routage
        void _on_raw_packet_received(int sender_id, const PackedByteArray &payload, bool is_reliable);
    };

} // namespace godot

#endif // CUSTOM_SPAWN_MANAGER_H
