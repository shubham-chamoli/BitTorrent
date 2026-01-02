#ifndef SHA1_H
#define SHA1_H

#include <string>

// Returns raw 20-byte SHA1 hash
std::string sha1_raw(const std::string &data);

#endif
