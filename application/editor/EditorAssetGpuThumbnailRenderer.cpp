#include "EditorAssetGpuThumbnailRenderer.h"

#include <algorithm>
#include <string>

namespace editor {
namespace {

uint64_t HashCombine(uint64_t hash, uint64_t value) {
    constexpr uint64_t kPrime = 1099511628211ull;
    for (int shift = 0; shift < 64; shift += 8) {
        hash ^= (value >> shift) & 0xffull;
        hash *= kPrime;
    }
    return hash;
}

uint64_t HashString(uint64_t hash, std::string_view value) {
    constexpr uint64_t kPrime = 1099511628211ull;
    for (const char ch : value) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= kPrime;
    }
    return hash;
}

uint64_t BuildGpuThumbnailToken(const EditorAssetGpuThumbnailEntry& entry) {
    uint64_t hash = 1469598103934665603ull;
    hash = HashString(hash, entry.key);
    hash = HashString(hash, entry.assetId);
    hash = HashCombine(hash, static_cast<uint64_t>(entry.kind));
    hash = HashCombine(hash, static_cast<uint64_t>(entry.previewKind));
    hash = HashCombine(hash, entry.sourceTimestamp);
    hash = HashCombine(hash, entry.previewGeneration);
    hash = HashCombine(hash, entry.renderRevision);
    hash = HashCombine(hash, entry.vertexCount);
    hash = HashCombine(hash, entry.faceCount);
    hash = HashCombine(hash, entry.materialSlotCount);
    hash = HashCombine(hash, entry.materialTextureCount);
    hash = HashCombine(hash, entry.materialTextureTimestamp);
    hash = HashCombine(hash, static_cast<uint64_t>(entry.boundsRadius * 1000.0f));
    hash = HashCombine(hash, entry.hasPreviewGeometry ? 1ull : 0ull);
    hash = HashCombine(hash, entry.hasMaterialBinding ? 1ull : 0ull);
    return hash == 0 ? 1 : hash;
}

uint32_t BuildSwatchRgba(uint64_t token, EditorAssetPreviewKind kind) {
    const uint32_t red = 72u + static_cast<uint32_t>((token >> 8) & 0x7fu);
    const uint32_t green = 72u + static_cast<uint32_t>((token >> 24) & 0x7fu);
    const uint32_t blue = 72u + static_cast<uint32_t>((token >> 40) & 0x7fu);
    const uint32_t kindBias = static_cast<uint32_t>(kind) * 17u;
    return 0xff000000u |
        (((red + kindBias) & 0xffu) << 0) |
        (((green + kindBias / 2u) & 0xffu) << 8) |
        (((blue + kindBias / 3u) & 0xffu) << 16);
}

EditorAssetGpuThumbnailEntry MakeEntry(const EditorAssetGpuThumbnailRequest& request) {
    EditorAssetGpuThumbnailEntry entry{};
    entry.key = request.key;
    entry.kind = request.kind;
    entry.previewKind = request.previewKind;
    entry.assetId = request.assetId;
    entry.sourcePath = request.sourcePath;
    entry.label = request.label;
    entry.previewFormat = request.previewFormat;
    entry.sourceTimestamp = request.sourceTimestamp;
    entry.previewGeneration = request.previewGeneration;
    entry.width = request.sourceWidth > 0 ? (std::min)(request.sourceWidth, 256u) : 96u;
    entry.height = request.sourceHeight > 0 ? (std::min)(request.sourceHeight, 256u) : 96u;
    entry.vertexCount = request.vertexCount;
    entry.faceCount = request.faceCount;
    entry.materialSlotCount = request.materialSlotCount;
    entry.materialTextureCount = request.materialTextureCount;
    entry.materialTextureTimestamp = request.materialTextureTimestamp;
    entry.boundsRadius = request.boundsRadius;
    entry.previewCameraDistance = request.previewCameraDistance;
    entry.previewLightDirection[0] = request.previewLightDirection[0];
    entry.previewLightDirection[1] = request.previewLightDirection[1];
    entry.previewLightDirection[2] = request.previewLightDirection[2];
    entry.byteSize = request.byteSize;
    entry.hasPreviewGeometry = request.hasPreviewGeometry;
    entry.hasMaterialBinding = request.hasMaterialBinding;
    entry.fallbackIcon = request.fallbackIcon;
    entry.detail = "GPU thumbnail render request is queued.";
    return entry;
}

bool RequestDiffers(
    const EditorAssetGpuThumbnailEntry& entry,
    const EditorAssetGpuThumbnailRequest& request) {
    return entry.kind != request.kind ||
        entry.previewKind != request.previewKind ||
        entry.assetId != request.assetId ||
        entry.sourcePath != request.sourcePath ||
        entry.label != request.label ||
        entry.previewFormat != request.previewFormat ||
        entry.sourceTimestamp != request.sourceTimestamp ||
        entry.previewGeneration != request.previewGeneration ||
        entry.vertexCount != request.vertexCount ||
        entry.faceCount != request.faceCount ||
        entry.materialSlotCount != request.materialSlotCount ||
        entry.materialTextureCount != request.materialTextureCount ||
        entry.materialTextureTimestamp != request.materialTextureTimestamp ||
        entry.boundsRadius != request.boundsRadius ||
        entry.previewCameraDistance != request.previewCameraDistance ||
        entry.previewLightDirection[0] != request.previewLightDirection[0] ||
        entry.previewLightDirection[1] != request.previewLightDirection[1] ||
        entry.previewLightDirection[2] != request.previewLightDirection[2] ||
        entry.byteSize != request.byteSize ||
        entry.hasPreviewGeometry != request.hasPreviewGeometry ||
        entry.hasMaterialBinding != request.hasMaterialBinding ||
        entry.fallbackIcon != request.fallbackIcon;
}

EditorAssetGpuThumbnailAllocationRequest BuildAllocationRequest(
    const EditorAssetGpuThumbnailEntry& entry) {
    EditorAssetGpuThumbnailAllocationRequest request{};
    request.key = entry.key;
    request.kind = entry.kind;
    request.previewKind = entry.previewKind;
    request.assetId = entry.assetId;
    request.sourcePath = entry.sourcePath;
    request.width = entry.width;
    request.height = entry.height;
    request.vertexCount = entry.vertexCount;
    request.faceCount = entry.faceCount;
    request.materialSlotCount = entry.materialSlotCount;
    request.materialTextureCount = entry.materialTextureCount;
    request.materialTextureTimestamp = entry.materialTextureTimestamp;
    request.boundsRadius = entry.boundsRadius;
    request.previewCameraDistance = entry.previewCameraDistance;
    request.previewLightDirection[0] = entry.previewLightDirection[0];
    request.previewLightDirection[1] = entry.previewLightDirection[1];
    request.previewLightDirection[2] = entry.previewLightDirection[2];
    request.byteSize = entry.byteSize;
    request.swatchRgba = entry.swatchRgba;
    request.hasPreviewGeometry = entry.hasPreviewGeometry;
    request.hasMaterialBinding = entry.hasMaterialBinding;
    request.sourceTimestamp = entry.sourceTimestamp;
    request.previewGeneration = entry.previewGeneration;
    request.renderRevision = entry.renderRevision;
    return request;
}

} // namespace

void EditorAssetGpuThumbnailRenderer::SetBackend(EditorAssetGpuThumbnailBackend* backend) {
    if (backend_ == backend) {
        return;
    }
    for (EditorAssetGpuThumbnailEntry& entry : entries_) {
        ReleaseEntry(entry);
        if (entry.status == EditorAssetGpuThumbnailStatus::Ready) {
            entry.status = EditorAssetGpuThumbnailStatus::Queued;
            entry.detail = "GPU thumbnail backend changed; render request is queued.";
        }
    }
    backend_ = backend;
    Touch();
}

void EditorAssetGpuThumbnailRenderer::Clear() {
    if (entries_.empty()) {
        return;
    }
    for (EditorAssetGpuThumbnailEntry& entry : entries_) {
        ReleaseEntry(entry);
    }
    entries_.clear();
    nextRenderRevision_ = 1;
    Touch();
}

bool EditorAssetGpuThumbnailRenderer::Enqueue(const EditorAssetGpuThumbnailRequest& request) {
    if (request.key.empty()) {
        return false;
    }

    EditorAssetGpuThumbnailEntry* existing = FindMutable(request.key);
    if (existing != nullptr) {
        if (!RequestDiffers(*existing, request)) {
            return false;
        }
        ReleaseEntry(*existing);
        if (existing->status == EditorAssetGpuThumbnailStatus::Queued ||
            existing->status == EditorAssetGpuThumbnailStatus::Rendering) {
            *existing = MakeEntry(request);
            Touch();
            return true;
        }
        existing->status = EditorAssetGpuThumbnailStatus::Stale;
    }

    entries_.push_back(MakeEntry(request));
    Touch();
    return true;
}

bool EditorAssetGpuThumbnailRenderer::Retry(std::string_view key) {
    EditorAssetGpuThumbnailEntry* entry = FindMutable(key);
    if (entry == nullptr || entry->status != EditorAssetGpuThumbnailStatus::Failed) {
        return false;
    }
    entry->status = EditorAssetGpuThumbnailStatus::Queued;
    entry->detail = "GPU thumbnail render request is queued.";
    ReleaseEntry(*entry);
    entry->swatchRgba = 0;
    Touch();
    return true;
}

uint32_t EditorAssetGpuThumbnailRenderer::ProcessBudgeted(uint32_t maxJobs) {
    uint32_t processed = 0;
    for (EditorAssetGpuThumbnailEntry& entry : entries_) {
        if (processed >= maxJobs) {
            break;
        }
        if (entry.status != EditorAssetGpuThumbnailStatus::Queued) {
            continue;
        }

        entry.status = EditorAssetGpuThumbnailStatus::Rendering;
        entry.detail = "GPU thumbnail render target is being prepared.";
        ++entry.attempts;
        Touch();

        if (!EditorAssetPreviewKindSupportsGpuThumbnail(entry.previewKind)) {
            entry.status = EditorAssetGpuThumbnailStatus::Failed;
            entry.detail = "Preview kind cannot be represented by a GPU thumbnail.";
            ++processed;
            Touch();
            continue;
        }

        entry.renderRevision = nextRenderRevision_++;
        const uint64_t fallbackToken = BuildGpuThumbnailToken(entry);
        entry.gpuHandleToken = fallbackToken;
        entry.swatchRgba = BuildSwatchRgba(entry.gpuHandleToken, entry.previewKind);

        if (backend_ != nullptr) {
            EditorAssetGpuThumbnailAllocation allocation{};
            std::string error;
            if (!backend_->AllocateThumbnail(BuildAllocationRequest(entry), allocation, error)) {
                entry.status = EditorAssetGpuThumbnailStatus::Failed;
                entry.detail = error.empty() ? "GPU thumbnail SRV allocation failed." : error;
                entry.gpuHandleToken = 0;
                entry.displayTextureId = 0;
                entry.descriptorIndex = UINT32_MAX;
                entry.shaderResourceView = false;
                ++processed;
                Touch();
                continue;
            }
            entry.gpuHandleToken = allocation.resourceId != 0 ? allocation.resourceId : fallbackToken;
            entry.displayTextureId = allocation.displayTextureId;
            entry.descriptorIndex = allocation.descriptorIndex;
            if (allocation.width > 0 && allocation.height > 0) {
                entry.width = allocation.width;
                entry.height = allocation.height;
            }
            entry.shaderResourceView = allocation.shaderResourceView;
            if (!allocation.detail.empty()) {
                entry.detail = allocation.detail;
            }
        } else {
            entry.displayTextureId = fallbackToken;
            entry.descriptorIndex = UINT32_MAX;
            entry.shaderResourceView = false;
        }
        entry.status = EditorAssetGpuThumbnailStatus::Ready;
        if (entry.detail.empty() || entry.detail == "GPU thumbnail render target is being prepared.") {
            entry.detail = "GPU thumbnail render target is ready.";
        }
        ++processed;
        Touch();
    }
    return processed;
}

const EditorAssetGpuThumbnailEntry* EditorAssetGpuThumbnailRenderer::Find(std::string_view key) const {
    const auto it = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&](const EditorAssetGpuThumbnailEntry& entry) {
            return entry.key == key && entry.status != EditorAssetGpuThumbnailStatus::Stale;
        });
    return it != entries_.end() ? &*it : nullptr;
}

EditorAssetGpuThumbnailEntry* EditorAssetGpuThumbnailRenderer::FindMutable(std::string_view key) {
    const auto it = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&](const EditorAssetGpuThumbnailEntry& entry) {
            return entry.key == key && entry.status != EditorAssetGpuThumbnailStatus::Stale;
        });
    return it != entries_.end() ? &*it : nullptr;
}

std::size_t EditorAssetGpuThumbnailRenderer::Count(EditorAssetGpuThumbnailStatus status) const {
    return static_cast<std::size_t>(std::count_if(
        entries_.begin(),
        entries_.end(),
        [&](const EditorAssetGpuThumbnailEntry& entry) {
            return entry.status == status;
        }));
}

EditorAssetGpuThumbnailBackendTelemetry EditorAssetGpuThumbnailRenderer::BackendTelemetry() const {
    return backend_ != nullptr ? backend_->Telemetry() : EditorAssetGpuThumbnailBackendTelemetry{};
}

void EditorAssetGpuThumbnailRenderer::Touch() {
    ++revision_;
}

void EditorAssetGpuThumbnailRenderer::ReleaseEntry(EditorAssetGpuThumbnailEntry& entry) {
    if (backend_ != nullptr && entry.gpuHandleToken != 0) {
        backend_->ReleaseThumbnail(entry.key, entry.gpuHandleToken);
    }
    entry.gpuHandleToken = 0;
    entry.displayTextureId = 0;
    entry.descriptorIndex = UINT32_MAX;
    entry.shaderResourceView = false;
}

const char* ToString(EditorAssetGpuThumbnailStatus status) {
    switch (status) {
    case EditorAssetGpuThumbnailStatus::NotRequested:
        return "NotRequested";
    case EditorAssetGpuThumbnailStatus::Queued:
        return "Queued";
    case EditorAssetGpuThumbnailStatus::Rendering:
        return "Rendering";
    case EditorAssetGpuThumbnailStatus::Ready:
        return "Ready";
    case EditorAssetGpuThumbnailStatus::Failed:
        return "Failed";
    case EditorAssetGpuThumbnailStatus::Stale:
        return "Stale";
    case EditorAssetGpuThumbnailStatus::Cancelled:
        return "Cancelled";
    }
    return "Unknown";
}

bool EditorAssetPreviewKindSupportsGpuThumbnail(EditorAssetPreviewKind kind) {
    return kind == EditorAssetPreviewKind::Texture ||
        kind == EditorAssetPreviewKind::Mesh ||
        kind == EditorAssetPreviewKind::Text ||
        kind == EditorAssetPreviewKind::Audio ||
        kind == EditorAssetPreviewKind::Icon;
}

} // namespace editor
