#include "torrent/torrent_parser.h"
#include "tracker/tracker_client.h"
#include "peer/peer_connection.h"
#include "piece/piece_manager.h" // Include the new manager
#include "utils/sha1.h"
#include "utils/url_encode.h"

#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <memory>
#include <fstream>
#include <mutex>

int main()
{
    try
    {
        TorrentParser parser("sample.torrent");
        parser.parse();
        parser.print_info();

        std::string info_hash_raw = sha1_raw(parser.get_info_raw());
        std::string info_hash = url_encode(info_hash_raw);
        // Mimic qBittorrent 4.3.0 (-qB4030-)
        std::string peer_id = "-qB4030-123456789012";

        // 1. Setup Shared Resources
        auto piece_manager = std::make_shared<PieceManager>(
            parser.get_pieces_hashes(),
            parser.get_piece_length());

        // Open file ONCE in binary mode
        auto file = std::make_shared<std::fstream>(
            "output_file.iso", // Use name from torrent info if available
            std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc);

        // Pre-allocate file size (Prevents fragmentation)
        // Note: For large files, use specific OS calls, but this is a portable hack
        /*
        file->seekp(parser.get_file_length() - 1);
        file->write("", 1);
        */

        auto file_mutex = std::make_shared<std::mutex>();

        // 2. Get Peers
        TrackerClient tracker(parser.get_trackers()[0]);
        int64_t size = parser.get_file_length();
        auto peers = tracker.request_peers(info_hash, peer_id, size);

        std::cout << "Found " << peers.size() << " peers. Starting swarm...\n";

        // 3. Launch Threads
        std::vector<std::thread> threads;
        const int MAX_THREADS = 10;
        int active_threads = 0;

        for (const auto &p : peers)
        {
            if (active_threads >= MAX_THREADS)
                break;

            threads.emplace_back([&, p]()
                                 {
            try {
        std::cout << "Attempting to connect to peer: " << p.ip << ":" << p.port << "...\n";
        PeerConnection peer(
            p.ip, p.port, info_hash_raw, peer_id,
            parser.get_pieces_hashes(), parser.get_piece_length(),
            piece_manager, file, file_mutex 
        );
        peer.handshake();
    } catch (const std::exception& e) {
        std::cerr << "❌ Peer error (" << p.ip << "): " << e.what() << "\n";
    } catch (...) {
        std::cerr << "❌ Unknown error with peer " << p.ip << "\n";
    } });
            active_threads++;
        }

        for (auto &t : threads)
        {
            if (t.joinable())
                t.join();
        }

        std::cout << "Download loop finished.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}