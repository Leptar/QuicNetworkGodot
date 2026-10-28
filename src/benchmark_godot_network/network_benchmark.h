#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/packet_peer_udp.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

namespace godot {

#pragma pack(push, 1)
    struct PingPacket {
        uint32_t sequence;
        uint64_t send_time_usec;
    };
#pragma pack(pop)

    class NetworkBenchmark : public RefCounted {
        GDCLASS(NetworkBenchmark, RefCounted);

    private:
        Ref<PacketPeerUDP> peer;
        bool is_server = false;
        uint32_t current_seq = 0;

        // Métriques
        uint64_t last_rtt_usec = 0;
        double smoothed_rtt_usec = 0.0;
        double jitter_usec = 0.0;

    protected:
        static void _bind_methods();

    public:
        NetworkBenchmark();
        ~NetworkBenchmark();

        int start_server(int port);
        int start_client(const String &host, int port);
        void send_ping();
        void poll();

        double get_last_rtt_ms() const { return last_rtt_usec / 1000.0; }
        double get_jitter_ms() const { return jitter_usec / 1000.0; }
        void close();
    };

} // namespace godot