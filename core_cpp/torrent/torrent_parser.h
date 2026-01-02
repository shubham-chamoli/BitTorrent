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

private:
    std::string file_path;
    std::string announce;
    int64_t piece_length;
    int64_t file_length;
};

#endif
