    #include "torrent/torrent_parser.h"
    #include "tracker/tracker_client.h"
    #include <iostream>
    #include "utils/sha1.h"
    #include "utils/url_encode.h"
    #include "peer/peer_connection.h"

    int main() {
        const int MAX_PEERS_TO_TRY = 10;
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

            int attempts = 0;
            bool connected = false;

            for (const auto &p : peers) {
                if (attempts >= MAX_PEERS_TO_TRY) break;
                attempts++;
            
                try {
                    std::cout << "Attempting handshake with "
                              << p.ip << ":" << p.port << "\n";
                
                    PeerConnection peer(
                        p.ip,
                        p.port,
                        info_hash_raw,
                        peer_id,
                        parser.get_pieces_hashes()
                    );
                
                    peer.handshake();
                
                    // If handshake + messaging succeeds,
                    // we stop trying other peers
                    connected = true;
                    break;
                
                } catch (const std::exception &e) {
                    std::cout << "Peer failed ("
                              << p.ip << ":" << p.port
                              << "), trying next...\n";
                }
            }

            if (!connected) {
                std::cout << "No usable peers found\n";
            }
        
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }
