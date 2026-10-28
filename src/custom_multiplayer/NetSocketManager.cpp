#include "NetSocketManager.h"
#include <godot_cpp/classes/stream_peer_buffer.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/engine.hpp>

using namespace godot;

void NetSocketManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start_server", "port"), &NetSocketManager::start_server, DEFVAL(7070));
    ClassDB::bind_method(D_METHOD("join_server", "ip", "port"), &NetSocketManager::join_server, DEFVAL(7070));
    ClassDB::bind_method(D_METHOD("leave_server"), &NetSocketManager::leave_server);
    ClassDB::bind_method(D_METHOD("send_packet", "peer_id", "data", "reliable"), &NetSocketManager::send_packet);

    ClassDB::bind_method(D_METHOD("get_is_server"), &NetSocketManager::get_is_server);
    ClassDB::bind_method(D_METHOD("get_local_peer_id"), &NetSocketManager::get_local_peer_id);
    ClassDB::bind_method(D_METHOD("get_peers"), &NetSocketManager::get_peers);

    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_server"), "", "get_is_server");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "local_peer_id"), "", "get_local_peer_id");
    ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "peers"), "", "get_peers");

    ADD_SIGNAL(MethodInfo("peer_connected", PropertyInfo(Variant::INT, "peer_id")));
    ADD_SIGNAL(MethodInfo("peer_disconnected", PropertyInfo(Variant::INT, "peer_id")));
    ADD_SIGNAL(MethodInfo("raw_packet_received", PropertyInfo(Variant::INT, "peer_id"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "payload"), PropertyInfo(Variant::BOOL, "is_reliable")));

    ClassDB::bind_method(D_METHOD("_on_server_peer_connected", "peer_id"), &NetSocketManager::_on_server_peer_connected);
    ClassDB::bind_method(D_METHOD("_on_server_peer_disconnected", "peer_id"), &NetSocketManager::_on_server_peer_disconnected);
    ClassDB::bind_method(D_METHOD("_on_server_packet_received", "peer_id", "channel_id", "data", "reliable"), &NetSocketManager::_on_server_packet_received);
    ClassDB::bind_method(D_METHOD("_on_client_connected"), &NetSocketManager::_on_client_connected);
    ClassDB::bind_method(D_METHOD("_on_client_disconnected"), &NetSocketManager::_on_client_disconnected);
    ClassDB::bind_method(D_METHOD("_on_client_packet_received", "channel_id", "data", "reliable"), &NetSocketManager::_on_client_packet_received);
}

NetSocketManager::NetSocketManager() {}
NetSocketManager::~NetSocketManager() { leave_server(); }

bool NetSocketManager::get_is_server() const { return is_server; }
int NetSocketManager::get_local_peer_id() const { return local_peer_id; }
Dictionary NetSocketManager::get_peers() const { return peers; }

void NetSocketManager::start_server(int port) {
    is_server = true;
    local_peer_id = 1;

    // Instanciation propre
    quic_server = memnew(QuicServer);

    // On attache le serveur au SceneTree actif (Root Window) pour qu'il existe dans le jeu
    SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (tree && tree->get_root()) {
        tree->get_root()->call_deferred("add_child", quic_server);
    } else {
        call_deferred("add_child", quic_server);
    }

    // Maintenant qu'il est créé et placé, on active sa boucle _process en toute sécurité
    quic_server->set_process(true);

    quic_server->connect("peer_connected", Callable(this, "_on_server_peer_connected"));
    quic_server->connect("peer_disconnected", Callable(this, "_on_server_peer_disconnected"));
    quic_server->connect("packet_received", Callable(this, "_on_server_packet_received"));

    quic_server->call("start", port, "res://certs/server.crt", "res://certs/server.key");
    UtilityFunctions::print("[NetSocketManager] Serveur QUIC démarré (Port: ", port, ")");
}

bool NetSocketManager::join_server(const String &ip, int port) {
    is_server = false;

    // Instanciation propre
    quic_client = memnew(QuicClient);

    //On l'attache au SceneTree actif
    SceneTree *tree = Object::cast_to<SceneTree>(Engine::get_singleton()->get_main_loop());
    if (tree && tree->get_root()) {
        tree->get_root()->call_deferred("add_child", quic_client);
    } else {
        call_deferred("add_child", quic_client);
    }

    // Activation du _process
    quic_client->set_process(true);

    quic_client->connect("connected", Callable(this, "_on_client_connected"));
    quic_client->connect("disconnected", Callable(this, "_on_client_disconnected"));
    quic_client->connect("packet_received", Callable(this, "_on_client_packet_received"));

    UtilityFunctions::print("[NetSocketManager] Connexion QUIC en cours vers ", ip, ":", port);
    return quic_client->call("connect_to_host", ip, port);
}

void NetSocketManager::leave_server() {
    if (is_server && quic_server) {
        quic_server->call("stop");
        quic_server->queue_free();
        quic_server = nullptr;
    } else if (!is_server && quic_client) {
        quic_client->call("disconnect_from_host");
        quic_client->queue_free();
        quic_client = nullptr;
    }

    is_server = false;
    local_peer_id = 1;
    peers.clear();
    UtilityFunctions::print("[NetSocketManager] Réseau QUIC fermé.");
}

void NetSocketManager::send_packet(int peer_id, const PackedByteArray &data, bool reliable) {
    int channel_id = reliable ? 0 : 1;

    if (is_server && quic_server) {
        if (peer_id == 0) {
            quic_server->call("broadcast_packet", channel_id, data, reliable);
        } else {
            quic_server->call("send_packet", peer_id, channel_id, data, reliable);
        }
    } else if (!is_server && quic_client) {
        quic_client->call("send_packet", channel_id, data, reliable);
    }
}

void NetSocketManager::_on_server_peer_connected(int peer_id) {
    peers[peer_id] = true;

    Ref<StreamPeerBuffer> buffer;
    buffer.instantiate();
    buffer->put_u8(0);
    buffer->put_u32(peer_id);

    if (quic_server) {
        quic_server->call("send_packet", peer_id, 0, buffer->get_data_array(), true);
    }

    emit_signal("peer_connected", peer_id);
}

void NetSocketManager::_on_server_peer_disconnected(int peer_id) {
    peers.erase(peer_id);
    emit_signal("peer_disconnected", peer_id);
}

void NetSocketManager::_on_server_packet_received(int peer_id, int channel_id, const PackedByteArray &data, bool reliable) {
    emit_signal("raw_packet_received", peer_id, data, reliable);
}

void NetSocketManager::_on_client_connected() {
    UtilityFunctions::print("[NetSocketManager] TLS Validé. Attente de l'ID serveur...");
}

void NetSocketManager::_on_client_disconnected() {
    emit_signal("peer_disconnected", 1);
}

void NetSocketManager::_on_client_packet_received(int channel_id, const PackedByteArray &data, bool reliable) {
    if (data.size() >= 5 && data[0] == 0) {
        Ref<StreamPeerBuffer> buffer;
        buffer.instantiate();
        buffer->set_data_array(data);
        buffer->seek(1);

        local_peer_id = buffer->get_u32();
        UtilityFunctions::print("[NetSocketManager] Handshake Applicatif validé. Mon Peer ID est : ", local_peer_id);

        emit_signal("peer_connected", 1);
    } else {
        emit_signal("raw_packet_received", 1, data, reliable);
    }
}