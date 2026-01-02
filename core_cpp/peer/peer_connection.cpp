#include "../utils/sha1.h"
#include "peer_connection.h"
#include <iostream>
#include <cstring>
#include <fstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

PeerConnection::PeerConnection(
    const std::string &ip,
    uint16_t port,
    const std::string &info_hash_raw,
    const std::string &peer_id,
    const std::string &pieces_hashes
)
    : ip(ip), port(port),
      info_hash_raw(info_hash_raw),
      peer_id(peer_id),
      pieces_hashes(pieces_hashes) {}

void PeerConnection::handshake() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("Socket creation failed");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    std::cout << "Connecting to peer " << ip << ":" << port << "...\n";

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        throw std::runtime_error("Failed to connect to peer");
    }

    // ---- Build handshake ----
    unsigned char handshake[68];
    memset(handshake, 0, sizeof(handshake));

    handshake[0] = 19;
    memcpy(handshake + 1, "BitTorrent protocol", 19);
    memcpy(handshake + 28, info_hash_raw.data(), 20);
    memcpy(handshake + 48, peer_id.data(), 20);

    send(sock, (char*)handshake, 68, 0);

    // ---- Receive handshake ----
    unsigned char response[68];
    int received = recv(sock, (char*)response, 68, 0);
    if (received != 68) {
        throw std::runtime_error("Invalid handshake response");
    }

    // ---- Verify protocol string ----
    if (response[0] != 19 ||
        memcmp(response + 1, "BitTorrent protocol", 19) != 0) {
        throw std::runtime_error("Invalid BitTorrent protocol");
    }

    // ---- Verify info_hash ----
    if (memcmp(response + 28, info_hash_raw.data(), 20) != 0) {
        throw std::runtime_error("info_hash mismatch");
    }

    std::string peer_id_remote(
        reinterpret_cast<char*>(response + 48), 20
    );

    std::cout << "Handshake successful!\n";
    std::cout << "Peer ID: " << peer_id_remote << "\n";

    
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#endif

    send_interested();
    receive_messages();

}

void PeerConnection::send_interested() {
    uint32_t len = htonl(1); // length = 1 (only message ID)
    uint8_t id = 2;          // interested

    send(sock, (char*)&len, 4, 0);
    send(sock, (char*)&id, 1, 0);

    std::cout << "Sent: interested\n";
}

void PeerConnection::receive_messages() {
    while (true) {
        uint32_t len;
        int r = recv(sock, (char*)&len, 4, 0);
        if (r <= 0) break;

        len = ntohl(len);

        if (len == 0) {
            std::cout << "Keep-alive received\n";
            continue;
        }

        uint8_t id;
        recv(sock, (char*)&id, 1, 0);

        std::string payload;
        if (len > 1) {
            payload.resize(len - 1);
            recv(sock, payload.data(), len - 1, 0);
        }

        handle_message(id, payload);
    }
}

void PeerConnection::handle_message(uint8_t id,
                                    const std::string &payload) {
    switch (id) {
        case 0:
            std::cout << "Peer choked us\n";
            break;

        case 1:
            std::cout << "Peer unchoked us\n";

            // Request first block of first piece (16 KB)
            request_block(
                0,              // piece index
                0,              // block offset
                16 * 1024       // block size
            );
            break;

        case 5:
            std::cout << "Received bitfield ("
                      << payload.size()
                      << " bytes)\n";
            break;

        case 7: {
            uint32_t index =
                ntohl(*(uint32_t*)&payload[0]);
            uint32_t begin =
                ntohl(*(uint32_t*)&payload[4]);
                
            const char* data = payload.data() + 8;
            uint32_t data_len = payload.size() - 8;
                
            if (index != current_piece) break;
                
            if (piece_buffer.empty()) {
                piece_buffer.resize(data_len);
            }
        
            memcpy(piece_buffer.data() + begin, data, data_len);
            bytes_received += data_len;
        
            std::cout << "Received block "
                      << begin << " (" << data_len << " bytes)\n";
        
            // Piece complete
            if (bytes_received >= piece_buffer.size()) {
                verify_and_write_piece();
            }

    break;
}


        default:
            std::cout << "Received message id "
                      << (int)id
                      << "\n";
    }
}


void PeerConnection::request_block(uint32_t index,
                                   uint32_t begin,
                                   uint32_t length) {
    uint32_t msg_len = htonl(13);
    uint8_t id = 6;

    uint32_t i = htonl(index);
    uint32_t b = htonl(begin);
    uint32_t l = htonl(length);

    send(sock, (char*)&msg_len, 4, 0);
    send(sock, (char*)&id, 1, 0);
    send(sock, (char*)&i, 4, 0);
    send(sock, (char*)&b, 4, 0);
    send(sock, (char*)&l, 4, 0);

    std::cout << "Requested block: piece "
              << index << " offset " << begin << "\n";
}

void PeerConnection::verify_and_write_piece() {
    std::string piece_data(
        piece_buffer.begin(),
        piece_buffer.end()
    );

    std::string hash = sha1_raw(piece_data);

    const std::string& all_hashes = pieces_hashes;

    std::string expected =
        all_hashes.substr(current_piece * 20, 20);

    if (hash == expected) {
        std::cout << "Piece "
                  << current_piece
                  << " verified successfully\n";

        std::ofstream out("output.data",
                          std::ios::binary | std::ios::app);
        out.write(piece_buffer.data(),
                  piece_buffer.size());
    } else {
        std::cout << "Piece "
                  << current_piece
                  << " hash mismatch\n";
    }

    piece_buffer.clear();
    bytes_received = 0;
    current_piece++;
}
