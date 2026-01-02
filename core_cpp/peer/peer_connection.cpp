#include "peer_connection.h"
#include "../utils/sha1.h"

#include <iostream>
#include <cstring>
#include <fstream>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

PeerConnection::PeerConnection(
    const std::string &ip,
    uint16_t port,
    const std::string &info_hash_raw,
    const std::string &peer_id,
    const std::string &pieces_hashes)
    : ip(ip),
      port(port),
      info_hash_raw(info_hash_raw),
      peer_id(peer_id),
      pieces_hashes(pieces_hashes),
      peer_choked(true),
      current_piece(0),
      current_offset(0) {}

void PeerConnection::handshake() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        throw std::runtime_error("Socket creation failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    std::cout << "Connecting to peer " << ip << ":" << port << "...\n";

    if (connect(sock, (sockaddr *)&addr, sizeof(addr)) != 0)
        throw std::runtime_error("Failed to connect to peer");

    unsigned char handshake_msg[68]{0};
    handshake_msg[0] = 19;
    memcpy(handshake_msg + 1, "BitTorrent protocol", 19);
    memcpy(handshake_msg + 28, info_hash_raw.data(), 20);
    memcpy(handshake_msg + 48, peer_id.data(), 20);

    send(sock, (char *)handshake_msg, 68, 0);

    unsigned char response[68];
    if (recv(sock, (char *)response, 68, 0) != 68)
        throw std::runtime_error("Invalid handshake response");

    if (memcmp(response + 28, info_hash_raw.data(), 20) != 0)
        throw std::runtime_error("info_hash mismatch");

    std::cout << "Handshake successful!\n";

    output.open("downloaded.data", std::ios::binary);
    if (!output)
        throw std::runtime_error("Failed to open output file");

    send_interested();
    receive_messages();
}

void PeerConnection::send_interested() {
    uint32_t len = htonl(1);
    uint8_t id = 2;
    send(sock, (char *)&len, 4, 0);
    send(sock, (char *)&id, 1, 0);
    std::cout << "Sent: interested\n";
}

void PeerConnection::receive_messages() {
    auto start = std::chrono::steady_clock::now();

    while (true) {
        uint32_t len;
        if (recv(sock, (char *)&len, 4, 0) <= 0)
            break;

        len = ntohl(len);
        if (len == 0)
            continue;

        uint8_t id;
        recv(sock, (char *)&id, 1, 0);

        std::string payload(len - 1, '\0');
        if (len > 1)
            recv(sock, payload.data(), len - 1, 0);

        handle_message(id, payload);

        if (peer_choked) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() > 10) {
                std::cout << "Peer did not unchoke, giving up\n";
                break;
            }
        }
    }
}

void PeerConnection::handle_message(uint8_t id, const std::string &payload) {
    if (id == 1) {
        std::cout << "Peer unchoked us\n";
        peer_choked = false;
        request_block();
    } else if (id == 7) {
        handle_piece(payload);
    }
}

void PeerConnection::request_block() {
    if (peer_choked)
        return;

    uint32_t msg_len = htonl(13);
    uint8_t id = 6;

    uint32_t index = htonl(current_piece);
    uint32_t begin = htonl(current_offset);
    uint32_t length = htonl(BLOCK_SIZE);

    send(sock, (char *)&msg_len, 4, 0);
    send(sock, (char *)&id, 1, 0);
    send(sock, (char *)&index, 4, 0);
    send(sock, (char *)&begin, 4, 0);
    send(sock, (char *)&length, 4, 0);

    std::cout << "Requested block: piece " << current_piece
              << " offset " << current_offset << "\n";
}

void PeerConnection::handle_piece(const std::string &payload) {
    if (payload.size() < 8)
        return;

    const char *data = payload.data() + 8;
    size_t data_len = payload.size() - 8;

    piece_buffer.insert(piece_buffer.end(), data, data + data_len);
    current_offset += data_len;

    if (piece_buffer.size() >= BLOCK_SIZE * 16) { // simple full-piece assumption
        if (verify_piece()) {
            std::cout << "Piece " << current_piece << " verified\n";
            output.write(piece_buffer.data(), piece_buffer.size());
            current_piece++;
        } else {
            std::cout << "Piece verification failed\n";
        }

        piece_buffer.clear();
        current_offset = 0;
    }

    request_block();
}

bool PeerConnection::verify_piece() {
    const char *expected = pieces_hashes.data() + current_piece * 20;
    std::string computed = sha1_raw(
        std::string(piece_buffer.begin(), piece_buffer.end()));
    return memcmp(expected, computed.data(), 20) == 0;
}
