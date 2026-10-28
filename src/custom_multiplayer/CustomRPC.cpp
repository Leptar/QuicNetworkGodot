#include "CustomRPC.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/classes/engine.hpp>

using namespace godot;

void CustomRPC::_bind_methods() {
    ClassDB::bind_method(D_METHOD(
        "send_rpc", "target_peer_id", "target_node", "method_name", "args"),
        &CustomRPC::send_rpc, DEFVAL(Array())
    );
    ClassDB::bind_method(D_METHOD(
        "broadcast_rpc", "target_node", "method_name", "args", "exclude_peer"),
        &CustomRPC::broadcast_rpc, DEFVAL(Array()), DEFVAL(0)
    );
    ClassDB::bind_method(D_METHOD(
        "_on_raw_packet_received", "sender_id", "payload", "is_reliable"),
        &CustomRPC::_on_raw_packet_received);
}

CustomRPC::CustomRPC() = default;
CustomRPC::~CustomRPC() = default;

void CustomRPC::send_rpc(int target_peer_id, Node *target_node, const String &method_name, const Array &args) {
    if (!target_node) return;

    Ref<StreamPeerBuffer> buffer;
    buffer.instantiate();

    // Identifiant du type de paquet (1 octet)
    buffer->put_u8(PACKET_TYPE_RPC);
    buffer->put_var(target_node->get_path());
    buffer->put_var(method_name);
    buffer->put_var(args);

    // On récupère le NetSocketManager pour envoyer le paquet en fiable (TCP/QUIC Stream)
    Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
    if (net_manager) {
        net_manager->call("send_packet", target_peer_id, buffer->get_data_array(), true);
    }
}

void CustomRPC::broadcast_rpc(Node *target_node, const String &method_name, const Array &args, int exclude_peer) {
    Node *net_manager = Object::cast_to<Node>(Engine::get_singleton()->get_singleton("NetSocketManager"));
    if (!net_manager) return;

    // On récupère la liste des pairs directement depuis l'Autoload NetSocketManager
    Dictionary peers = net_manager->get("peers");
    Array keys = peers.keys();

    // On itère sur tous les pairs connectés
    for (int i = 0; i < keys.size(); i++) {
        int peer_id = keys[i];
        if (peer_id != exclude_peer) {
            send_rpc(peer_id, target_node, method_name, args);
        }
    }
}

void CustomRPC::_on_raw_packet_received(int sender_id, const PackedByteArray &payload, bool is_reliable) {
    // On ignore les paquets UDP pour les RPC
    if (!is_reliable) return;

    Ref<StreamPeerBuffer> buffer;
    buffer.instantiate();
    buffer->set_data_array(payload);

    if (buffer->get_u8() == PACKET_TYPE_RPC) {
        // On doit décoder exactement dans le même ordre que l'encodage
        String node_path = buffer->get_var();
        String method_name = buffer->get_var();
        Array args = buffer->get_var();

        _execute_rpc(sender_id, node_path, method_name, args);
    }
}

void CustomRPC::_execute_rpc(int sender_id, const String &node_path, const String &method_name, const Array &args) {
    // On récupère le nœud cible depuis l'arbre principal
    SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (!tree) return;

    Node *target_node = tree->get_root()->get_node_or_null(NodePath(node_path));

    if (!target_node) {
        UtilityFunctions::printerr("[CustomRPC] Échec : Nœud introuvable -> ", node_path);
        return;
    }

    if (!target_node->has_method(method_name)) {
        UtilityFunctions::printerr("[CustomRPC] Échec : Méthode introuvable sur ", node_path, " -> ", method_name);
        return;
    }

    // Exécution dynamique de la fonction avec ses arguments
    target_node->callv(method_name, args);
}