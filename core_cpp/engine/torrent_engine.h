#ifndef TORRENT_ENGINE_H
#define TORRENT_ENGINE_H

#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <mutex>

#include "../torrent/torrent_parser.h"
#include "../tracker/tracker_client.h"
#include "../peer/peer_connection.h"
#include "../piece/piece_manager.h"

class TorrentEngine {
public:
    TorrentEngine(const std::string& torrent_path,
                  const std::string& output_path);

    void start();
    void stop();

    float get_progress() const;
    bool is_finished() const;

private:
    void run();

private:
    std::string torrent_path;
    std::string output_path;

    std::atomic<bool> running{false};
    std::atomic<bool> finished{false};

    std::shared_ptr<PieceManager> piece_manager;
    std::shared_ptr<std::fstream> file;
    std::shared_ptr<std::mutex> file_mutex;

    std::vector<std::thread> peer_threads;
};

#endif
