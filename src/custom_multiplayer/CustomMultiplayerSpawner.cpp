//
// Created by leoul on 2026-10-26.
//

#include "CustomMultiplayerSpawner.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void CustomMultiplayerSpawner::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_spawn_path", "path"), &CustomMultiplayerSpawner::set_spawn_path);
    ClassDB::bind_method(D_METHOD("get_spawn_path"), &CustomMultiplayerSpawner::get_spawn_path);

    ClassDB::bind_method(D_METHOD("set_spawnable_scenes", "scenes"), &CustomMultiplayerSpawner::set_spawnable_scenes);
    ClassDB::bind_method(D_METHOD("get_spawnable_scenes"), &CustomMultiplayerSpawner::get_spawnable_scenes);

    ClassDB::bind_method(D_METHOD("_on_child_entered_tree", "node"), &CustomMultiplayerSpawner::_on_child_entered_tree);
    ClassDB::bind_method(D_METHOD("_on_child_exiting_tree", "node"), &CustomMultiplayerSpawner::_on_child_exiting_tree);
    ClassDB::bind_method(D_METHOD("sync_to_new_peer", "peer_id"), &CustomMultiplayerSpawner::sync_to_new_peer);

    ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "spawn_path"), "set_spawn_path", "get_spawn_path");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "spawnable_scenes", PROPERTY_HINT_ARRAY_TYPE, "PackedScene"), "set_spawnable_scenes", "get_spawnable_scenes");
}

CustomMultiplayerSpawner::CustomMultiplayerSpawner() {}
CustomMultiplayerSpawner::~CustomMultiplayerSpawner() {}

void CustomMultiplayerSpawner::set_spawn_path(const NodePath &path) { spawn_path = path; }
NodePath CustomMultiplayerSpawner::get_spawn_path() const { return spawn_path; }

void CustomMultiplayerSpawner::set_spawnable_scenes(const TypedArray<PackedScene> &scenes) { spawnable_scenes = scenes; }
TypedArray<PackedScene> CustomMultiplayerSpawner::get_spawnable_scenes() const { return spawnable_scenes; }

void CustomMultiplayerSpawner::_ready() {
    Object *spawn_manager = Engine::get_singleton()->get_singleton("CustomSpawnManager");

    if (spawn_manager) {
        for (int i = 0; i < spawnable_scenes.size(); i++) {
            Ref<PackedScene> scene = spawnable_scenes[i];
            if (scene.is_valid()) {
                uint32_t scene_id = scene->get_path().hash();
                spawn_manager->call("register_scene", scene_id, scene);
            }
        }
    }

    if (!spawn_path.is_empty()) {
        _spawn_node = get_node_or_null(spawn_path);
        if (_spawn_node) {
            _spawn_node->connect("child_entered_tree", Callable(this, "_on_child_entered_tree"));
            _spawn_node->connect("child_exiting_tree", Callable(this, "_on_child_exiting_tree"));
        }
    }
}

void CustomMultiplayerSpawner::_on_child_entered_tree(Node *node) {
    Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
    if (!net_manager || !(bool)net_manager->get("is_server")) return;

    if (node->get_scene_file_path().is_empty()) return;

    uint32_t scene_id = node->get_scene_file_path().hash();
    Object *spawn_manager = Engine::get_singleton()->get_singleton("CustomSpawnManager");

    if (spawn_manager) {
        Vector3 pos = Vector3(0, 0, 0);
        Node3D *node3d = Object::cast_to<Node3D>(node);
        if (node3d) pos = node3d->get_position();

        spawn_manager->call("broadcast_spawn", _spawn_node->get_path(), scene_id, node->get_name(), pos, 0);
    }
}

void CustomMultiplayerSpawner::_on_child_exiting_tree(Node *node) {
    Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
    if (!net_manager || !(bool)net_manager->get("is_server")) return;

    if (node->get_scene_file_path().is_empty()) return;

    Object *spawn_manager = Engine::get_singleton()->get_singleton("CustomSpawnManager");
    if (spawn_manager) {
        spawn_manager->call("broadcast_despawn", node->get_path(), 0);
    }
}

void CustomMultiplayerSpawner::sync_to_new_peer(int peer_id) {
    Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
    if (!net_manager || !(bool)net_manager->get("is_server") || !_spawn_node) return;

    Object *spawn_manager = Engine::get_singleton()->get_singleton("CustomSpawnManager");
    if (!spawn_manager) return;

    TypedArray<Node> children = _spawn_node->get_children();
    for (int i = 0; i < children.size(); i++) {
        Node *child = Object::cast_to<Node>(children[i]);
        if (child && !child->get_scene_file_path().is_empty()) {
            uint32_t scene_id = child->get_scene_file_path().hash();

            Vector3 pos = Vector3(0, 0, 0);
            Node3D *node3d = Object::cast_to<Node3D>(child);
            if (node3d) pos = node3d->get_position();

            spawn_manager->call("send_spawn", peer_id, _spawn_node->get_path(), scene_id, child->get_name(), pos);
        }
    }
}