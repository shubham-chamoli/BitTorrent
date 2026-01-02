#ifndef PEER_CONNECTION_H
#define PEER_CONNECTION_H

#include <string>
#include <cstdint>

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
};

#endif
