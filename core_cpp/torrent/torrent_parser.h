#ifndef TORRENT_PARSER_H
#define TORRENT_PARSER_H

#include <string>
#include <cstdint>

class TorrentParser {

public:
    explicit TorrentParser(const std::string &file_path);
    void parse();
    void print_info() const;

    const std::string& get_announce() const;
    const std::string& get_info_raw() const;

    const std::string& get_pieces_hashes() const;
    int64_t get_piece_length() const;

private:
    std::string file_path;
    std::string announce;
    int64_t piece_length;
    int64_t file_length;
    std::string info_raw;
    std::string pieces_hashes;

};

#endif
