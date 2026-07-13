#pragma once

#include "EditorAssetPreviewProvider.h"
#include "EditorAssetRegistry.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorAssetGpuThumbnailStatus {
    NotRequested,
    Queued,
    Rendering,
    Ready,
    Failed,
    Stale,
    Cancelled,
};

struct EditorAssetGpuThumbnailRequest {
    std::string key;
    EditorAssetKind kind = EditorAssetKind::Unknown;
    EditorAssetPreviewKind previewKind = EditorAssetPreviewKind::Icon;
    std::string assetId;
    std::string sourcePath;
    std::string label;
    std::string previewFormat;
    uint64_t sourceTimestamp = 0;
    uint32_t previewGeneration = 0;
    uint32_t sourceWidth = 0;
    uint32_t sourceHeight = 0;
    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;
    uint32_t materialSlotCount = 0;
    uint32_t materialTextureCount = 0;
    float boundsRadius = 0.0f;
    float previewCameraDistance = 0.0f;
    float previewLightDirection[3] = {0.35f, -0.85f, 0.38f};
    uint64_t materialTextureTimestamp = 0;
    uint64_t byteSize = 0;
    bool hasPreviewGeometry = false;
    bool hasMaterialBinding = false;
    bool fallbackIcon = false;
};

struct EditorAssetGpuThumbnailAllocationRequest {
    std::string key;
    EditorAssetKind kind = EditorAssetKind::Unknown;
    EditorAssetPreviewKind previewKind = EditorAssetPreviewKind::Icon;
    std::string assetId;
    std::string sourcePath;
    uint32_t width = 96;
    uint32_t height = 96;
    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;
    uint32_t materialSlotCount = 0;
    uint32_t materialTextureCount = 0;
    float boundsRadius = 0.0f;
    float previewCameraDistance = 0.0f;
    float previewLightDirection[3] = {0.35f, -0.85f, 0.38f};
    uint64_t materialTextureTimestamp = 0;
    uint64_t byteSize = 0;
    uint32_t swatchRgba = 0;
    bool hasPreviewGeometry = false;
    bool hasMaterialBinding = false;
    uint64_t sourceTimestamp = 0;
    uint32_t previewGeneration = 0;
    uint32_t renderRevision = 0;
};

struct EditorAssetGpuThumbnailAllocation {
    uint64_t resourceId = 0;
    uint64_t displayTextureId = 0;
    uint32_t descriptorIndex = UINT32_MAX;
    uint32_t width = 0;
    uint32_t height = 0;
    bool shaderResourceView = false;
    std::string detail;
};

struct EditorAssetGpuThumbnailBackendTelemetry {
    uint64_t allocationRequests = 0;
    uint64_t allocated = 0;
    uint64_t released = 0;
    uint32_t residentCount = 0;
    uint32_t descriptorCapacity = 0;
    uint64_t uploadBytes = 0;
    uint32_t retainedUploadResources = 0;
    uint64_t cacheHits = 0;
    uint64_t cacheMisses = 0;
    uint64_t cacheStores = 0;
    uint64_t cacheEvictions = 0;
    uint64_t fallbackUploads = 0;
    uint64_t pendingUploadBytes = 0;
    uint64_t retiredUploadBytes = 0;
    uint32_t pendingUploadCount = 0;
    uint64_t previewSceneRendered = 0;
    uint64_t previewSceneFallback = 0;
    uint64_t previewSceneProxyGeometry = 0;
    uint64_t previewSceneRendererDraws = 0;
    uint64_t previewSceneProceduralFallback = 0;
    uint64_t previewSceneProductionMeshDraws = 0;
    uint64_t previewSceneMaterialBound = 0;
    uint64_t previewSceneMaterialTextureBound = 0;
    uint64_t previewSceneMaterialTextureFallback = 0;
    uint64_t previewSceneMaterialTextureSrvBound = 0;
    uint64_t previewSceneMaterialTextureSrvFallback = 0;
    uint32_t previewSceneMaterialTextureSrvDescriptors = 0;
    uint64_t previewSceneMaterialTextureTables = 0;
    uint64_t previewSceneMaterialPbrPreviews = 0;
    uint64_t previewSceneMaterialNormalMapBound = 0;
    uint64_t previewSceneMaterialRoughnessMapBound = 0;
    uint64_t previewSceneMaterialMetallicMapBound = 0;
    uint64_t previewSceneProductionMaterialCacheHits = 0;
    uint64_t previewSceneProductionMaterialCacheMisses = 0;
    uint64_t previewRenderTargetReused = 0;
    uint64_t previewRenderTargetResized = 0;
};

class EditorAssetGpuThumbnailBackend {
public:
    virtual ~EditorAssetGpuThumbnailBackend() = default;
    virtual bool AllocateThumbnail(
        const EditorAssetGpuThumbnailAllocationRequest& request,
        EditorAssetGpuThumbnailAllocation& outAllocation,
        std::string& outError) = 0;
    virtual void ReleaseThumbnail(std::string_view key, uint64_t resourceId) = 0;
    virtual EditorAssetGpuThumbnailBackendTelemetry Telemetry() const { return {}; }
};

struct EditorAssetGpuThumbnailEntry {
    std::string key;
    EditorAssetKind kind = EditorAssetKind::Unknown;
    EditorAssetPreviewKind previewKind = EditorAssetPreviewKind::Icon;
    std::string assetId;
    std::string sourcePath;
    std::string label;
    std::string previewFormat;
    uint64_t sourceTimestamp = 0;
    uint32_t previewGeneration = 0;
    uint32_t renderRevision = 0;
    uint32_t attempts = 0;
    uint32_t width = 96;
    uint32_t height = 96;
    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;
    uint32_t materialSlotCount = 0;
    uint32_t materialTextureCount = 0;
    float boundsRadius = 0.0f;
    float previewCameraDistance = 0.0f;
    float previewLightDirection[3] = {0.35f, -0.85f, 0.38f};
    uint64_t materialTextureTimestamp = 0;
    uint64_t byteSize = 0;
    uint32_t swatchRgba = 0;
    uint64_t gpuHandleToken = 0;
    uint64_t displayTextureId = 0;
    uint32_t descriptorIndex = UINT32_MAX;
    bool shaderResourceView = false;
    bool hasPreviewGeometry = false;
    bool hasMaterialBinding = false;
    EditorAssetGpuThumbnailStatus status = EditorAssetGpuThumbnailStatus::Queued;
    std::string detail;
    bool fallbackIcon = false;
};

class EditorAssetGpuThumbnailRenderer {
public:
    void SetBackend(EditorAssetGpuThumbnailBackend* backend);
    void Clear();
    bool Enqueue(const EditorAssetGpuThumbnailRequest& request);
    bool Retry(std::string_view key);
    uint32_t ProcessBudgeted(uint32_t maxJobs);

    const EditorAssetGpuThumbnailEntry* Find(std::string_view key) const;
    EditorAssetGpuThumbnailEntry* FindMutable(std::string_view key);

    std::size_t Count() const { return entries_.size(); }
    std::size_t Count(EditorAssetGpuThumbnailStatus status) const;
    uint32_t Revision() const { return revision_; }
    const std::vector<EditorAssetGpuThumbnailEntry>& Entries() const { return entries_; }
    EditorAssetGpuThumbnailBackendTelemetry BackendTelemetry() const;

private:
    void Touch();
    void ReleaseEntry(EditorAssetGpuThumbnailEntry& entry);

    std::vector<EditorAssetGpuThumbnailEntry> entries_;
    EditorAssetGpuThumbnailBackend* backend_ = nullptr;
    uint32_t revision_ = 0;
    uint32_t nextRenderRevision_ = 1;
};

const char* ToString(EditorAssetGpuThumbnailStatus status);
bool EditorAssetPreviewKindSupportsGpuThumbnail(EditorAssetPreviewKind kind);

} // namespace editor
