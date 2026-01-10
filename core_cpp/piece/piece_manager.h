#ifndef PIECE_MANAGER_H
#define PIECE_MANAGER_H

#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <iostream>

class PieceManager {
public:
    PieceManager(const std::string& pieces_hashes, uint32_t piece_length)
        : pieces_hashes(pieces_hashes), piece_length(piece_length) {
        
        // Calculate total number of pieces (20 bytes per SHA1 hash)
        total_pieces = pieces_hashes.size() / 20;
        
        // Initialize state: false = missing
        bitfield.resize(total_pieces, false);
        in_progress.resize(total_pieces, false);
    }

    // Returns the index of the next piece a peer should download
    // Returns -1 if all pieces are done or busy
    // Updated: Now accepts the peer's bitfield to find a matching piece
    int get_next_piece_index(const std::vector<bool>& peer_has_piece) {
        std::lock_guard<std::mutex> lock(mtx);
        
        for (int i = 0; i < total_pieces; ++i) {
            // Logic: We need it AND no one is working on it AND peer has it
            if (!bitfield[i] && !in_progress[i]) {
                
                // Check if peer actually has this piece
                if (i < peer_has_piece.size() && peer_has_piece[i]) {
                    in_progress[i] = true; 
                    return i;
                }
            }
        }
        return -1; // No matching work found
    }

    // Called when a peer successfully verifies a hash
    void mark_piece_complete(int index) {
        std::lock_guard<std::mutex> lock(mtx);
        bitfield[index] = true;
        in_progress[index] = false;
        
        // Calculate progress %
        int count = std::count(bitfield.begin(), bitfield.end(), true);
        std::cout << "Progress: " << count << "/" << total_pieces << " pieces\n";
    }

    // Called when hash verification fails (release the piece for others)
    void mark_piece_failed(int index) {
        std::lock_guard<std::mutex> lock(mtx);
        in_progress[index] = false; 
    }

    bool is_complete() {
        std::lock_guard<std::mutex> lock(mtx);
        return std::all_of(bitfield.begin(), bitfield.end(), [](bool b){ return b; });
    }

private:
    std::string pieces_hashes;
    uint32_t piece_length;
    int total_pieces;

    std::vector<bool> bitfield;      // true = we have this piece
    std::vector<bool> in_progress;   // true = a peer is currently downloading this
    std::mutex mtx;                  // Protects the vectors
};

#endif