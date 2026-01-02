#include "torrent/torrent_parser.h"
#include "tracker/tracker_client.h"
#include "peer/peer_connection.h"
#include "utils/sha1.h"
#include "utils/url_encode.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

int main() {
    const int MAX_PARALLEL_PEERS = 5;

    std::atomic<bool> download_started(false);
    std::mutex cout_mutex;

    try {
        TorrentParser parser("sample.torrent");
        parser.parse();
        parser.print_info();

        std::string info_hash_raw = sha1_raw(parser.get_info_raw());
        std::string info_hash = url_encode(info_hash_raw);
        std::string peer_id   = "-BT0001-123456789012";

        TrackerClient tracker(parser.get_announce());
        auto peers = tracker.request_peers(
            info_hash,
            peer_id,
            0
        );

        std::cout << "Peers received: " << peers.size() << "\n";

        // ---- Lambda MUST be inside main ----
        auto try_peer = [&](PeerInfo p) {
            if (download_started.load()) return;

            try {
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "Trying peer "
                              << p.ip << ":" << p.port << "\n";
                }

                PeerConnection peer(
                    p.ip,
                    p.port,
                    info_hash_raw,
                    peer_id,
                    parser.get_pieces_hashes()
                );

                peer.handshake();

                // If this peer reaches unchoke + request,
                // mark success
                download_started.store(true);

                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "Download started with "
                              << p.ip << ":" << p.port << "\n";
                }

            } catch (...) {
                // peer failed, silently ignore
            }
        };

        std::vector<std::thread> threads;

        int launched = 0;
        for (const auto &p : peers) {
            if (launched >= MAX_PARALLEL_PEERS) break;
            launched++;

            threads.emplace_back(try_peer, p);
        }

        // ---- Wait for all threads ----
        for (auto &t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }

        if (!download_started.load()) {
            std::cout << "No peer unchoked us\n";
        }

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}
