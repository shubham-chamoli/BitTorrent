#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "../core_cpp/torrent/torrent_parser.h"
#include "../core_cpp/tracker/tracker_client.h"
#include "../core_cpp/peer/peer_connection.h"
#include "../core_cpp/piece/piece_manager.h"
#include "../core_cpp/utils/sha1.h"
#include "../core_cpp/utils/url_encode.h"
#include <thread>
#include <atomic>
#include <vector>
#include <iostream>

namespace py = pybind11;

// Wrapper class to manage the C++ threads
class BitTorrentEngine {
public:
    BitTorrentEngine(const std::string& torrent_path, const std::string& output_path) 
        : torrent_path(torrent_path), output_path(output_path), running(false) {}

    void start() {
        if (running) return;
        running = true;
        worker_thread = std::thread(&BitTorrentEngine::run_loop, this);
    }

    void stop() {
        running = false;
        if (worker_thread.joinable()) worker_thread.join();
    }

    float get_progress() const {
        if (!piece_manager) return 0.0f;
        return piece_manager->get_progress_ratio(); 
    }

private:
    std::string torrent_path;
    std::string output_path;
    std::atomic<bool> running;
    std::thread worker_thread;
    std::shared_ptr<PieceManager> piece_manager;

    void run_loop() {
        try {
            TorrentParser parser(torrent_path);
            parser.parse();
            
            std::string info_hash_raw = sha1_raw(parser.get_info_raw());
            std::string info_hash = url_encode(info_hash_raw);
            std::string peer_id = "-qB4030-123456789012";

            piece_manager = std::make_shared<PieceManager>(
                parser.get_pieces_hashes(), 
                parser.get_piece_length()
            );

            auto file = std::make_shared<std::fstream>(
                output_path, 
                std::ios::binary | std::ios::out | std::ios::in | std::ios::trunc
            );
            auto file_mutex = std::make_shared<std::mutex>();

            TrackerClient tracker(parser.get_trackers()[0]);
            // Send file_length to tracker
            auto peers = tracker.request_peers(info_hash, peer_id, parser.get_file_length());

            std::vector<std::thread> threads;
            
            for (const auto& p : peers) {
                if (!running) break;
                threads.emplace_back([&, p]() {
                    try {
                        PeerConnection peer(
                            p.ip, p.port, info_hash_raw, peer_id,
                            parser.get_pieces_hashes(), parser.get_piece_length(),
                            piece_manager, file, file_mutex
                        );
                        peer.handshake();
                    } catch (...) {}
                });
            }

            for (auto& t : threads) {
                if (t.joinable()) t.join();
            }
        } catch (const std::exception& e) {
            std::cerr << "Engine Error: " << e.what() << "\n";
        }
    }
};

PYBIND11_MODULE(bittorrent_core, m) {
    py::class_<BitTorrentEngine>(m, "BitTorrentEngine")
        .def(py::init<std::string, std::string>())
        .def("start", &BitTorrentEngine::start)
        .def("stop", &BitTorrentEngine::stop)
        .def("get_progress", &BitTorrentEngine::get_progress);
}