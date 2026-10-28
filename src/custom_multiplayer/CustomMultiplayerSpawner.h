//
// Created by leoul on 2026-10-26.
//

#pragma once

#ifndef CUSTOM_MULTIPLAYER_SPAWNER_H
#define CUSTOM_MULTIPLAYER_SPAWNER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace godot {

    class CustomMultiplayerSpawner : public Node {
        GDCLASS(CustomMultiplayerSpawner, Node)

        NodePath spawn_path;

        Node *_spawn_node = nullptr;

    protected:
        static void _bind_methods();

    public:
        TypedArray<PackedScene> spawnable_scenes;
        CustomMultiplayerSpawner();
        ~CustomMultiplayerSpawner();

        // Accesseurs pour l'éditeur Godot
        void set_spawn_path(const NodePath &path);
        NodePath get_spawn_path() const;

        void set_spawnable_scenes(const TypedArray<PackedScene> &scenes);
        TypedArray<PackedScene> get_spawnable_scenes() const;

        // Méthodes du cycle de vie et signaux
        void _ready() override;
        void _on_child_entered_tree(Node *node);
        void _on_child_exiting_tree(Node *node);

        // Rattrapage d'état pour les nouveaux clients
        void sync_to_new_peer(int peer_id);
    };

} // namespace godot

#endif // CUSTOM_MULTIPLAYER_SPAWNER_H
