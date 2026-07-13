#pragma once

#include "EditorAssetGpuThumbnailRenderer.h"
#include "EditorAssetPreviewJobQueue.h"
#include "EditorAssetRegistry.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorAssetThumbnailStatus {
    Missing,
    Unsupported,
    Pending,
    Ready,
    Failed,
};

struct EditorAssetThumbnailEntry {
    std::string key;
    EditorAssetKind kind = EditorAssetKind::Unknown;
    std::string assetId;
    std::string sourcePath;
    std::string label;
    std::string detail;
    EditorAssetPreviewKind previewKind = EditorAssetPreviewKind::Icon;
    std::string previewFormat;
    uint64_t byteSize = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t vertexCount = 0;
    uint32_t faceCount = 0;
    uint32_t materialSlotCount = 0;
    uint32_t materialTextureCount = 0;
    uint32_t lineCount = 0;
    uint64_t materialTextureTimestamp = 0;
    float boundsRadius = 0.0f;
    float previewCameraDistance = 0.0f;
    float previewLightDirection[3] = {0.35f, -0.85f, 0.38f};
    uint64_t sourceTimestamp = 0;
    uint32_t generation = 0;
    EditorAssetThumbnailStatus status = EditorAssetThumbnailStatus::Pending;
    EditorAssetPreviewJobStatus jobStatus = EditorAssetPreviewJobStatus::Queued;
    uint32_t jobAttempts = 0;
    EditorAssetGpuThumbnailStatus gpuStatus = EditorAssetGpuThumbnailStatus::NotRequested;
    uint32_t gpuRenderRevision = 0;
    uint32_t gpuWidth = 0;
    uint32_t gpuHeight = 0;
    uint32_t gpuSwatchRgba = 0;
    uint64_t gpuHandleToken = 0;
    uint64_t gpuDisplayTextureId = 0;
    uint32_t gpuDescriptorIndex = UINT32_MAX;
    bool gpuShaderResourceView = false;
    bool hasPreviewGeometry = false;
    bool hasMaterialBinding = false;
    bool fallbackIcon = false;
};

class EditorAssetThumbnailService {
public:
    void Clear();
    void SetGpuThumbnailBackend(EditorAssetGpuThumbnailBackend* backend);
    void Sync(const EditorAssetRegistry& registry);
    uint32_t ProcessPreviewJobs(uint32_t maxJobsPerFrame);
    uint32_t ProcessPreviewJobs(
        std::chrono::milliseconds maxTimePerFrame,
        uint32_t maxLaunchesPerFrame);
    uint32_t ProcessGpuThumbnails(uint32_t maxJobsPerFrame);
    bool RetryPreview(std::string_view key);
    bool RetryGpuThumbnail(std::string_view key);

    EditorAssetThumbnailEntry Resolve(const EditorAssetRecord& record) const;
    const EditorAssetThumbnailEntry* Find(std::string_view key) const;
    const EditorAssetPreviewJobQueue& PreviewJobs() const { return previewJobs_; }
    const EditorAssetGpuThumbnailRenderer& GpuThumbnails() const { return gpuThumbnails_; }

    uint32_t Revision() const { return revision_; }
    uint32_t RegistryRevision() const { return registryRevision_; }
    std::size_t Count() const { return entries_.size(); }
    std::size_t Count(EditorAssetThumbnailStatus status) const;
    const std::vector<EditorAssetThumbnailEntry>& Entries() const { return entries_; }

private:
    void Touch();
    void RefreshGpuRequestsFromEntries();
    bool UpdateEntryGpuState(EditorAssetThumbnailEntry& entry);

    std::vector<EditorAssetThumbnailEntry> entries_;
    EditorAssetPreviewJobQueue previewJobs_;
    EditorAssetGpuThumbnailRenderer gpuThumbnails_;
    uint32_t revision_ = 0;
    uint32_t registryRevision_ = 0;
};

const char* ToString(EditorAssetThumbnailStatus status);
std::string BuildEditorAssetThumbnailKey(const EditorAssetRecord& record);
bool EditorAssetKindSupportsThumbnailPreview(EditorAssetKind kind);

} // namespace editor
