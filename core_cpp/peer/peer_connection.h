#ifndef PEER_CONNECTION_H
#define PEER_CONNECTION_H

#include <string>
#include <cstdint>
#include <vector>
#include <fstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

constexpr uint32_t BLOCK_SIZE = 16 * 1024; // 16 KB

class PeerConnection {
public:
    PeerConnection(
        const std::string &ip,
        uint16_t port,
        const std::string &info_hash_raw,
        const std::string &peer_id,
        const std::string &pieces_hashes 
    );

    void handshake();

private:
    std::string ip;
    uint16_t port;
    std::string info_hash_raw;
    std::string peer_id;
    std::vector<char> piece_buffer;
    uint32_t current_piece = 0;
    uint32_t bytes_received = 0;
    std::string pieces_hashes;
    bool peer_choking = true;
    uint32_t current_offset = 0;
    bool peer_choked = true;
    std::ofstream output;

#ifdef _WIN32
    SOCKET sock;   
#endif

    void send_interested();
    void receive_messages();
    void handle_message(uint8_t id, const std::string &payload);
    void request_block(uint32_t index, uint32_t begin, uint32_t length);
    void verify_and_write_piece();
    void request_block();
    void handle_piece(const std::string &payload);
    bool verify_piece();


};

#endif
