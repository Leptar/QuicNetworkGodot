#include "network_benchmark_tcp.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cmath>
#include <cstring>

using namespace godot;

void NetworkBenchmarkTCP::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start_server", "port"), &NetworkBenchmarkTCP::start_server);
    ClassDB::bind_method(D_METHOD("start_client", "host", "port"), &NetworkBenchmarkTCP::start_client);
    ClassDB::bind_method(D_METHOD("send_ping"), &NetworkBenchmarkTCP::send_ping);
    ClassDB::bind_method(D_METHOD("poll"), &NetworkBenchmarkTCP::poll);
    ClassDB::bind_method(D_METHOD("set_no_delay", "enabled"), &NetworkBenchmarkTCP::set_no_delay);
    ClassDB::bind_method(D_METHOD("get_last_rtt_ms"), &NetworkBenchmarkTCP::get_last_rtt_ms);
    ClassDB::bind_method(D_METHOD("get_jitter_ms"), &NetworkBenchmarkTCP::get_jitter_ms);
    ClassDB::bind_method(D_METHOD("is_connected_to_host"), &NetworkBenchmarkTCP::is_connected_to_host);
    ClassDB::bind_method(D_METHOD("close"), &NetworkBenchmarkTCP::close);
}

NetworkBenchmarkTCP::NetworkBenchmarkTCP() {
    server.instantiate();
    peer.instantiate();
}

NetworkBenchmarkTCP::~NetworkBenchmarkTCP() {
    close();
}

int NetworkBenchmarkTCP::start_server(int port) {
    is_server = true;
    Error err = server->listen(port);
    if (err != OK) {
        UtilityFunctions::printerr("[TCP Server] Échec du listen sur le port : ", port);
        return err;
    }
    UtilityFunctions::print("[TCP Server] Écoute TCP active sur le port : ", port);
    return OK;
}

int NetworkBenchmarkTCP::start_client(const String &host, int port) {
    is_server = false;
    Error err = peer->connect_to_host(host, port);
    if (err != OK) {
        UtilityFunctions::printerr("[TCP Client] Impossible d'initier la connexion vers : ", host);
        return err;
    }
    peer->set_no_delay(false);
    UtilityFunctions::print("[TCP Client] Connexion en cours vers ", host, ":", port);
    return OK;
}

void NetworkBenchmarkTCP::set_no_delay(bool enabled) {
    if (peer.is_valid()) {
        peer->set_no_delay(enabled);
    }
    if (client_connection.is_valid()) {
        client_connection->set_no_delay(enabled);
    }
}

bool NetworkBenchmarkTCP::is_connected_to_host() const {
    return peer.is_valid() && peer->get_status() == StreamPeerTCP::STATUS_CONNECTED;
}

void NetworkBenchmarkTCP::send_ping() {
    if (is_server || !is_connected_to_host()) {
        return;
    }

    PingPacketTCP pkt;
    pkt.sequence = ++current_seq;
    pkt.send_time_usec = Time::get_singleton()->get_ticks_usec();

    PackedByteArray buffer;
    buffer.resize(sizeof(PingPacketTCP));
    memcpy(buffer.ptrw(), &pkt, sizeof(PingPacketTCP));

    peer->put_data(buffer);
}

void NetworkBenchmarkTCP::poll() {
    if (is_server) {
        if (server->is_connection_available()) {
            client_connection = server->take_connection();
            client_connection->set_no_delay(false);
            UtilityFunctions::print("[TCP Server] Client connecté avec succès !");
        }

        if (client_connection.is_valid()) {
            client_connection->poll();
            if (client_connection->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
                return;
            }

            while (client_connection->get_available_bytes() >= (int)sizeof(PingPacketTCP)) {
                Array res = client_connection->get_data(sizeof(PingPacketTCP));
                Error err = (Error)(int64_t)res[0];
                if (err == OK) {
                    PackedByteArray data = res[1];
                    client_connection->put_data(data);
                }
            }
        }
    } else {
        if (!peer.is_valid()) {
            return;
        }

        peer->poll();

        if (peer->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
            while (peer->get_available_bytes() >= (int)sizeof(PingPacketTCP)) {
                Array res = peer->get_data(sizeof(PingPacketTCP));
                Error err = (Error)(int64_t)res[0];
                if (err == OK) {
                    PackedByteArray buffer = res[1];
                    if (buffer.size() >= (int64_t)sizeof(PingPacketTCP)) {
                        PingPacketTCP pkt;
                        memcpy(&pkt, buffer.ptr(), sizeof(PingPacketTCP));

                        uint64_t now = Time::get_singleton()->get_ticks_usec();
                        if (now >= pkt.send_time_usec) {
                            last_rtt_usec = now - pkt.send_time_usec;

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
        }
    }
}

void NetworkBenchmarkTCP::close() {
    if (peer.is_valid()) {
        peer->disconnect_from_host();
    }
    if (client_connection.is_valid()) {
        client_connection->disconnect_from_host();
    }
    if (server.is_valid()) {
        server->stop();
    }
}