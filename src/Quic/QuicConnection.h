//
// Created by leoul on 2026-10-14.
//

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>
#include <openssl/ssl.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>

namespace godot {

class QuicConnection {
public:

    // delegates
    // appelé quand ngtcp2 a chiffré des octets qui doivent partir sur le réseau
    using TransmitCallback = std::function<void(const uint8_t *data, size_t len, const ngtcp2_path &path)>;

    // appelé quand ngtcp2 a déchiffré un message valide venant du réseau
    using PacketReceivedCallback = std::function<void(int channel_id, const uint8_t *data, size_t len, bool reliable)>;

    // appelé quand le Handshake TLS 1.3 est terminé et que les clés sont dérivées
    using HandshakeCompletedCallback = std::function<void()>;

private:
    bool is_server;
    bool connection_closed = false;

    // Table associant un channel_id logique Godot à un stream_id QUIC ouvert localement
    std::unordered_map<int, int64_t> channel_to_stream;

    // Contexte cryptographique et machine à états exclusifs à ce joueur
    ngtcp2_conn *connection = nullptr;
    SSL *ssl = nullptr;
    ngtcp2_crypto_conn_ref conn_ref;

    // Pointeurs vers les fonctions fournies par le nœud parent
    // A checker si je le fais bien
    TransmitCallback cb_transmit;
    PacketReceivedCallback cb_packet_received;
    HandshakeCompletedCallback custom_cb_handshake_completed;

    // callback static ngtcp2
    static ngtcp2_conn *cb_get_conn(ngtcp2_crypto_conn_ref *conn_ref);
    static int cb_get_new_connection_id(ngtcp2_conn *conn, ngtcp2_cid *cid, uint8_t *token, size_t cidlen, void *user_data);
    static void cb_rand(uint8_t *dest, size_t destlen, const ngtcp2_rand_ctx *rand_ctx);

    static int cb_handshake_completed(ngtcp2_conn *conn, void *user_data);

    // callback de réception séparés
    static int cb_recv_stream_data(ngtcp2_conn *conn, uint32_t flags, int64_t stream_id, uint64_t offset, const uint8_t *data, size_t datalen, void *user_data, void *stream_user_data);
    static int cb_recv_datagram(ngtcp2_conn *conn, uint32_t flags, const uint8_t *data, size_t datalen, void *user_data);

    // surcouche du callback crypto
    static int custom_recv_crypto_data_cb(ngtcp2_conn *conn, ngtcp2_encryption_level level, uint64_t offset, const uint8_t *data, size_t datalen, void *user_data);

public:
    // cycle de vie
    // Le constructeur prend le rôle (serveur/client), le contexte TLS pré-initialisé par le parent,
    // les IDs de connexion (CID), le chemin réseau abstrait, et les trois délégués.
    QuicConnection(bool p_is_server, SSL_CTX *ssl_ctx,
                   const ngtcp2_cid &dcid, const ngtcp2_cid &scid,
                   const ngtcp2_cid &original_dcid,
                   const ngtcp2_path &path,
                   ngtcp2_tstamp initial_tstamp,
                   TransmitCallback transmit_cb,
                   PacketReceivedCallback receive_cb,
                   HandshakeCompletedCallback handshake_cb);
    ~QuicConnection();

    // Input
    // le parent appelle ça quand il a des octets destinés à ce joueur.
    void process_datagram(const ngtcp2_path &path, const uint8_t *data, size_t datalen, ngtcp2_tstamp ts);

    // Output
    // L'API applicative appelle ceci pour envoyer un payload de jeu.
    void send_packet(int channel_id, const uint8_t *data, size_t len, bool reliable, ngtcp2_tstamp ts);

    // maintenance (Horloge et écriture)
    // Force la machine à états à purger ses files d'attente sortantes.
    void write_packets(ngtcp2_tstamp ts);

    // Gère les timeouts et les retransmissions.
    void handle_expiry(ngtcp2_tstamp ts);

    // Permet au Serveur de savoir quand réveiller cette connexion (Min-Heap).
    ngtcp2_tstamp get_next_expiry() const;

    // Génère et envoie immédiatement un paquet CONNECTION_CLOSE au pair distant
    void close_connection(ngtcp2_tstamp ts);

    // Renvoie true si la connexion est morte (erreur ou fermeture propre).
    bool is_closed() const;
};

} // namespace godot