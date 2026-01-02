#include "bencode.h"
#include <stdexcept>
#include <cctype>

// Decode string: <len>:<bytes>
static std::string decode_string(const std::string &data, size_t &pos) {
    size_t colon = data.find(':', pos);
    if (colon == std::string::npos) {
        throw std::runtime_error("Invalid bencode string");
    }
    int length = std::stoi(data.substr(pos, colon - pos));
    pos = colon + 1;
    std::string result = data.substr(pos, length);
    pos += length;
    return result;
}

BencodeNode decode_bencode(const std::string &data, size_t &pos) {
    if (pos >= data.size()) {
        throw std::runtime_error("Unexpected end of data");
    }

    // Integer: i<number>e
    if (data[pos] == 'i') {
        pos++;
        size_t end = data.find('e', pos);
        if (end == std::string::npos) {
            throw std::runtime_error("Invalid bencode integer");
        }
        int64_t number = std::stoll(data.substr(pos, end - pos));
        pos = end + 1;
        return { number };
    }

    // String: <len>:<bytes>
    if (std::isdigit(static_cast<unsigned char>(data[pos]))) {
        return { decode_string(data, pos) };
    }

    // List: l<items>e
    if (data[pos] == 'l') {
        pos++;
        std::vector<BencodeNode> list;
        while (data[pos] != 'e') {
            list.push_back(decode_bencode(data, pos));
        }
        pos++; // skip 'e'
        return { list };
    }

    // Dictionary: d<key><value>e
    if (data[pos] == 'd') {
        pos++;
        std::unordered_map<std::string, BencodeNode> dict;
        while (data[pos] != 'e') {
            // keys are strings
            auto keyNode = decode_bencode(data, pos);
            auto key = std::get<std::string>(keyNode.value);
            dict[key] = decode_bencode(data, pos);
        }
        pos++; // skip 'e'
        return { dict };
    }

    throw std::runtime_error("Invalid bencode format");
}
