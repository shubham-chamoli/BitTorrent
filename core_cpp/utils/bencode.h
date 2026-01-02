#ifndef BENCODE_H
#define BENCODE_H

#include<string>
#include<vector>
#include<unordered_map>
#include<variant>
#include <cstdint>
#include <cstddef>


class BencodeNode;

// A bencode value can be: int, string, list, or dictionary
using BencodeValue=std::variant<
    int64_t,
    std::string,
    std::vector<BencodeNode>,
    std::unordered_map<std::string,BencodeNode>
>;

class BencodeNode{
public:
    BencodeValue value;
};

// Decode bencoded data starting at position `pos`
BencodeNode decode_bencode(const std::string &data,size_t &pos);

#endif