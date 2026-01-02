#include "url_encode.h"
#include <sstream>
#include <iomanip>

std::string url_encode(const std::string &data) {
    std::ostringstream encoded;
    for (unsigned char c : data) {
        // Unreserved characters according to RFC 3986
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << '%' << std::uppercase
                    << std::hex << std::setw(2)
                    << std::setfill('0') << (int)c;
        }
    }
    return encoded.str();
}
