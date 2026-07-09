#include "EditorAssetThumbnailCache.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>

namespace editor {
namespace {

uint64_t HashString(uint64_t hash, std::string_view value) {
    constexpr uint64_t kPrime = 1099511628211ull;
    for (const char ch : value) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= kPrime;
    }
    return hash;
}

uint64_t HashValue(uint64_t hash, uint64_t value) {
    constexpr uint64_t kPrime = 1099511628211ull;
    for (int shift = 0; shift < 64; shift += 8) {
        hash ^= (value >> shift) & 0xffull;
        hash *= kPrime;
    }
    return hash;
}

std::string HashHex(uint64_t hash) {
    std::ostringstream stream;
    stream << std::hex << hash;
    return stream.str();
}

bool IsValidPixelPayload(const EditorAssetThumbnailPixelData& pixels) {
    return pixels.width > 0 &&
        pixels.height > 0 &&
        pixels.rowPitch >= pixels.width * 4u &&
        pixels.rgba8.size() >= static_cast<size_t>(pixels.rowPitch) * pixels.height;
}

uint64_t PayloadBytes(const EditorAssetThumbnailPixelData& pixels) {
    return static_cast<uint64_t>(pixels.rgba8.size());
}

} // namespace

void EditorAssetThumbnailCacheStore::Configure(EditorAssetThumbnailCachePolicy policy) {
    policy_ = std::move(policy);
    if (policy_.maxMemoryBytes == 0) {
        policy_.maxMemoryBytes = 1;
    }
    EvictIfNeeded();
}

void EditorAssetThumbnailCacheStore::ClearMemory() {
    entries_.clear();
    telemetry_.residentBytes = 0;
    telemetry_.residentEntries = 0;
}

void EditorAssetThumbnailCacheStore::ClearAll() {
    ClearMemory();
    telemetry_ = {};
    if (!policy_.persistentCacheEnabled || policy_.persistentRoot.empty()) {
        return;
    }
    std::error_code error;
    std::filesystem::remove_all(policy_.persistentRoot, error);
}

bool EditorAssetThumbnailCacheStore::TryLoad(
    const EditorAssetThumbnailCacheKey& key,
    EditorAssetThumbnailPixelData& outPixels,
    std::string& outDetail) {
    const std::string stableKey = StableKey(key);
    const auto it = entries_.find(stableKey);
    if (it != entries_.end()) {
        it->second.lastUse = nextUse_++;
        outPixels = it->second.pixels;
        ++telemetry_.hits;
        outDetail = "Thumbnail cache memory hit.";
        return true;
    }

    if (policy_.persistentCacheEnabled && LoadFromDisk(stableKey, outPixels)) {
        PutMemory(stableKey, outPixels, false);
        ++telemetry_.hits;
        outDetail = "Thumbnail cache disk hit.";
        return true;
    }

    ++telemetry_.misses;
    outDetail = "Thumbnail cache miss.";
    return false;
}

bool EditorAssetThumbnailCacheStore::Store(
    const EditorAssetThumbnailCacheKey& key,
    const EditorAssetThumbnailPixelData& pixels,
    bool fallbackIcon,
    std::string& outDetail) {
    if (!IsValidPixelPayload(pixels)) {
        outDetail = "Thumbnail cache rejected invalid pixel payload.";
        return false;
    }
    if (fallbackIcon && !policy_.storeFallbackIcons) {
        outDetail = "Thumbnail cache policy skipped fallback icon payload.";
        return false;
    }

    const std::string stableKey = StableKey(key);
    PutMemory(stableKey, pixels, fallbackIcon);
    if (policy_.persistentCacheEnabled) {
        StoreToDisk(stableKey, pixels);
    }
    ++telemetry_.stores;
    outDetail = "Thumbnail cache stored payload.";
    return true;
}

std::string EditorAssetThumbnailCacheStore::StableKey(const EditorAssetThumbnailCacheKey& key) const {
    uint64_t hash = 1469598103934665603ull;
    hash = HashString(hash, key.thumbnailKey);
    hash = HashString(hash, key.assetId);
    hash = HashString(hash, key.sourcePath);
    hash = HashValue(hash, static_cast<uint64_t>(key.kind));
    hash = HashValue(hash, static_cast<uint64_t>(key.previewKind));
    hash = HashValue(hash, key.sourceTimestamp);
    hash = HashValue(hash, key.previewGeneration);
    hash = HashValue(hash, key.previewVersion);
    hash = HashValue(hash, key.width);
    hash = HashValue(hash, key.height);
    hash = HashValue(hash, key.vertexCount);
    hash = HashValue(hash, key.faceCount);
    hash = HashValue(hash, key.materialSlotCount);
    hash = HashValue(hash, key.materialTextureCount);
    hash = HashValue(hash, key.materialTextureTimestamp);
    hash = HashValue(hash, key.boundsRadiusMilli);
    hash = HashValue(hash, key.byteSize);
    return HashHex(hash);
}

std::filesystem::path EditorAssetThumbnailCacheStore::CacheFilePath(
    const std::string& stableKey) const {
    return policy_.persistentRoot / (stableKey + ".rgba8thumb");
}

bool EditorAssetThumbnailCacheStore::LoadFromDisk(
    const std::string& stableKey,
    EditorAssetThumbnailPixelData& outPixels) const {
    if (policy_.persistentRoot.empty()) {
        return false;
    }
    std::ifstream file(CacheFilePath(stableKey), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::string magic;
    file >> magic;
    if (magic != "CG5THUMB1") {
        return false;
    }
    uint64_t byteCount = 0;
    file >> outPixels.width >> outPixels.height >> outPixels.rowPitch >> byteCount;
    file.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    if (outPixels.width == 0 ||
        outPixels.height == 0 ||
        outPixels.rowPitch < outPixels.width * 4u ||
        byteCount == 0 ||
        byteCount > 64ull * 1024ull * 1024ull) {
        return false;
    }
    outPixels.rgba8.resize(static_cast<size_t>(byteCount));
    file.read(
        reinterpret_cast<char*>(outPixels.rgba8.data()),
        static_cast<std::streamsize>(outPixels.rgba8.size()));
    return file.good() && IsValidPixelPayload(outPixels);
}

bool EditorAssetThumbnailCacheStore::StoreToDisk(
    const std::string& stableKey,
    const EditorAssetThumbnailPixelData& pixels) const {
    if (policy_.persistentRoot.empty()) {
        return false;
    }
    std::error_code error;
    std::filesystem::create_directories(policy_.persistentRoot, error);
    if (error) {
        return false;
    }

    std::ofstream file(CacheFilePath(stableKey), std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file << "CG5THUMB1\n";
    file << pixels.width << ' ' << pixels.height << ' ' << pixels.rowPitch << ' ' <<
        pixels.rgba8.size() << '\n';
    file.write(
        reinterpret_cast<const char*>(pixels.rgba8.data()),
        static_cast<std::streamsize>(pixels.rgba8.size()));
    return file.good();
}

void EditorAssetThumbnailCacheStore::PutMemory(
    std::string stableKey,
    const EditorAssetThumbnailPixelData& pixels,
    bool fallbackIcon) {
    const auto existing = entries_.find(stableKey);
    if (existing != entries_.end()) {
        telemetry_.residentBytes -= existing->second.bytes;
    }

    Entry entry{};
    entry.pixels = pixels;
    entry.bytes = PayloadBytes(pixels);
    entry.lastUse = nextUse_++;
    entry.fallbackIcon = fallbackIcon;
    telemetry_.residentBytes += entry.bytes;
    entries_[std::move(stableKey)] = std::move(entry);
    telemetry_.residentEntries = static_cast<uint32_t>(entries_.size());
    EvictIfNeeded();
}

void EditorAssetThumbnailCacheStore::EvictIfNeeded() {
    while (telemetry_.residentBytes > policy_.maxMemoryBytes && !entries_.empty()) {
        const auto oldest = std::min_element(
            entries_.begin(),
            entries_.end(),
            [](const auto& left, const auto& right) {
                return left.second.lastUse < right.second.lastUse;
            });
        if (oldest == entries_.end()) {
            break;
        }
        telemetry_.residentBytes -= oldest->second.bytes;
        entries_.erase(oldest);
        ++telemetry_.evictions;
    }
    telemetry_.residentEntries = static_cast<uint32_t>(entries_.size());
}

EditorAssetThumbnailCacheKey BuildEditorAssetThumbnailCacheKey(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    uint32_t previewVersion) {
    EditorAssetThumbnailCacheKey key{};
    key.thumbnailKey = request.key;
    key.kind = request.kind;
    key.previewKind = request.previewKind;
    key.assetId = request.assetId;
    key.sourcePath = request.sourcePath;
    key.sourceTimestamp = request.sourceTimestamp;
    key.previewGeneration = request.previewGeneration;
    key.previewVersion = previewVersion;
    key.width = request.width;
    key.height = request.height;
    key.vertexCount = request.vertexCount;
    key.faceCount = request.faceCount;
    key.materialSlotCount = request.materialSlotCount;
    key.materialTextureCount = request.materialTextureCount;
    key.materialTextureTimestamp = request.materialTextureTimestamp;
    key.boundsRadiusMilli = static_cast<uint32_t>((std::max)(0.0f, request.boundsRadius) * 1000.0f);
    key.byteSize = request.byteSize;
    return key;
}

} // namespace editor
