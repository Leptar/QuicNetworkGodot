//
// Created by leoul on 2026-10-26.
//

#include "CustomMultiplayerSynchronizer.h"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/dictionary.hpp>

using namespace godot;

void CustomMultiplayerSynchronizer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_sync_properties", "props"), &CustomMultiplayerSynchronizer::set_sync_properties);
    ClassDB::bind_method(D_METHOD("get_sync_properties"), &CustomMultiplayerSynchronizer::get_sync_properties);

    ClassDB::bind_method(D_METHOD("set_sync_interval", "interval"), &CustomMultiplayerSynchronizer::set_sync_interval);
    ClassDB::bind_method(D_METHOD("get_sync_interval"), &CustomMultiplayerSynchronizer::get_sync_interval);

    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "sync_properties", PROPERTY_HINT_ARRAY_TYPE, "String"), "set_sync_properties", "get_sync_properties");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sync_interval"), "set_sync_interval", "get_sync_interval");
}

CustomMultiplayerSynchronizer::CustomMultiplayerSynchronizer() {
    // Valeurs par défaut comme dans ton GDScript
    sync_properties.push_back("position");
    sync_properties.push_back("rotation");
    set_physics_process(true);
}
CustomMultiplayerSynchronizer::~CustomMultiplayerSynchronizer() {}

void CustomMultiplayerSynchronizer::set_sync_properties(const TypedArray<String> &props) { sync_properties = props; }
TypedArray<String> CustomMultiplayerSynchronizer::get_sync_properties() const { return sync_properties; }

void CustomMultiplayerSynchronizer::set_sync_interval(double interval) { sync_interval = interval; }
double CustomMultiplayerSynchronizer::get_sync_interval() const { return sync_interval; }

void CustomMultiplayerSynchronizer::_ready() {
    _parent = get_parent();
    if (_parent) {
        String parent_name = _parent->get_name();
        if (parent_name.is_valid_int()) {
            authority_id = parent_name.to_int(); // Règle d'autorité basée sur le nom
        }
    }
}

void CustomMultiplayerSynchronizer::_physics_process(double delta) {
    if (Engine::get_singleton()->is_editor_hint()) return; // Ignorer dans l'éditeur

    Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
    if (!net_manager) return;

    int local_peer_id = net_manager->get("local_peer_id");
    if (local_peer_id != authority_id) return; // Seul le propriétaire synchronise

    _time_since_last_sync += delta;
    if (_time_since_last_sync >= sync_interval) {
        _time_since_last_sync = 0.0;
        _sync_data();
    }
}

void CustomMultiplayerSynchronizer::_sync_data() {
    if (!_parent) return;

    Dictionary state;

    for (int i = 0; i < sync_properties.size(); i++) {
        String path_str = sync_properties[i];
        String node_path = ".";
        String prop = path_str;

        if (path_str.contains(":")) {
            PackedStringArray parts = path_str.split(":");
            node_path = parts[0];
            prop = parts[1];
        }

        Node *target = _parent->get_node_or_null(NodePath(node_path));

        if (target) {
            Variant val = target->get(prop);
            if (val) {
                state[path_str] = val;
            }
        }
    }

    if (!state.is_empty()) {
        Object *sync_state_manager = Engine::get_singleton()->get_singleton("CustomSyncState");
        if (sync_state_manager) {
            sync_state_manager->call("broadcast_sync_state", _parent, state, 0); // Envoi UDP
        }
    }
}