#ifndef TRACKER_CLIENT_H
#define TRACKER_CLIENT_H

#include <string>
#include <vector>
#include <cstdint>

struct PeerInfo {
    std::string ip;
    uint16_t port;
};

class TrackerClient {
public:
    explicit TrackerClient(const std::string &tracker_url);

    // Contact tracker and return list of peers
    std::vector<PeerInfo> request_peers(
        const std::string &info_hash,
        const std::string &peer_id,
        int64_t left
    );

private:
    std::string tracker_url;
    bool use_tls;
};

#endif
