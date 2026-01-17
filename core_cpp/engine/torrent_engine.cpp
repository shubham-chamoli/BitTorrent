#include "torrent_engine.h"

#include "../utils/sha1.h"
#include "../utils/url_encode.h"

#include <iostream>
#include <fstream>
#include <chrono>

TorrentEngine::TorrentEngine(
    const std::string& torrent_path,
    const std::string& output_path
)
    : torrent_path(torrent_path),
      output_path(output_path) {}

void TorrentEngine::start() {
    if (running) return;

    running = true;
    finished = false;

    std::thread(&TorrentEngine::run, this).detach();
}

void TorrentEngine::run() {
    try {
        // --------------------------------------
        // 1. Parse Torrent
        // --------------------------------------
        TorrentParser parser(torrent_path);
        parser.parse();
        parser.print_info();

        std::string info_hash_raw = sha1_raw(parser.get_info_raw());
        std::string info_hash = url_encode(info_hash_raw);
        std::string peer_id = "-qB4030-123456789012";

        // --------------------------------------
        // 2. Shared State
        // --------------------------------------
        piece_manager = std::make_shared<PieceManager>(
            parser.get_pieces_hashes(),
            parser.get_piece_length()
        );

        file = std::make_shared<std::fstream>(
            output_path,
            std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc
        );

        file_mutex = std::make_shared<std::mutex>();

        // --------------------------------------
        // 3. Tracker + Peers
        // --------------------------------------
        TrackerClient tracker(parser.get_trackers()[0]);

        auto peers = tracker.request_peers(
            info_hash,
            peer_id,
            parser.get_file_length()
        );

        std::cout << "Found " << peers.size() << " peers\n";

        const int MAX_PEERS = 10;

        for (const auto& p : peers) {
            if (!running) break;
            if ((int)peer_threads.size() >= MAX_PEERS) break;

            peer_threads.emplace_back([&, p]() {
                try {
                    PeerConnection peer(
                        p.ip,
                        p.port,
                        info_hash_raw,
                        peer_id,
                        parser.get_pieces_hashes(),
                        parser.get_piece_length(),
                        piece_manager,
                        file,
                        file_mutex
                    );
                    peer.handshake();
                } catch (...) {
                    // Silent failure (normal in BitTorrent)
                }
            });
        }

        // --------------------------------------
        // 4. Wait for peers
        // --------------------------------------
        for (auto& t : peer_threads) {
            if (t.joinable())
                t.join();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Engine error: " << e.what() << "\n";
    }

    finished = true;
    running = false;
}

void TorrentEngine::stop() {
    running = false;
}

float TorrentEngine::get_progress() const {
    if (!piece_manager) return 0.0f;
    return piece_manager->get_progress_ratio();
}

bool TorrentEngine::is_finished() const {
    return finished;
}
