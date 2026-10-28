#include "network_benchmark.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>

using namespace godot;

void NetworkBenchmark::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start_server", "port"), &NetworkBenchmark::start_server);
    ClassDB::bind_method(D_METHOD("start_client", "host", "port"), &NetworkBenchmark::start_client);
    ClassDB::bind_method(D_METHOD("send_ping"), &NetworkBenchmark::send_ping);
    ClassDB::bind_method(D_METHOD("poll"), &NetworkBenchmark::poll);
    ClassDB::bind_method(D_METHOD("get_last_rtt_ms"), &NetworkBenchmark::get_last_rtt_ms);
    ClassDB::bind_method(D_METHOD("get_jitter_ms"), &NetworkBenchmark::get_jitter_ms);
    ClassDB::bind_method(D_METHOD("close"), &NetworkBenchmark::close);
}

NetworkBenchmark::NetworkBenchmark() {
    peer.instantiate();
}

NetworkBenchmark::~NetworkBenchmark() {
    close();
}

int NetworkBenchmark::start_server(int port) {
    is_server = true;
    Error err = peer->bind(port);
    if (err != OK) {
        UtilityFunctions::printerr("[UDP Server] Erreur de bind sur le port : ", port);
        return err;
    }
    UtilityFunctions::print("[UDP Server] Écoute active sur le port : ", port);
    return OK;
}

int NetworkBenchmark::start_client(const String &host, int port) {
    is_server = false;
    Error err = peer->connect_to_host(host, port);
    if (err != OK) {
        UtilityFunctions::printerr("[UDP Client] Impossible de se connecter à : ", host);
        return err;
    }
    UtilityFunctions::print("[UDP Client] Connecté à ", host, ":", port);
    return OK;
}

void NetworkBenchmark::send_ping() {
    if (is_server || !peer->is_socket_connected()) {
        return;
    }

    PingPacket pkt;
    pkt.sequence = ++current_seq;
    pkt.send_time_usec = Time::get_singleton()->get_ticks_usec();

    PackedByteArray buffer;
    buffer.resize(sizeof(PingPacket));
    memcpy(buffer.ptrw(), &pkt, sizeof(PingPacket));

    peer->put_packet(buffer);
}

void NetworkBenchmark::poll() {
    if (!peer.is_valid()) {
        return;
    }

    while (peer->get_available_packet_count() > 0) {
        PackedByteArray buffer = peer->get_packet();
        if (buffer.size() < (int64_t)sizeof(PingPacket)) {
            continue;
        }

        PingPacket pkt;
        memcpy(&pkt, buffer.ptr(), sizeof(PingPacket));

        if (is_server) {
            // Echo immédiat vers l'émetteur
            String sender_ip = peer->get_packet_ip();
            int sender_port = peer->get_packet_port();
            peer->set_dest_address(sender_ip, sender_port);
            peer->put_packet(buffer);
        } else {
            // Réception de l'écho côté client
            uint64_t now = Time::get_singleton()->get_ticks_usec();
            if (now >= pkt.send_time_usec) {
                last_rtt_usec = now - pkt.send_time_usec;

                // Calcul du Jitter (RFC 3550 standard)
                if (smoothed_rtt_usec == 0.0) {
                    smoothed_rtt_usec = (double)last_rtt_usec;
                } else {
                    double diff = std::abs((double)last_rtt_usec - smoothed_rtt_usec);
                    jitter_usec += (diff - jitter_usec) / 16.0;
                    smoothed_rtt_usec += ((double)last_rtt_usec - smoothed_rtt_usec) / 8.0;
                }
            }
        }
    }
}

void NetworkBenchmark::close() {
    if (peer.is_valid()) {
        peer->close();
    }
}