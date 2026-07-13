#pragma once

#include "EditorAssetPreviewProvider.h"
#include "EditorAssetRegistry.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace editor {

enum class EditorAssetPreviewJobStatus {
    Queued,
    Running,
    Ready,
    Failed,
    Stale,
    Cancelled,
};

struct EditorAssetPreviewJob {
    std::string key;
    EditorAssetRecord record;
    uint64_t sourceTimestamp = 0;
    uint64_t runId = 0;
    uint32_t attempts = 0;
    EditorAssetPreviewJobStatus status = EditorAssetPreviewJobStatus::Queued;
    EditorAssetPreviewInfo preview;
    std::string message;
};

struct EditorAssetPreviewJobResult {
    std::string key;
    uint64_t sourceTimestamp = 0;
    uint64_t runId = 0;
    uint32_t attempt = 0;
    EditorAssetPreviewInfo preview;
};

struct EditorAssetPreviewWorkItem {
    std::string key;
    EditorAssetRecord record;
    uint64_t sourceTimestamp = 0;
    uint64_t runId = 0;
    uint32_t attempt = 0;
};

class EditorAssetPreviewJobQueue {
public:
    ~EditorAssetPreviewJobQueue();

    void Clear();

    bool Enqueue(const EditorAssetRecord& record, std::string key);
    bool Retry(std::string_view key);
    uint32_t ProcessBudgeted(uint32_t maxJobs);
    uint32_t ProcessTimeBudgeted(
        std::chrono::milliseconds maxTime,
        uint32_t maxLaunchesPerFrame);

    const EditorAssetPreviewJob* Find(std::string_view key) const;
    EditorAssetPreviewJob* FindMutable(std::string_view key);

    std::size_t Count() const { return jobs_.size(); }
    std::size_t Count(EditorAssetPreviewJobStatus status) const;
    std::size_t ActiveAsyncJobCount() const { return activeRunIds_.size(); }
    uint32_t Revision() const { return revision_; }
    const std::vector<EditorAssetPreviewJob>& Jobs() const { return jobs_; }

private:
    uint32_t CompleteReadyJobs();
    void EnsureWorkersStarted();
    bool LaunchQueuedJob(EditorAssetPreviewJob& job);
    void ShutdownWorkers();
    void Touch();
    void WorkerLoop();

    std::vector<EditorAssetPreviewJob> jobs_;
    std::mutex workerMutex_;
    std::condition_variable workerCv_;
    std::deque<EditorAssetPreviewWorkItem> pendingWork_;
    std::vector<std::thread> workers_;
    bool workersStarted_ = false;
    bool workersStopping_ = false;
    std::mutex completedMutex_;
    std::vector<EditorAssetPreviewJobResult> completed_;
    std::unordered_set<uint64_t> activeRunIds_;
    uint64_t nextRunId_ = 1;
    uint32_t maxActiveAsyncJobs_ = 2;
    uint32_t revision_ = 0;
};

const char* ToString(EditorAssetPreviewJobStatus status);

} // namespace editor
