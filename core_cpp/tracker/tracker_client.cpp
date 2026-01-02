#include "tracker_client.h"
#include "../utils/bencode.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unordered_map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

TrackerClient::TrackerClient(const std::string &tracker_url)
    : tracker_url(tracker_url) {}

static void init_winsock() {
#ifdef _WIN32
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        initialized = true;
    }
#endif
}

std::vector<PeerInfo> TrackerClient::request_peers(
    const std::string &info_hash,
    const std::string &peer_id,
    int64_t left
) {
    init_winsock();

    // ---- Parse tracker URL (http://host:port/path) ----
    std::string url = tracker_url;
    if (url.find("http://") == 0) {
        url = url.substr(7);
    }

    size_t slash = url.find('/');
    std::string host = url.substr(0, slash);
    std::string path = url.substr(slash);

    std::string port = "80";
    size_t colon = host.find(':');
    if (colon != std::string::npos) {
        port = host.substr(colon + 1);
        host = host.substr(0, colon);
    }

    // ---- Build HTTP GET request ----
    std::ostringstream request;
    request << "GET " << path
            << "?info_hash=" << info_hash
            << "&peer_id=" << peer_id
            << "&port=6881"
            << "&uploaded=0"
            << "&downloaded=0"
            << "&left=" << left
            << "&compact=1 HTTP/1.1\r\n"
            << "Host: " << host << "\r\n"
            << "Connection: close\r\n\r\n";

    // ---- DNS lookup ----
    addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) {
        throw std::runtime_error("DNS lookup failed");
    }

    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("Socket creation failed");
    }

    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
        throw std::runtime_error("Connection to tracker failed");
    }

    // ---- Send request ----
    std::string req = request.str();
    send(sock, req.c_str(), (int)req.size(), 0);

    // ---- Receive response ----
    std::string response;
    char buffer[4096];
    int bytes;
    while ((bytes = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytes);
    }

#ifdef _WIN32
    closesocket(sock);
#endif
    freeaddrinfo(res);

    // ---- Strip HTTP headers ----
    size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        throw std::runtime_error("Invalid HTTP response");
    }

    std::string body = response.substr(header_end + 4);

    // ---- Decode bencoded tracker response ----
    size_t pos = 0;
    BencodeNode root = decode_bencode(body, pos);

    auto dict = std::get<
        std::unordered_map<std::string, BencodeNode>
    >(root.value);

    std::vector<PeerInfo> peers;

    // ---- Compact peer list ----
    if (dict.count("peers")) {
        std::string peer_bytes = std::get<std::string>(dict["peers"].value);

        for (size_t i = 0; i + 6 <= peer_bytes.size(); i += 6) {
            PeerInfo p;
            p.ip = std::to_string((unsigned char)peer_bytes[i]) + "." +
                   std::to_string((unsigned char)peer_bytes[i + 1]) + "." +
                   std::to_string((unsigned char)peer_bytes[i + 2]) + "." +
                   std::to_string((unsigned char)peer_bytes[i + 3]);

            p.port = ((unsigned char)peer_bytes[i + 4] << 8) |
                      (unsigned char)peer_bytes[i + 5];

            peers.push_back(p);
        }
    }

    return peers;
}
