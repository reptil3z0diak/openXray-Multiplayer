#pragma once

#include "Handshake.h"

#include <string>
#include <string_view>
#include <vector>

namespace xrmp::anticheat
{
struct IntegrityManifestEntry
{
    std::string path;
    net::Bytes content;
};

struct IntegrityManifest
{
    net::Checksum assetChecksum{};
    net::Checksum scriptChecksum{};
    net::Checksum configChecksum{};
};

class AssetIntegrityCheck
{
public:
    explicit AssetIntegrityCheck(net::HandshakePolicy policy = {});

    bool validate(const net::HandshakeRequest& request, std::string* error) const;

    static net::Checksum computeChecksum(std::string_view data);
    static net::Checksum buildManifestChecksum(const std::vector<IntegrityManifestEntry>& entries);
    static IntegrityManifest buildManifest(const std::vector<IntegrityManifestEntry>& assets,
        const std::vector<IntegrityManifestEntry>& scripts, const std::vector<IntegrityManifestEntry>& configs);

private:
    net::HandshakePolicy policy_{};
};
} // namespace xrmp::anticheat
