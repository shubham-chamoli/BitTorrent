#ifndef PEER_CONNECTION_H
#define PEER_CONNECTION_H

#include <string>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

class PeerConnection {
public:
    PeerConnection(
        const std::string &ip,
        uint16_t port,
        const std::string &info_hash_raw,
        const std::string &peer_id
    );

    void handshake();

private:
    std::string ip;
    uint16_t port;
    std::string info_hash_raw;
    std::string peer_id;

#ifdef _WIN32
    SOCKET sock;   
#endif

    void send_interested();
    void receive_messages();
    void handle_message(uint8_t id, const std::string &payload);
};

#endif
