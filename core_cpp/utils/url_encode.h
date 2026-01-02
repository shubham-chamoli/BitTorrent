#ifndef URL_ENCODE_H
#define URL_ENCODE_H

#include <string>

// URL-encode binary data (used for info_hash)
std::string url_encode(const std::string &data);

#endif
