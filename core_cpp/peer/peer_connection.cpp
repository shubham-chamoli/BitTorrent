#include "peer_connection.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

PeerConnection::PeerConnection(
    const std::string &ip,
    uint16_t port,
    const std::string &info_hash_raw,
    const std::string &peer_id
)
    : ip(ip), port(port),
      info_hash_raw(info_hash_raw),
      peer_id(peer_id) {}

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
}
