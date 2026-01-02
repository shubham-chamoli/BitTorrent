#include "torrent_parser.h"
#include "../utils/bencode.h"
#include <fstream>
#include <iostream>
#include <unordered_map>

TorrentParser::TorrentParser(const std::string &file_path)
    : file_path(file_path), piece_length(0), file_length(0) {}

void TorrentParser::parse() {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open torrent file");
    }

    std::string data(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    size_t pos = 0;
    BencodeNode root = decode_bencode(data, pos);

    auto root_dict = std::get<
        std::unordered_map<std::string, BencodeNode>
    >(root.value);

    // ---- announce ----
    if (root_dict.count("announce")) {
        announce = std::get<std::string>(root_dict["announce"].value);
    } else {
        announce = "N/A";
    }

    // ---- info ----
    if (!root_dict.count("info")) {
        throw std::runtime_error("Invalid torrent: missing info dictionary");
    }

    auto info_dict = std::get<
        std::unordered_map<std::string, BencodeNode>
    >(root_dict["info"].value);

    // ---- piece length ----
    if (info_dict.count("piece length")) {
        piece_length = std::get<int64_t>(info_dict["piece length"].value);
    } else {
        throw std::runtime_error("Invalid torrent: missing piece length");
    }

    // ---- single-file vs multi-file ----
    if (info_dict.count("length")) {
        // single-file torrent
        file_length = std::get<int64_t>(info_dict["length"].value);
    } else if (info_dict.count("files")) {
        // multi-file torrent
        file_length = 0;
        auto files = std::get<
            std::vector<BencodeNode>
        >(info_dict["files"].value);

        for (const auto &f : files) {
            auto file_dict = std::get<
                std::unordered_map<std::string, BencodeNode>
            >(f.value);

            file_length += std::get<int64_t>(file_dict["length"].value);
        }
    } else {
        throw std::runtime_error("Invalid torrent: no file length info");
    }
}

void TorrentParser::print_info() const {
    std::cout << "Tracker URL  : " << announce << "\n";
    std::cout << "Piece Length : " << piece_length << "\n";
    std::cout << "Total Size   : " << file_length << "\n";
}

const std::string& TorrentParser::get_announce() const {
    return announce;
}

