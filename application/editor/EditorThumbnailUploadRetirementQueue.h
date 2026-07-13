#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <deque>

namespace editor {

struct EditorThumbnailUploadRetirementTelemetry {
    uint64_t pendingBytes = 0;
    uint64_t retiredBytes = 0;
    uint32_t pendingCount = 0;
    uint32_t retiredCount = 0;
};

class EditorThumbnailUploadRetirementQueue {
public:
    void Clear();
    void Enqueue(
        Microsoft::WRL::ComPtr<ID3D12Resource> resource,
        uint64_t byteSize,
        uint64_t retireFenceValue);
    void EnqueueForTesting(uint64_t byteSize, uint64_t retireFenceValue);
    uint32_t RetireCompleted(uint64_t completedFenceValue);

    const EditorThumbnailUploadRetirementTelemetry& Telemetry() const { return telemetry_; }
    uint32_t PendingCount() const { return telemetry_.pendingCount; }
    uint64_t PendingBytes() const { return telemetry_.pendingBytes; }

private:
    struct PendingUpload {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint64_t byteSize = 0;
        uint64_t retireFenceValue = 0;
    };

    std::deque<PendingUpload> pending_;
    EditorThumbnailUploadRetirementTelemetry telemetry_{};
};

} // namespace editor
