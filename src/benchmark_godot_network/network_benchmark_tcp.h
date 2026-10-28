#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/tcp_server.hpp>
#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

#pragma pack(push, 1)
    struct PingPacketTCP {
        uint32_t sequence;
        uint64_t send_time_usec;
    };
#pragma pack(pop)

    class NetworkBenchmarkTCP : public RefCounted {
        GDCLASS(NetworkBenchmarkTCP, RefCounted);

    private:
        Ref<TCPServer> server;
        Ref<StreamPeerTCP> client_connection; // Socket client côté serveur
        Ref<StreamPeerTCP> peer;              // Socket client côté client

        bool is_server = false;
        uint32_t current_seq = 0;

        uint64_t last_rtt_usec = 0;
        double smoothed_rtt_usec = 0.0;
        double jitter_usec = 0.0;

    protected:
        static void _bind_methods();

    public:
        NetworkBenchmarkTCP();
        ~NetworkBenchmarkTCP();

        int start_server(int port);
        int start_client(const String &host, int port);
        void send_ping();
        void poll();

        void set_no_delay(bool enabled);
        double get_last_rtt_ms() const { return last_rtt_usec / 1000.0; }
        double get_jitter_ms() const { return jitter_usec / 1000.0; }
        bool is_connected_to_host() const;
        void close();
    };

} // namespace godot