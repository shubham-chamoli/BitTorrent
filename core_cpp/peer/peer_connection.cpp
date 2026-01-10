#include "peer_connection.h"
#include "../utils/sha1.h"
#include <cstring>
#include <iostream>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

PeerConnection::PeerConnection(
    const std::string &ip, uint16_t port, const std::string &info_hash_raw,
    const std::string &peer_id, const std::string &pieces_hashes, uint32_t piece_length,
    std::shared_ptr<PieceManager> pm, std::shared_ptr<std::fstream> file, std::shared_ptr<std::mutex> file_mtx)
    : ip(ip), 
      port(port), 
      info_hash_raw(info_hash_raw), 
      peer_id(peer_id),
      pieces_hashes(pieces_hashes), 
      piece_length(piece_length),
      piece_manager(pm), 
      file(file), 
      file_mutex(file_mtx),
      peer_pieces(pieces_hashes.size() / 20, false) 
{
}

void PeerConnection::handshake() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) throw std::runtime_error("Socket creation failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    // Timeout: 5 seconds
    DWORD timeout = 5000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    if (connect(sock, (sockaddr *)&addr, sizeof(addr)) != 0) {
        closesocket(sock);
        // THROW ERROR HERE so main knows it failed
        throw std::runtime_error("Connection failed (Timeout or Refused)");
    }

    // --- Standard Handshake ---
    unsigned char hs[68]{0};
    hs[0] = 19;
    memcpy(hs + 1, "BitTorrent protocol", 19);
    memcpy(hs + 28, info_hash_raw.data(), 20);
    memcpy(hs + 48, peer_id.data(), 20);

    send(sock, (char *)hs, 68, 0);
    unsigned char resp[68];
    if (recv(sock, (char *)resp, 68, 0) != 68) {
        closesocket(sock);
        return;
    }

    if (memcmp(resp + 28, info_hash_raw.data(), 20) != 0) {
        closesocket(sock);
        return;
    }

    std::cout << "Connected to " << ip << "\n";
    send_interested();
    receive_messages();

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#endif
}

void PeerConnection::send_interested() {
    uint32_t len = htonl(1);
    uint8_t id = 2;
    send(sock, (char *)&len, 4, 0);
    send(sock, (char *)&id, 1, 0);
}

void PeerConnection::receive_messages() {
    while (!piece_manager->is_complete()) {
        uint32_t len;
        int bytes_read = recv(sock, (char *)&len, 4, 0);
        
        // --- DEBUGGING LOGIC ---
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                std::cerr << "❌ Connection closed by peer (Graceful EOF)\n";
            } else {
                // Check Windows Error Code
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT) {
                    std::cerr << "❌ Connection timed out (No data for 5s)\n";
                } else {
                    std::cerr << "❌ Connection error: " << err << "\n";
                }
            }
            break;
        }
        // -----------------------

        len = ntohl(len);

        std::cout << "[Raw] Received Message Length: " << len << "\n";

        if (len == 0) continue; // Keep-alive

        uint8_t id;
        recv(sock, (char *)&id, 1, 0);

        std::string payload(len - 1, '\0');
        if (len > 1) {
            int received = 0;
            while(received < len - 1) {
                 int r = recv(sock, payload.data() + received, len - 1 - received, 0);
                 if(r <= 0) break;
                 received += r;
            }
        }

        handle_message(id, payload);
    }
    finished = true;
}

void PeerConnection::handle_message(uint8_t id, const std::string &payload) {
    // --- X-RAY LOGS ---
    switch (id) {
        case 0: std::cout << "[Peer " << ip << "] Choke (ID 0)\n"; break;
        case 1: std::cout << "[Peer " << ip << "] Unchoke (ID 1)\n"; break;
        case 2: std::cout << "[Peer " << ip << "] Interested (ID 2)\n"; break;
        case 3: std::cout << "[Peer " << ip << "] Not Interested (ID 3)\n"; break;
        case 4: std::cout << "[Peer " << ip << "] Have (ID 4)\n"; break;
        case 5: std::cout << "[Peer " << ip << "] Bitfield (ID 5) - Size: " << payload.size() << "\n"; break;
        case 6: std::cout << "[Peer " << ip << "] Request (ID 6)\n"; break;
        case 7: std::cout << "[Peer " << ip << "] Piece (ID 7)\n"; break;
        case 20: std::cout << "[Peer " << ip << "] Extension (ID 20)\n"; break;
        default: std::cout << "[Peer " << ip << "] Unknown Message (ID " << (int)id << ")\n"; break;
    }
    // ------------------

    if (id == 0) { // Choke
        peer_choked = true;
    } 
    else if (id == 1) { // Unchoke
        peer_choked = false;
        // If we need work, ask Brain
        if(current_piece == -1) {
            // FIX: Pass 'peer_pieces' to the manager
            current_piece = piece_manager->get_next_piece_index(peer_pieces);
            current_offset = 0;
            piece_buffer.clear();
        }
        request_block();
    }
    // Inside handle_message switch/case block:


    else if (id == 4) { // Have
        if (payload.size() == 4) {
            // FIX: We must extract 'index' from the payload first!
            uint32_t index = 0;
            memcpy(&index, payload.data(), 4);
            index = ntohl(index);

            if (index < peer_pieces.size()) {
                peer_pieces[index] = true;
                std::cout << "[Peer " << ip << "] Has Piece " << index << "\n";

                // TRIGGER: If we are idle, grab this piece!
                if (current_piece == -1 && !peer_choked) {
                    current_piece = piece_manager->get_next_piece_index(peer_pieces);
                    if (current_piece != -1) {
                         current_offset = 0;
                         piece_buffer.clear();
                         request_block();
                    }
                }
            }
        }
    }
    else if (id == 5) { // Bitfield
        // ... (Existing parsing logic) ... 
        // (after the for loop that fills peer_pieces):
        
        std::cout << "[Peer " << ip << "] Processed Bitfield. Checking for work...\n";
        // TRIGGER: Check if there is anything we can download now
        if (current_piece == -1 && !peer_choked) {
            current_piece = piece_manager->get_next_piece_index(peer_pieces);
            if (current_piece != -1) {
                 current_offset = 0;
                 piece_buffer.clear();
                 request_block();
            }
        }
    }
    else if (id == 7) { // Piece
        handle_piece(payload);
    }
    // Note: We ignore Bitfield (ID 5) and Have (ID 4) for now. 
    // This assumes the peer is a Seed (has everything). 
}

void PeerConnection::request_block() {
    if (peer_choked || current_piece == -1) return;

    // --- CRITICAL CHECK ---
    if (!peer_pieces[current_piece]) {
        std::cout << "[Peer " << ip << "] We want Piece " << current_piece 
                  << " but peer doesn't have it. Waiting...\n";
        
        // Release the piece back to the brain so someone else can try
        piece_manager->mark_piece_failed(current_piece);
        current_piece = -1; 
        
        // Try to find a different piece that THIS peer actually has
        // (For now, we just return and wait for a 'Have' message)
        return;
    }
    // ----------------------

    // Request up to 16KB or remaining bytes
    uint32_t left_in_piece = piece_length - current_offset;
    uint32_t req_len = std::min(BLOCK_SIZE, left_in_piece);

    if (current_offset < piece_length) {
        // Prepare the Request Message (ID 6)
        // Format: <Length 13><ID 6><Index><Begin><Length>
        
        // 1. Create a buffer for the whole message (4 + 1 + 4 + 4 + 4 = 17 bytes)
        std::vector<char> buffer(17);
        
        uint32_t msg_len = htonl(13);
        uint8_t id = 6;
        uint32_t index = htonl(current_piece);
        uint32_t begin = htonl(current_offset);
        uint32_t length = htonl(req_len);

        // 2. Pack data into buffer
        memcpy(buffer.data(), &msg_len, 4);
        memcpy(buffer.data() + 4, &id, 1);
        memcpy(buffer.data() + 5, &index, 4);
        memcpy(buffer.data() + 9, &begin, 4);
        memcpy(buffer.data() + 13, &length, 4);

        // 3. Debug Log
        std::cout << "[Peer " << ip << "] Requesting Piece " << current_piece 
                  << " Offset " << current_offset << " Length " << req_len << "\n";

        // 4. Send in one go
        send(sock, buffer.data(), buffer.size(), 0);
    }
}

void PeerConnection::handle_piece(const std::string &payload) {
    if (payload.size() < 8) return; // Header is 8 bytes
    // payload: <index 4B><begin 4B><block data...>
    
    // We trust TCP ordering for simplicity (real clients check index/begin)
    const char* data = payload.data() + 8;
    size_t len = payload.size() - 8;

    piece_buffer.insert(piece_buffer.end(), data, data + len);
    current_offset += len;

    // Piece Complete?
    if (current_offset >= piece_length) {
        if (verify_piece()) {
            std::cout << "✔ Piece " << current_piece << " verified by " << ip << "\n";
            
            // --- CRITICAL FILE I/O FIX ---
            {
                std::lock_guard<std::mutex> lock(*file_mutex);
                uint64_t file_pos = (uint64_t)current_piece * piece_length;
                file->seekp(file_pos);
                file->write(piece_buffer.data(), piece_buffer.size());
                file->flush(); // Force write to disk
            }
            // -----------------------------

            piece_manager->mark_piece_complete(current_piece);
        } else {
            std::cout << "✖ Piece verification failed " << current_piece << "\n";
            piece_manager->mark_piece_failed(current_piece);
        }

        // Get next job
        piece_buffer.clear();
        current_offset = 0;
        current_piece = piece_manager->get_next_piece_index(peer_pieces);
    }

    request_block();
}

bool PeerConnection::verify_piece() {
    // Note: The last piece of a torrent might be smaller than `piece_length`.
    // SHA1 validation must handle that. For now assuming full pieces.
    const char* expected = pieces_hashes.data() + current_piece * 20;
    std::string computed = sha1_raw(std::string(piece_buffer.begin(), piece_buffer.end()));
    return memcmp(expected, computed.data(), 20) == 0;
}