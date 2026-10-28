#include "register_types.h"
#include "benchmark_godot_network/network_benchmark.h"
#include "benchmark_godot_network/network_benchmark_tcp.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "custom_multiplayer/CustomMultiplayerSpawner.h"
#include "custom_multiplayer/CustomMultiplayerSynchronizer.h"
#include "custom_multiplayer/CustomRPC.h"
#include "custom_multiplayer/CustomSpawnManager.h"
#include "custom_multiplayer/CustomSyncState.h"
#include "custom_multiplayer/NetSocketManager.h"
#include "godot_cpp/classes/engine.hpp"
#include "Quic/QuicClient.h"
#include "Quic/QuicServer.h"

using namespace godot;
static CustomRPC *customRPC = nullptr;
static CustomSyncState *custom_sync_state = nullptr;
static CustomSpawnManager *custom_spawn_manager = nullptr;
static NetSocketManager *net_socket_manager = nullptr;

void initialize_network_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_class<NetworkBenchmark>();
    ClassDB::register_class<NetworkBenchmarkTCP>();

#if defined(_WIN32)
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

    ClassDB::register_class<QuicClient>();
    ClassDB::register_class<QuicServer>();

    // addons custom multiplayer
    ClassDB::register_class<CustomMultiplayerSpawner>();
    ClassDB::register_class<CustomMultiplayerSynchronizer>();

    // autoload singleton
    ClassDB::register_class<CustomRPC>();
    customRPC = memnew(godot::CustomRPC);
    Engine::get_singleton()->register_singleton("CustomRPC", customRPC);

    ClassDB::register_class<CustomSyncState>();
    custom_sync_state = memnew(godot::CustomSyncState);
    Engine::get_singleton()->register_singleton("CustomSyncState", custom_sync_state);

    ClassDB::register_class<CustomSpawnManager>();
    custom_spawn_manager = memnew(godot::CustomSpawnManager);
    Engine::get_singleton()->register_singleton("CustomSpawnManager", custom_spawn_manager);

    ClassDB::register_class<NetSocketManager>();
    net_socket_manager = memnew(godot::NetSocketManager);
    Engine::get_singleton()->register_singleton("NetSocketManager", net_socket_manager);

    net_socket_manager->connect("raw_packet_received", Callable(customRPC, "_on_raw_packet_received"));
    net_socket_manager->connect("raw_packet_received", Callable(custom_sync_state, "_on_raw_packet_received"));
    net_socket_manager->connect("raw_packet_received", Callable(custom_spawn_manager, "_on_raw_packet_received"));
}

void uninitialize_network_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

#if defined(_WIN32)
    // Nettoyage sécurisé
    WSACleanup();
#endif

    if (net_socket_manager) {
        if (customRPC) {
            net_socket_manager->disconnect("raw_packet_received", Callable(customRPC, "_on_raw_packet_received"));
        }
        if (custom_sync_state) {
            net_socket_manager->disconnect("raw_packet_received", Callable(custom_sync_state, "_on_raw_packet_received"));
        }
        if (custom_spawn_manager) {
            net_socket_manager->disconnect("raw_packet_received", Callable(custom_spawn_manager, "_on_raw_packet_received"));
        }
    }

    Engine::get_singleton()->unregister_singleton("CustomRPC");
    if (customRPC) {
        memdelete(customRPC);
        customRPC = nullptr;
    }

    Engine::get_singleton()->unregister_singleton("CustomSyncState");
    if (custom_sync_state) {
        memdelete(custom_sync_state);
        custom_sync_state = nullptr;
    }

    Engine::get_singleton()->unregister_singleton("CustomSpawnManager");
    if (custom_spawn_manager) {
        memdelete(custom_spawn_manager);
        custom_spawn_manager = nullptr;
    }

    Engine::get_singleton()->unregister_singleton("NetSocketManager");
    if (net_socket_manager) {
        memdelete(net_socket_manager);
        net_socket_manager = nullptr;
    }
}

extern "C" {
GDExtensionBool GDE_EXPORT network_test_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization
) {
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_network_module);
    init_obj.register_terminator(uninitialize_network_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}