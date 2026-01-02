#include "torrent/torrent_parser.h"
#include "tracker/tracker_client.h"
#include <iostream>

int main() {
    try {
        TorrentParser parser("sample.torrent");
        parser.parse();
        parser.print_info();

        std::string info_hash = "12345678901234567890"; // placeholder
        std::string peer_id   = "-BT0001-123456789012";

        TrackerClient tracker(parser.get_announce());
        auto peers = tracker.request_peers(
            info_hash,
            peer_id,
            0
        );

        std::cout << "Peers received: " << peers.size() << "\n";
        for (const auto &p : peers) {
            std::cout << p.ip << ":" << p.port << "\n";
        }

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}
