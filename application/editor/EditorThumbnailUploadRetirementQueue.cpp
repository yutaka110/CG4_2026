#include "EditorThumbnailUploadRetirementQueue.h"

namespace editor {

void EditorThumbnailUploadRetirementQueue::Clear() {
    pending_.clear();
    telemetry_ = {};
}

void EditorThumbnailUploadRetirementQueue::Enqueue(
    Microsoft::WRL::ComPtr<ID3D12Resource> resource,
    uint64_t byteSize,
    uint64_t retireFenceValue) {
    if (resource == nullptr || retireFenceValue == 0) {
        return;
    }
    PendingUpload upload{};
    upload.resource = std::move(resource);
    upload.byteSize = byteSize;
    upload.retireFenceValue = retireFenceValue;
    pending_.push_back(std::move(upload));
    telemetry_.pendingBytes += byteSize;
    telemetry_.pendingCount = static_cast<uint32_t>(pending_.size());
}

void EditorThumbnailUploadRetirementQueue::EnqueueForTesting(
    uint64_t byteSize,
    uint64_t retireFenceValue) {
    if (retireFenceValue == 0) {
        return;
    }
    PendingUpload upload{};
    upload.byteSize = byteSize;
    upload.retireFenceValue = retireFenceValue;
    pending_.push_back(std::move(upload));
    telemetry_.pendingBytes += byteSize;
    telemetry_.pendingCount = static_cast<uint32_t>(pending_.size());
}

uint32_t EditorThumbnailUploadRetirementQueue::RetireCompleted(uint64_t completedFenceValue) {
    uint32_t retired = 0;
    while (!pending_.empty() && pending_.front().retireFenceValue <= completedFenceValue) {
        telemetry_.pendingBytes -= pending_.front().byteSize;
        telemetry_.retiredBytes += pending_.front().byteSize;
        pending_.pop_front();
        ++retired;
        ++telemetry_.retiredCount;
    }
    telemetry_.pendingCount = static_cast<uint32_t>(pending_.size());
    return retired;
}

} // namespace editor
