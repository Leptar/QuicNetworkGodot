#include "register_types.h"
#include "benchmark_godot_network/network_benchmark.h"
#include "network_benchmark_tcp.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "Quic/QuicClient.h"
#include "Quic/QuicServer.h"

using namespace godot;

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
}

void uninitialize_network_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

#if defined(_WIN32)
    // Nettoyage sécurisé
    WSACleanup();
#endif
}

extern "C" {
GDExtensionBool GDE_EXPORT network_test_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization
) {
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_network_module);
    init_obj.register_terminator(uninitialize_network_module);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}