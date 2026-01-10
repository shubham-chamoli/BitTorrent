#ifndef PEER_CONNECTION_H
#define PEER_CONNECTION_H

#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <memory> 
#include <mutex>
#include "../piece/piece_manager.h"

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
        const std::string &pieces_hashes,
        uint32_t piece_length,
        std::shared_ptr<PieceManager> pm,      // Shared Brain
        std::shared_ptr<std::fstream> file,    // Shared File
        std::shared_ptr<std::mutex> file_mtx   // Shared File Lock
    );

    void handshake();
    bool is_finished() const { return finished; }

private:
    std::vector<bool> peer_pieces;
    std::string ip;
    uint16_t port;
    std::string info_hash_raw;
    std::string peer_id;
    std::vector<char> piece_buffer;
    
    // Shared Resources
    std::shared_ptr<PieceManager> piece_manager;
    std::shared_ptr<std::fstream> file;
    std::shared_ptr<std::mutex> file_mutex;

    int current_piece = -1; // -1 means "I have no work"
    uint32_t current_offset = 0;
    
    std::string pieces_hashes;
    uint32_t piece_length;
    bool peer_choked = true;
    bool finished = false;

#ifdef _WIN32
    SOCKET sock;   
#endif

    void send_interested();
    void receive_messages();
    void handle_message(uint8_t id, const std::string &payload);
    void request_block();
    void handle_piece(const std::string &payload);
    bool verify_piece();
};

#endif