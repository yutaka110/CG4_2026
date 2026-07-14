#include "EditorAssetThumbnailService.h"

#include <algorithm>
#include <utility>

namespace editor {
namespace {

EditorAssetThumbnailStatus ToThumbnailStatus(EditorAssetPreviewReadiness readiness) {
    switch (readiness) {
    case EditorAssetPreviewReadiness::Missing:
        return EditorAssetThumbnailStatus::Missing;
    case EditorAssetPreviewReadiness::Unsupported:
        return EditorAssetThumbnailStatus::Unsupported;
    case EditorAssetPreviewReadiness::Ready:
        return EditorAssetThumbnailStatus::Ready;
    case EditorAssetPreviewReadiness::Failed:
        return EditorAssetThumbnailStatus::Failed;
    }
    return EditorAssetThumbnailStatus::Unsupported;
}

bool PreviewContentDiffers(
    const EditorAssetThumbnailEntry& previous,
    const EditorAssetThumbnailEntry& current) {
    return previous.sourceTimestamp != current.sourceTimestamp ||
        previous.status != current.status ||
        previous.sourcePath != current.sourcePath ||
        previous.detail != current.detail ||
        previous.label != current.label ||
        previous.previewKind != current.previewKind ||
        previous.previewFormat != current.previewFormat ||
        previous.byteSize != current.byteSize ||
        previous.width != current.width ||
        previous.height != current.height ||
        previous.vertexCount != current.vertexCount ||
        previous.faceCount != current.faceCount ||
        previous.materialSlotCount != current.materialSlotCount ||
        previous.materialTextureCount != current.materialTextureCount ||
        previous.materialTextureTimestamp != current.materialTextureTimestamp ||
        previous.lineCount != current.lineCount ||
        previous.boundsRadius != current.boundsRadius ||
        previous.previewCameraDistance != current.previewCameraDistance ||
        previous.previewLightDirection[0] != current.previewLightDirection[0] ||
        previous.previewLightDirection[1] != current.previewLightDirection[1] ||
        previous.previewLightDirection[2] != current.previewLightDirection[2] ||
        previous.jobStatus != current.jobStatus ||
        previous.jobAttempts != current.jobAttempts ||
        previous.gpuStatus != current.gpuStatus ||
        previous.gpuRenderRevision != current.gpuRenderRevision ||
        previous.gpuWidth != current.gpuWidth ||
        previous.gpuHeight != current.gpuHeight ||
        previous.gpuSwatchRgba != current.gpuSwatchRgba ||
        previous.gpuHandleToken != current.gpuHandleToken ||
        previous.gpuDisplayTextureId != current.gpuDisplayTextureId ||
        previous.gpuDescriptorIndex != current.gpuDescriptorIndex ||
        previous.gpuShaderResourceView != current.gpuShaderResourceView ||
        previous.hasPreviewGeometry != current.hasPreviewGeometry ||
        previous.hasMaterialBinding != current.hasMaterialBinding ||
        previous.fallbackIcon != current.fallbackIcon;
}

bool EntryDiffers(
    const EditorAssetThumbnailEntry& previous,
    const EditorAssetThumbnailEntry& current) {
    return previous.kind != current.kind ||
        previous.assetId != current.assetId ||
        previous.generation != current.generation ||
        PreviewContentDiffers(previous, current);
}

EditorAssetGpuThumbnailRequest BuildGpuThumbnailRequest(const EditorAssetThumbnailEntry& entry) {
    EditorAssetGpuThumbnailRequest request{};
    request.key = entry.key;
    request.kind = entry.kind;
    request.previewKind = entry.previewKind;
    request.assetId = entry.assetId;
    request.sourcePath = entry.sourcePath;
    request.label = entry.label;
    request.previewFormat = entry.previewFormat;
    request.sourceTimestamp = entry.sourceTimestamp;
    request.previewGeneration = entry.jobAttempts;
    request.sourceWidth = entry.width;
    request.sourceHeight = entry.height;
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
    request.hasPreviewGeometry = entry.hasPreviewGeometry;
    request.hasMaterialBinding = entry.hasMaterialBinding;
    request.fallbackIcon = entry.fallbackIcon;
    return request;
}

void ApplyGpuState(
    EditorAssetThumbnailEntry& entry,
    const EditorAssetGpuThumbnailEntry* gpuEntry) {
    if (entry.status != EditorAssetThumbnailStatus::Ready) {
        entry.gpuStatus = entry.status == EditorAssetThumbnailStatus::Pending
            ? EditorAssetGpuThumbnailStatus::NotRequested
            : EditorAssetGpuThumbnailStatus::Cancelled;
        entry.gpuRenderRevision = 0;
        entry.gpuWidth = 0;
        entry.gpuHeight = 0;
        entry.gpuSwatchRgba = 0;
        entry.gpuHandleToken = 0;
        entry.gpuDisplayTextureId = 0;
        entry.gpuDescriptorIndex = UINT32_MAX;
        entry.gpuShaderResourceView = false;
        return;
    }

    if (gpuEntry == nullptr) {
        entry.gpuStatus = EditorAssetGpuThumbnailStatus::Queued;
        entry.gpuRenderRevision = 0;
        entry.gpuWidth = 0;
        entry.gpuHeight = 0;
        entry.gpuSwatchRgba = 0;
        entry.gpuHandleToken = 0;
        entry.gpuDisplayTextureId = 0;
        entry.gpuDescriptorIndex = UINT32_MAX;
        entry.gpuShaderResourceView = false;
        return;
    }

    entry.gpuStatus = gpuEntry->status;
    entry.gpuRenderRevision = gpuEntry->renderRevision;
    entry.gpuWidth = gpuEntry->width;
    entry.gpuHeight = gpuEntry->height;
    entry.gpuSwatchRgba = gpuEntry->swatchRgba;
    entry.gpuHandleToken = gpuEntry->gpuHandleToken;
    entry.gpuDisplayTextureId = gpuEntry->displayTextureId;
    entry.gpuDescriptorIndex = gpuEntry->descriptorIndex;
    entry.gpuShaderResourceView = gpuEntry->shaderResourceView;
}

EditorAssetThumbnailEntry BuildEntry(
    const EditorAssetRecord& record,
    const EditorAssetThumbnailEntry* previous,
    const EditorAssetPreviewJob* job,
    const EditorAssetGpuThumbnailEntry* gpuEntry) {
    EditorAssetThumbnailEntry entry{};
    entry.key = BuildEditorAssetThumbnailKey(record);
    entry.kind = record.kind;
    entry.assetId = record.id;
    entry.sourcePath = record.sourcePath;
    entry.sourceTimestamp = record.sourceTimestamp;
    entry.generation = previous != nullptr ? previous->generation : 1;

    if (record.missing) {
        entry.label = ToString(record.kind);
        entry.detail = "Source file is missing; using missing-asset fallback.";
        entry.previewKind = EditorAssetPreviewKind::Icon;
        entry.status = EditorAssetThumbnailStatus::Missing;
        entry.jobStatus = EditorAssetPreviewJobStatus::Cancelled;
        entry.fallbackIcon = true;
    } else if (job != nullptr &&
        job->sourceTimestamp == record.sourceTimestamp &&
        (job->status == EditorAssetPreviewJobStatus::Ready ||
            job->status == EditorAssetPreviewJobStatus::Failed)) {
        const EditorAssetPreviewInfo& preview = job->preview;
        entry.label = preview.label.empty() ? ToString(record.kind) : preview.label;
        entry.detail = preview.detail;
        entry.previewKind = preview.kind;
        entry.previewFormat = preview.format;
        entry.byteSize = preview.byteSize;
        entry.width = preview.width;
        entry.height = preview.height;
        entry.vertexCount = preview.vertexCount;
        entry.faceCount = preview.faceCount;
        entry.materialSlotCount = preview.materialSlotCount;
        entry.materialTextureCount = preview.materialTextureCount;
        entry.materialTextureTimestamp = preview.materialTextureTimestamp;
        entry.lineCount = preview.lineCount;
        entry.boundsRadius = preview.boundsRadius;
        entry.previewCameraDistance = preview.previewCameraDistance;
        entry.previewLightDirection[0] = preview.previewLightDirection[0];
        entry.previewLightDirection[1] = preview.previewLightDirection[1];
        entry.previewLightDirection[2] = preview.previewLightDirection[2];
        entry.status = ToThumbnailStatus(preview.readiness);
        entry.jobStatus = job->status;
        entry.jobAttempts = job->attempts;
        entry.hasPreviewGeometry = preview.hasPreviewGeometry;
        entry.hasMaterialBinding = preview.hasMaterialBinding;
        entry.fallbackIcon = preview.fallbackIcon;
    } else if (EditorAssetKindSupportsThumbnailPreview(record.kind)) {
        entry.label = ToString(record.kind);
        entry.detail = job != nullptr && job->status == EditorAssetPreviewJobStatus::Running
            ? "Preview job is processing."
            : "Preview job is queued.";
        entry.previewKind = EditorAssetPreviewKind::Icon;
        entry.status = EditorAssetThumbnailStatus::Pending;
        entry.jobStatus = job != nullptr ? job->status : EditorAssetPreviewJobStatus::Queued;
        entry.jobAttempts = job != nullptr ? job->attempts : 0;
        entry.fallbackIcon = true;
    } else {
        entry.label = ToString(record.kind);
        entry.detail = std::string(ToString(record.kind)) + " uses the kind icon fallback.";
        entry.previewKind = EditorAssetPreviewKind::Icon;
        entry.status = EditorAssetThumbnailStatus::Unsupported;
        entry.jobStatus = EditorAssetPreviewJobStatus::Cancelled;
        entry.fallbackIcon = true;
    }

    if (previous != nullptr && PreviewContentDiffers(*previous, entry)) {
        entry.generation = previous->generation + 1;
    }
    ApplyGpuState(entry, gpuEntry);
    if (previous != nullptr && PreviewContentDiffers(*previous, entry)) {
        entry.generation = previous->generation + 1;
    }
    return entry;
}

} // namespace

void EditorAssetThumbnailService::Clear() {
    if (entries_.empty() &&
        registryRevision_ == 0 &&
        previewJobs_.Count() == 0 &&
        gpuThumbnails_.Count() == 0) {
        return;
    }
    entries_.clear();
    previewJobs_.Clear();
    gpuThumbnails_.Clear();
    registryRevision_ = 0;
    Touch();
}

void EditorAssetThumbnailService::SetGpuThumbnailBackend(EditorAssetGpuThumbnailBackend* backend) {
    gpuThumbnails_.SetBackend(backend);
    bool changed = false;
    for (EditorAssetThumbnailEntry& entry : entries_) {
        if (UpdateEntryGpuState(entry)) {
            changed = true;
        }
    }
    if (changed) {
        Touch();
    }
}

void EditorAssetThumbnailService::Sync(const EditorAssetRegistry& registry) {
    if (registryRevision_ == registry.Revision() &&
        entries_.size() == registry.Records().size()) {
        return;
    }

    std::vector<EditorAssetThumbnailEntry> updated;
    updated.reserve(registry.Records().size());

    bool changed = registryRevision_ != registry.Revision() || entries_.size() != registry.Records().size();
    for (const EditorAssetRecord& record : registry.Records()) {
        const std::string key = BuildEditorAssetThumbnailKey(record);
        if (!record.missing && EditorAssetKindSupportsThumbnailPreview(record.kind)) {
            previewJobs_.Enqueue(record, key);
        }
        const EditorAssetThumbnailEntry* previous = Find(key);
        const EditorAssetPreviewJob* job = previewJobs_.Find(key);
        const EditorAssetGpuThumbnailEntry* gpuEntry = gpuThumbnails_.Find(key);
        EditorAssetThumbnailEntry entry = BuildEntry(record, previous, job, gpuEntry);
        if (previous == nullptr || EntryDiffers(*previous, entry)) {
            changed = true;
        }
        updated.push_back(std::move(entry));
    }

    entries_ = std::move(updated);
    RefreshGpuRequestsFromEntries();
    registryRevision_ = registry.Revision();
    if (changed) {
        Touch();
    }
}

uint32_t EditorAssetThumbnailService::ProcessPreviewJobs(uint32_t maxJobsPerFrame) {
    const uint32_t processed = previewJobs_.ProcessBudgeted(maxJobsPerFrame);
    if (processed == 0) {
        return 0;
    }

    bool changed = false;
    for (EditorAssetThumbnailEntry& entry : entries_) {
        const EditorAssetPreviewJob* job = previewJobs_.Find(entry.key);
        if (job == nullptr) {
            continue;
        }
        const EditorAssetGpuThumbnailEntry* gpuEntry = gpuThumbnails_.Find(entry.key);
        EditorAssetThumbnailEntry updated = BuildEntry(job->record, &entry, job, gpuEntry);
        if (EntryDiffers(entry, updated)) {
            entry = std::move(updated);
            changed = true;
        }
    }
    RefreshGpuRequestsFromEntries();
    if (changed) {
        Touch();
    }
    return processed;
}

uint32_t EditorAssetThumbnailService::ProcessPreviewJobs(
    std::chrono::milliseconds maxTimePerFrame,
    uint32_t maxLaunchesPerFrame) {
    const uint32_t processed =
        previewJobs_.ProcessTimeBudgeted(maxTimePerFrame, maxLaunchesPerFrame);
    if (processed == 0) {
        return 0;
    }

    bool changed = false;
    for (EditorAssetThumbnailEntry& entry : entries_) {
        const EditorAssetPreviewJob* job = previewJobs_.Find(entry.key);
        if (job == nullptr) {
            continue;
        }
        const EditorAssetGpuThumbnailEntry* gpuEntry = gpuThumbnails_.Find(entry.key);
        EditorAssetThumbnailEntry updated = BuildEntry(job->record, &entry, job, gpuEntry);
        if (EntryDiffers(entry, updated)) {
            entry = std::move(updated);
            changed = true;
        }
    }
    RefreshGpuRequestsFromEntries();
    if (changed) {
        Touch();
    }
    return processed;
}

uint32_t EditorAssetThumbnailService::ProcessGpuThumbnails(uint32_t maxJobsPerFrame) {
    RefreshGpuRequestsFromEntries();
    const uint32_t processed = gpuThumbnails_.ProcessBudgeted(maxJobsPerFrame);
    if (processed == 0) {
        return 0;
    }

    bool changed = false;
    for (EditorAssetThumbnailEntry& entry : entries_) {
        if (UpdateEntryGpuState(entry)) {
            changed = true;
        }
    }
    if (changed) {
        Touch();
    }
    return processed;
}

bool EditorAssetThumbnailService::RetryPreview(std::string_view key) {
    const bool retried = previewJobs_.Retry(key);
    if (!retried) {
        return false;
    }
    for (EditorAssetThumbnailEntry& entry : entries_) {
        if (entry.key != key) {
            continue;
        }
        const EditorAssetPreviewJob* job = previewJobs_.Find(key);
        const EditorAssetGpuThumbnailEntry* gpuEntry = gpuThumbnails_.Find(key);
        EditorAssetThumbnailEntry updated = BuildEntry(job->record, &entry, job, gpuEntry);
        if (EntryDiffers(entry, updated)) {
            entry = std::move(updated);
            Touch();
        }
        break;
    }
    return true;
}

bool EditorAssetThumbnailService::RetryGpuThumbnail(std::string_view key) {
    const bool retried = gpuThumbnails_.Retry(key);
    if (!retried) {
        return false;
    }
    for (EditorAssetThumbnailEntry& entry : entries_) {
        if (entry.key == key && UpdateEntryGpuState(entry)) {
            Touch();
            break;
        }
    }
    return true;
}

EditorAssetThumbnailEntry EditorAssetThumbnailService::Resolve(
    const EditorAssetRecord& record) const {
    const std::string key = BuildEditorAssetThumbnailKey(record);
    if (const EditorAssetThumbnailEntry* entry = Find(key)) {
        return *entry;
    }
    return BuildEntry(record, nullptr, previewJobs_.Find(key), gpuThumbnails_.Find(key));
}

const EditorAssetThumbnailEntry* EditorAssetThumbnailService::Find(std::string_view key) const {
    const auto it = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&](const EditorAssetThumbnailEntry& entry) {
            return entry.key == key;
        });
    return it != entries_.end() ? &*it : nullptr;
}

std::size_t EditorAssetThumbnailService::Count(EditorAssetThumbnailStatus status) const {
    return static_cast<std::size_t>(std::count_if(
        entries_.begin(),
        entries_.end(),
        [&](const EditorAssetThumbnailEntry& entry) {
            return entry.status == status;
        }));
}

void EditorAssetThumbnailService::Touch() {
    ++revision_;
}

void EditorAssetThumbnailService::RefreshGpuRequestsFromEntries() {
    bool changed = false;
    for (EditorAssetThumbnailEntry& entry : entries_) {
        if (entry.status != EditorAssetThumbnailStatus::Ready) {
            continue;
        }
        if (gpuThumbnails_.Enqueue(BuildGpuThumbnailRequest(entry))) {
            changed = true;
        }
        if (UpdateEntryGpuState(entry)) {
            changed = true;
        }
    }
    if (changed) {
        Touch();
    }
}

bool EditorAssetThumbnailService::UpdateEntryGpuState(EditorAssetThumbnailEntry& entry) {
    EditorAssetThumbnailEntry updated = entry;
    ApplyGpuState(updated, gpuThumbnails_.Find(entry.key));
    if (!PreviewContentDiffers(entry, updated)) {
        return false;
    }
    const uint32_t previousGeneration = entry.generation;
    entry = std::move(updated);
    entry.generation = previousGeneration + 1;
    return true;
}

const char* ToString(EditorAssetThumbnailStatus status) {
    switch (status) {
    case EditorAssetThumbnailStatus::Missing:
        return "Missing";
    case EditorAssetThumbnailStatus::Unsupported:
        return "Unsupported";
    case EditorAssetThumbnailStatus::Pending:
        return "Pending";
    case EditorAssetThumbnailStatus::Ready:
        return "Ready";
    case EditorAssetThumbnailStatus::Failed:
        return "Failed";
    }
    return "Unknown";
}

std::string BuildEditorAssetThumbnailKey(const EditorAssetRecord& record) {
    if (!record.thumbnailKey.empty()) {
        return record.thumbnailKey;
    }
    std::string key = "thumb:";
    key += ToString(record.kind);
    key += ':';
    if (!record.guid.empty()) {
        key += record.guid;
    } else if (!record.logicalPath.empty()) {
        key += record.logicalPath;
    } else if (!record.sourcePath.empty()) {
        key += record.sourcePath;
    } else {
        key += record.id;
    }
    return key;
}

bool EditorAssetKindSupportsThumbnailPreview(EditorAssetKind kind) {
    return kind == EditorAssetKind::Texture ||
        kind == EditorAssetKind::Mesh ||
        kind == EditorAssetKind::Effect ||
        kind == EditorAssetKind::Course ||
        kind == EditorAssetKind::Prefab ||
        kind == EditorAssetKind::MaterialGraph ||
        kind == EditorAssetKind::VfxGraph ||
        kind == EditorAssetKind::AnimationStateMachine ||
        kind == EditorAssetKind::GameplayVisualScript ||
        kind == EditorAssetKind::Audio;
}

} // namespace editor
