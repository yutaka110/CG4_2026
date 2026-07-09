#pragma once

#include "EditorAssetGpuThumbnailRenderer.h"
#include "EditorAssetThumbnailTextureLoader.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace editor {

struct EditorAssetThumbnailCachePolicy {
    bool persistentCacheEnabled = false;
    bool storeFallbackIcons = true;
    uint32_t previewVersion = 1;
    uint64_t maxMemoryBytes = 16ull * 1024ull * 1024ull;
    uint32_t maxRetainedUploadResources = 256;
    std::filesystem::path persistentRoot;
};

struct EditorAssetThumbnailCacheKey {
    std::string thumbnailKey;
    EditorAssetKind kind = EditorAssetKind::Unknown;
    EditorAssetPreviewKind previewKind = EditorAssetPreviewKind::Icon;
    std::string assetId;
    std::string sourcePath;
    uint64_t sourceTimestamp = 0;
    uint32_t previewGeneration = 0;
    uint32_t previewVersion = 1;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;
    uint32_t materialSlotCount = 0;
    uint32_t materialTextureCount = 0;
    uint32_t boundsRadiusMilli = 0;
    uint64_t materialTextureTimestamp = 0;
    uint64_t byteSize = 0;
};

struct EditorAssetThumbnailCacheTelemetry {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t stores = 0;
    uint64_t evictions = 0;
    uint64_t residentBytes = 0;
    uint32_t residentEntries = 0;
};

class EditorAssetThumbnailCacheStore {
public:
    void Configure(EditorAssetThumbnailCachePolicy policy);
    void ClearMemory();
    void ClearAll();

    const EditorAssetThumbnailCachePolicy& Policy() const { return policy_; }
    const EditorAssetThumbnailCacheTelemetry& Telemetry() const { return telemetry_; }

    bool TryLoad(
        const EditorAssetThumbnailCacheKey& key,
        EditorAssetThumbnailPixelData& outPixels,
        std::string& outDetail);
    bool Store(
        const EditorAssetThumbnailCacheKey& key,
        const EditorAssetThumbnailPixelData& pixels,
        bool fallbackIcon,
        std::string& outDetail);

private:
    struct Entry {
        EditorAssetThumbnailPixelData pixels;
        uint64_t bytes = 0;
        uint64_t lastUse = 0;
        bool fallbackIcon = false;
    };

    std::string StableKey(const EditorAssetThumbnailCacheKey& key) const;
    std::filesystem::path CacheFilePath(const std::string& stableKey) const;
    bool LoadFromDisk(const std::string& stableKey, EditorAssetThumbnailPixelData& outPixels) const;
    bool StoreToDisk(const std::string& stableKey, const EditorAssetThumbnailPixelData& pixels) const;
    void PutMemory(std::string stableKey, const EditorAssetThumbnailPixelData& pixels, bool fallbackIcon);
    void EvictIfNeeded();

    EditorAssetThumbnailCachePolicy policy_{};
    EditorAssetThumbnailCacheTelemetry telemetry_{};
    std::unordered_map<std::string, Entry> entries_;
    uint64_t nextUse_ = 1;
};

EditorAssetThumbnailCacheKey BuildEditorAssetThumbnailCacheKey(
    const EditorAssetGpuThumbnailAllocationRequest& request,
    uint32_t previewVersion);

} // namespace editor
