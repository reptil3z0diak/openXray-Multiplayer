#include "AssetIntegrityCheck.h"

#include <algorithm>

namespace xrmp::anticheat
{
namespace
{
void hashBytes(std::uint32_t& hash, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
}
} // namespace

AssetIntegrityCheck::AssetIntegrityCheck(net::HandshakePolicy policy) : policy_(std::move(policy)) {}

bool AssetIntegrityCheck::validate(const net::HandshakeRequest& request, std::string* error) const
{
    if (request.assetChecksum != policy_.assetChecksum)
    {
        if (error)
            *error = "asset integrity checksum mismatch";
        return false;
    }
    if (request.scriptChecksum != policy_.scriptChecksum)
    {
        if (error)
            *error = "script integrity checksum mismatch";
        return false;
    }
    if (request.configChecksum != policy_.configChecksum)
    {
        if (error)
            *error = "config integrity checksum mismatch";
        return false;
    }
    return true;
}

net::Checksum AssetIntegrityCheck::computeChecksum(std::string_view data)
{
    std::uint32_t hash = 2166136261u;
    hashBytes(hash, data.data(), data.size());

    net::Checksum checksum{};
    for (std::size_t index = 0; index < checksum.size(); ++index)
    {
        checksum[index] = static_cast<net::Byte>((hash >> ((index % 4) * 8)) & 0xFF);
        hash = (hash * 16777619u) ^ static_cast<std::uint32_t>(index * 31u + 17u);
    }
    return checksum;
}

net::Checksum AssetIntegrityCheck::buildManifestChecksum(const std::vector<IntegrityManifestEntry>& entries)
{
    std::vector<IntegrityManifestEntry> sorted = entries;
    std::sort(sorted.begin(), sorted.end(), [](const IntegrityManifestEntry& left, const IntegrityManifestEntry& right) {
        return left.path < right.path;
    });

    std::string canonical;
    for (const auto& entry : sorted)
    {
        canonical += entry.path;
        canonical.push_back('\n');
        canonical.append(reinterpret_cast<const char*>(entry.content.data()), entry.content.size());
        canonical.push_back('\n');
    }
    return computeChecksum(canonical);
}

IntegrityManifest AssetIntegrityCheck::buildManifest(const std::vector<IntegrityManifestEntry>& assets,
    const std::vector<IntegrityManifestEntry>& scripts, const std::vector<IntegrityManifestEntry>& configs)
{
    IntegrityManifest manifest;
    manifest.assetChecksum = buildManifestChecksum(assets);
    manifest.scriptChecksum = buildManifestChecksum(scripts);
    manifest.configChecksum = buildManifestChecksum(configs);
    return manifest;
}
} // namespace xrmp::anticheat
