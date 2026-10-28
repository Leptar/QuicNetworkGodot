//
// Created by leoul on 2026-10-14.
//
#include "QuicConnection.h"
#include <cstring>
#include <iostream>

#include "godot_cpp/variant/utility_functions.hpp"
#include "openssl/rand.h"

namespace godot {

ngtcp2_conn *QuicConnection::cb_get_conn(ngtcp2_crypto_conn_ref *conn_ref) {
    QuicConnection *conn = static_cast<QuicConnection *>(conn_ref->user_data);
    return conn->connection;
}

int QuicConnection::cb_get_new_connection_id(ngtcp2_conn *conn, ngtcp2_cid *cid, uint8_t *token, size_t cidlen, void *user_data) {
    if (RAND_bytes(cid->data, cidlen) != 1 || RAND_bytes(token, NGTCP2_STATELESS_RESET_TOKENLEN) != 1) {
        return NGTCP2_ERR_CALLBACK_FAILURE;
    }
    cid->datalen = cidlen;
    return 0;
}

void QuicConnection::cb_rand(uint8_t *dest, size_t destlen, const ngtcp2_rand_ctx *rand_ctx) {
    RAND_bytes(dest, destlen);
}

int QuicConnection::cb_handshake_completed(ngtcp2_conn *conn, void *user_data) {
    QuicConnection *c = static_cast<QuicConnection *>(user_data);
    if (c->custom_cb_handshake_completed) {
        c->custom_cb_handshake_completed(); // Notifie le parent
    }
    return 0;
}

int QuicConnection::cb_recv_stream_data(ngtcp2_conn *conn, uint32_t flags, int64_t stream_id,
                                        uint64_t offset, const uint8_t *data, size_t datalen,
                                        void *user_data, void *stream_user_data) {
    QuicConnection *c = static_cast<QuicConnection *>(user_data);
    if (datalen > 0 && c->cb_packet_received) {
        int channel_id = static_cast<int>(data[0]);
        const uint8_t *payload = data + 1;
        size_t payload_len = datalen - 1;

        c->cb_packet_received(channel_id, payload, payload_len, true);
    }
    return 0;
}

int QuicConnection::cb_recv_datagram(ngtcp2_conn *conn, uint32_t flags, const uint8_t *data, size_t datalen, void *user_data) {
    QuicConnection *c = static_cast<QuicConnection *>(user_data);

    // extrait le channel_id (le 1er octet)
    if (datalen > 0 && c->cb_packet_received) {
        int channel_id = static_cast<int>(data[0]);
        const uint8_t *payload = data + 1;
        size_t payload_len = datalen - 1;

        // Unreliable : Transmission des données pures
        c->cb_packet_received(channel_id, payload, payload_len, false);
    }
    return 0;
}

int QuicConnection::custom_recv_crypto_data_cb(ngtcp2_conn *conn, ngtcp2_encryption_level level,
                                               uint64_t offset, const uint8_t *data, size_t datalen, void *user_data) {
    int rv = ngtcp2_crypto_recv_crypto_data_cb(conn, level, offset, data, datalen, user_data);
    if (rv != 0) {
        // En cas d'erreur de BoringSSL, l'erreur est purgée ici
        while (unsigned long ssl_err = ERR_get_error()) { /* TODO logging C++ standard */ }
    }
    return rv;
}

QuicConnection::QuicConnection(bool p_is_server, SSL_CTX *ssl_ctx,
                               const ngtcp2_cid &dcid, const ngtcp2_cid &scid,
                               const ngtcp2_cid &original_dcid,
                               const ngtcp2_path &path,
                               ngtcp2_tstamp initial_ts,
                               TransmitCallback transmit_cb,
                               PacketReceivedCallback receive_cb,
                               HandshakeCompletedCallback handshake_cb)
    : is_server(p_is_server), cb_transmit(transmit_cb),
      cb_packet_received(receive_cb), custom_cb_handshake_completed(handshake_cb)
{
    conn_ref.get_conn = cb_get_conn;
    conn_ref.user_data = this;

    ngtcp2_callbacks callbacks{};
    callbacks.recv_crypto_data = custom_recv_crypto_data_cb;
    callbacks.encrypt = ngtcp2_crypto_encrypt_cb;
    callbacks.decrypt = ngtcp2_crypto_decrypt_cb;
    callbacks.hp_mask = ngtcp2_crypto_hp_mask_cb;
    callbacks.recv_retry = ngtcp2_crypto_recv_retry_cb;
    callbacks.update_key = ngtcp2_crypto_update_key_cb;
    callbacks.delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb;
    callbacks.delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb;
    callbacks.get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb;
    callbacks.version_negotiation = ngtcp2_crypto_version_negotiation_cb;
    callbacks.rand = cb_rand;
    callbacks.get_new_connection_id = cb_get_new_connection_id;
    callbacks.handshake_completed = cb_handshake_completed;
    callbacks.recv_stream_data = cb_recv_stream_data;
    callbacks.recv_datagram = cb_recv_datagram; // Callback activé pour les datagrammes

    // Côté Client, un callback supplémentaire est requis pour le vol Initial
    if (!is_server) {
        callbacks.client_initial = ngtcp2_crypto_client_initial_cb;
    } else {
        callbacks.recv_client_initial = ngtcp2_crypto_recv_client_initial_cb;
    }

    ngtcp2_settings settings;
    ngtcp2_settings_default(&settings);
    // temps réel sera injecté lors du traitement
    settings.initial_ts = initial_ts;

    ngtcp2_transport_params params;
    ngtcp2_transport_params_default(&params);
    params.initial_max_streams_bidi = 100;
    params.initial_max_streams_uni = 100;
    params.initial_max_data = 1024 * 1024;
    params.initial_max_stream_data_bidi_local = 256 * 1024;
    params.initial_max_stream_data_bidi_remote = 256 * 1024;
    params.initial_max_stream_data_uni = 256 * 1024;

    // 1000 taille min pour initial
    params.max_datagram_frame_size = 1000;

    if (is_server) {
        params.original_dcid = original_dcid; // DCID original du client
        params.original_dcid_present = 1;
    }

    ssl = SSL_new(ssl_ctx);
    SSL_set_app_data(ssl, &conn_ref);

    if (is_server) {
        SSL_set_accept_state(ssl);
        ngtcp2_conn_server_new(&connection, &dcid, &scid, &path, NGTCP2_PROTO_VER_V1, &callbacks, &settings, &params, nullptr, this);
    } else {
        SSL_set_connect_state(ssl);
        static const uint8_t alpn_proto[] = "\x0agodot-quic";
        SSL_set_alpn_protos(ssl, alpn_proto, sizeof(alpn_proto) - 1);
        SSL_set_tlsext_host_name(ssl, "host");

        ngtcp2_conn_client_new(&connection, &dcid, &scid, &path, NGTCP2_PROTO_VER_V1, &callbacks, &settings, &params, nullptr, this);
    }

    ngtcp2_conn_set_tls_native_handle(connection, ssl);
}

QuicConnection::~QuicConnection() {
    if (connection) {
        ngtcp2_conn_del(connection);
    }
    if (ssl) {
        SSL_free(ssl);
    }
}

void QuicConnection::process_datagram(const ngtcp2_path &path, const uint8_t *data, size_t datalen, ngtcp2_tstamp ts) {
    if (is_closed()) return;

    ngtcp2_pkt_info pi{};
    int rv = ngtcp2_conn_read_pkt(connection, &path, &pi, data, datalen, ts);
    if (rv != 0) {
        if (rv == NGTCP2_ERR_DRAINING) {
            UtilityFunctions::print("[QUIC] (CONNECTION_CLOSE) recue du pair. Fermeture.");
        } else {
            UtilityFunctions::printerr("[QUIC] Erreur fatale ngtcp2_conn_read_pkt : ", rv, " (", ngtcp2_strerror(rv), ")");
        }
        connection_closed = true;
        return;
    }

    // Double vérification : si le paquet contenait une trame CONNECTION_CLOSE, ngtcp2 entre en mode "draining"
    if (ngtcp2_conn_in_draining_period(connection)) {
        UtilityFunctions::print("[QUIC] Connexion en periode de draining (fermee par le pair).");
        connection_closed = true;
    }
}

void QuicConnection::write_packets(ngtcp2_tstamp ts) {
    if (is_closed() || !cb_transmit) return;

    uint8_t out_buf[1250];
    ngtcp2_path_storage ps;
    ngtcp2_path_storage_zero(&ps);
    ngtcp2_pkt_info pi;
    memset(&pi, 0, sizeof(pi));

    while (true) {
        ngtcp2_ssize nwrite = ngtcp2_conn_write_pkt(connection, &ps.path, &pi, out_buf, sizeof(out_buf), ts);
        if (nwrite <= 0) {
            break; // Plus rien à écrire ou erreur
        }
        cb_transmit(out_buf, static_cast<size_t>(nwrite), ps.path);
    }

}

void QuicConnection::send_packet(int channel_id, const uint8_t *data, size_t len, bool reliable, ngtcp2_tstamp ts) {
    if (is_closed()) return;

    if (reliable) {
        int64_t stream_id = -1;
        auto it = channel_to_stream.find(channel_id);

        if (it != channel_to_stream.end()) {
            stream_id = it->second;
        } else {
            // Ni le client ni le serveur n'ont encore ouvert de stream pour ce channel_id
            int rv = ngtcp2_conn_open_bidi_stream(connection, &stream_id, nullptr);
            if (rv != 0) {
                UtilityFunctions::printerr("[QUIC] Impossible d'ouvrir un stream pour le canal ",
                                                    channel_id, " : ", ngtcp2_strerror(rv));
                return;
            }
            channel_to_stream[channel_id] = stream_id;
        }

        // On préfixe le payload avec 1 octet pour le channel_id (comme en unreliable)
        std::vector<uint8_t> stream_payload(len + 1);
        stream_payload[0] = static_cast<uint8_t>(channel_id);
        std::memcpy(stream_payload.data() + 1, data, len);

        ngtcp2_vec vec;
        vec.base = stream_payload.data();
        vec.len = stream_payload.size();

        uint8_t out_buf[1250];
        ngtcp2_path_storage ps;
        ngtcp2_path_storage_zero(&ps);
        ngtcp2_pkt_info pi;
        memset(&pi, 0, sizeof(pi));

        while (vec.len > 0) {
            ngtcp2_ssize pdatalen = 0;
            ngtcp2_ssize nwrite = ngtcp2_conn_writev_stream(
                connection, &ps.path, &pi, out_buf, sizeof(out_buf), &pdatalen,
                NGTCP2_WRITE_STREAM_FLAG_NONE, stream_id, &vec, 1, ts);

            if (nwrite <= 0) {
                break;
            }

            if (cb_transmit) {
                cb_transmit(out_buf, static_cast<size_t>(nwrite), ps.path);
            }

            if (pdatalen > 0) {
                vec.base += pdatalen;
                vec.len -= pdatalen;
            } else {
                break;
            }
        }
    } else {
        // unreliable : Framing Applicatif
        if (len + 1 > 1150) return; // Sécurité : dépassement de la MTU théorique

        std::vector<uint8_t> datagram_payload(len + 1);
        datagram_payload[0] = static_cast<uint8_t>(channel_id); // 1er octet = Canal
        std::memcpy(datagram_payload.data() + 1, data, len);

        // prepa des buffers de sortie pour faire le paquet UDP complet
        uint8_t out_buf[1250];
        ngtcp2_path_storage ps;
        ngtcp2_path_storage_zero(&ps);
        ngtcp2_pkt_info pi;
        memset(&pi, 0, sizeof(pi));

        int accepted = 0;

        ngtcp2_ssize nwrite = ngtcp2_conn_write_datagram(
            connection,
            &ps.path,
            &pi,
            out_buf,
            sizeof(out_buf),
            &accepted,
            NGTCP2_WRITE_DATAGRAM_FLAG_NONE,
            0,
            datagram_payload.data(),
            datagram_payload.size(),
            ts
        );

        // Si le paquet est bien créé, on demande au parent de l'envoyer
        if (nwrite > 0 && cb_transmit) {
            cb_transmit(out_buf, static_cast<size_t>(nwrite), ps.path);
        }
    }
}

ngtcp2_tstamp QuicConnection::get_next_expiry() const {
    return is_closed() ? 0 : ngtcp2_conn_get_expiry(connection);
}

void QuicConnection::close_connection(ngtcp2_tstamp ts) {
    if (is_closed() || !connection) return;

    if (cb_transmit) {
        uint8_t out_buf[1250];
        ngtcp2_path_storage ps;
        ngtcp2_path_storage_zero(&ps);
        ngtcp2_pkt_info pi{};

        ngtcp2_ccerr ccerr;
        ngtcp2_ccerr_default(&ccerr); // NO_ERROR = Fermeture)

        ngtcp2_ssize nwrite = ngtcp2_conn_write_connection_close(
            connection, &ps.path, &pi, out_buf, sizeof(out_buf), &ccerr, ts);

        if (nwrite > 0) {
            cb_transmit(out_buf, static_cast<size_t>(nwrite), ps.path);
        }
    }

    connection_closed = true;
}

void QuicConnection::handle_expiry(ngtcp2_tstamp ts) {
    if (!is_closed()) {
        int rv = ngtcp2_conn_handle_expiry(connection, ts);
        if (rv != 0) {
            UtilityFunctions::print("[QUIC] Connexion expiree (Timeout) : ", ngtcp2_strerror(rv));
            connection_closed = true;
        }
    }
}

bool QuicConnection::is_closed() const {
    return connection == nullptr || connection_closed;
}

} // namespace godot