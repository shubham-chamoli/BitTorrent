#include "torrent/torrent_parser.h"
#include <iostream>

int main() {
    try {
        // Put a real .torrent file path here
        TorrentParser parser("sample.torrent");
        parser.parse();
        parser.print_info();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
