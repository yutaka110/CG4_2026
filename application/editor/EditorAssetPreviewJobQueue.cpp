#include "EditorAssetPreviewJobQueue.h"

#include <algorithm>
#include <utility>

namespace editor {
namespace {

bool TimeBudgetRemaining(
    std::chrono::steady_clock::time_point start,
    std::chrono::milliseconds maxTime) {
    if (maxTime <= std::chrono::milliseconds::zero()) {
        return false;
    }
    return std::chrono::steady_clock::now() - start < maxTime;
}

} // namespace

EditorAssetPreviewJobQueue::~EditorAssetPreviewJobQueue() {
    ShutdownWorkers();
}

void EditorAssetPreviewJobQueue::Clear() {
    bool hasPendingWork = false;
    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        hasPendingWork = !pendingWork_.empty();
    }
    bool hasCompletedWork = false;
    {
        std::lock_guard<std::mutex> lock(completedMutex_);
        hasCompletedWork = !completed_.empty();
    }
    if (jobs_.empty() && activeRunIds_.empty() && !hasPendingWork && !hasCompletedWork) {
        return;
    }
    jobs_.clear();
    activeRunIds_.clear();
    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        pendingWork_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(completedMutex_);
        completed_.clear();
    }
    Touch();
}

bool EditorAssetPreviewJobQueue::Enqueue(const EditorAssetRecord& record, std::string key) {
    if (key.empty()) {
        return false;
    }

    if (EditorAssetPreviewJob* existing = FindMutable(key)) {
        const bool sameSource =
            existing->sourceTimestamp == record.sourceTimestamp &&
            existing->record.sourcePath == record.sourcePath &&
            existing->record.kind == record.kind &&
            existing->record.id == record.id;
        if (sameSource && existing->status != EditorAssetPreviewJobStatus::Stale) {
            return false;
        }
        existing->status = EditorAssetPreviewJobStatus::Stale;
    }

    EditorAssetPreviewJob job{};
    job.key = std::move(key);
    job.record = record;
    job.sourceTimestamp = record.sourceTimestamp;
    job.status = EditorAssetPreviewJobStatus::Queued;
    jobs_.push_back(std::move(job));
    Touch();
    return true;
}

bool EditorAssetPreviewJobQueue::Retry(std::string_view key) {
    EditorAssetPreviewJob* job = FindMutable(key);
    if (job == nullptr || job->status != EditorAssetPreviewJobStatus::Failed) {
        return false;
    }
    job->status = EditorAssetPreviewJobStatus::Queued;
    job->runId = 0;
    job->message.clear();
    Touch();
    return true;
}

uint32_t EditorAssetPreviewJobQueue::ProcessBudgeted(uint32_t maxJobs) {
    if (maxJobs == 0) {
        return 0;
    }

    uint32_t processed = 0;
    EditorAssetPreviewProvider provider;
    for (EditorAssetPreviewJob& job : jobs_) {
        if (processed >= maxJobs) {
            break;
        }
        if (job.status != EditorAssetPreviewJobStatus::Queued) {
            continue;
        }

        job.status = EditorAssetPreviewJobStatus::Running;
        ++job.attempts;
        job.preview = provider.BuildPreview(job.record);
        job.message = job.preview.detail;
        job.status =
            job.preview.readiness == EditorAssetPreviewReadiness::Failed
                ? EditorAssetPreviewJobStatus::Failed
                : EditorAssetPreviewJobStatus::Ready;
        ++processed;
    }
    if (processed > 0) {
        Touch();
    }
    return processed;
}

uint32_t EditorAssetPreviewJobQueue::ProcessTimeBudgeted(
    std::chrono::milliseconds maxTime,
    uint32_t maxLaunchesPerFrame) {
    const auto start = std::chrono::steady_clock::now();
    uint32_t processed = CompleteReadyJobs();
    uint32_t launched = 0;

    while (launched < maxLaunchesPerFrame &&
        activeRunIds_.size() < maxActiveAsyncJobs_ &&
        TimeBudgetRemaining(start, maxTime)) {
        auto it = std::find_if(
            jobs_.begin(),
            jobs_.end(),
            [](const EditorAssetPreviewJob& job) {
                return job.status == EditorAssetPreviewJobStatus::Queued;
            });
        if (it == jobs_.end()) {
            break;
        }

        if (!LaunchQueuedJob(*it)) {
            break;
        }
        ++launched;
        ++processed;
    }

    if (TimeBudgetRemaining(start, maxTime)) {
        processed += CompleteReadyJobs();
    }
    return processed;
}

const EditorAssetPreviewJob* EditorAssetPreviewJobQueue::Find(std::string_view key) const {
    const auto it = std::find_if(
        jobs_.begin(),
        jobs_.end(),
        [&](const EditorAssetPreviewJob& job) {
            return job.key == key && job.status != EditorAssetPreviewJobStatus::Stale;
        });
    return it != jobs_.end() ? &*it : nullptr;
}

EditorAssetPreviewJob* EditorAssetPreviewJobQueue::FindMutable(std::string_view key) {
    const auto it = std::find_if(
        jobs_.begin(),
        jobs_.end(),
        [&](const EditorAssetPreviewJob& job) {
            return job.key == key && job.status != EditorAssetPreviewJobStatus::Stale;
        });
    return it != jobs_.end() ? &*it : nullptr;
}

std::size_t EditorAssetPreviewJobQueue::Count(EditorAssetPreviewJobStatus status) const {
    return static_cast<std::size_t>(std::count_if(
        jobs_.begin(),
        jobs_.end(),
        [&](const EditorAssetPreviewJob& job) {
            return job.status == status;
        }));
}

uint32_t EditorAssetPreviewJobQueue::CompleteReadyJobs() {
    std::vector<EditorAssetPreviewJobResult> completed;
    {
        std::lock_guard<std::mutex> lock(completedMutex_);
        completed.swap(completed_);
    }
    if (completed.empty()) {
        return 0;
    }

    uint32_t applied = 0;
    for (EditorAssetPreviewJobResult& result : completed) {
        activeRunIds_.erase(result.runId);

        EditorAssetPreviewJob* job = FindMutable(result.key);
        if (job == nullptr ||
            job->status != EditorAssetPreviewJobStatus::Running ||
            job->runId != result.runId ||
            job->sourceTimestamp != result.sourceTimestamp ||
            job->attempts != result.attempt) {
            continue;
        }

        job->preview = std::move(result.preview);
        job->message = job->preview.detail;
        job->status =
            job->preview.readiness == EditorAssetPreviewReadiness::Failed
                ? EditorAssetPreviewJobStatus::Failed
                : EditorAssetPreviewJobStatus::Ready;
        ++applied;
    }
    if (applied > 0 || !completed.empty()) {
        Touch();
    }
    return applied;
}

bool EditorAssetPreviewJobQueue::LaunchQueuedJob(EditorAssetPreviewJob& job) {
    if (job.status != EditorAssetPreviewJobStatus::Queued) {
        return false;
    }
    EnsureWorkersStarted();

    job.status = EditorAssetPreviewJobStatus::Running;
    ++job.attempts;
    job.runId = nextRunId_++;
    job.message = "Preview job is processing asynchronously.";
    activeRunIds_.insert(job.runId);

    EditorAssetPreviewWorkItem work{};
    work.record = job.record;
    work.key = job.key;
    work.sourceTimestamp = job.sourceTimestamp;
    work.runId = job.runId;
    work.attempt = job.attempts;
    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        pendingWork_.push_back(std::move(work));
    }
    workerCv_.notify_one();

    Touch();
    return true;
}

void EditorAssetPreviewJobQueue::EnsureWorkersStarted() {
    if (workersStarted_) {
        return;
    }

    workersStopping_ = false;
    workers_.reserve(maxActiveAsyncJobs_);
    for (uint32_t workerIndex = 0; workerIndex < maxActiveAsyncJobs_; ++workerIndex) {
        workers_.emplace_back([this]() {
            WorkerLoop();
        });
    }
    workersStarted_ = true;
}

void EditorAssetPreviewJobQueue::ShutdownWorkers() {
    if (!workersStarted_) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        workersStopping_ = true;
        pendingWork_.clear();
    }
    workerCv_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    workersStarted_ = false;
}

void EditorAssetPreviewJobQueue::WorkerLoop() {
    EditorAssetPreviewProvider provider;
    for (;;) {
        EditorAssetPreviewWorkItem work{};
        {
            std::unique_lock<std::mutex> lock(workerMutex_);
            workerCv_.wait(lock, [this]() {
                return workersStopping_ || !pendingWork_.empty();
            });
            if (workersStopping_ && pendingWork_.empty()) {
                return;
            }
            work = std::move(pendingWork_.front());
            pendingWork_.pop_front();
        }

        EditorAssetPreviewJobResult result{};
        result.key = std::move(work.key);
        result.sourceTimestamp = work.sourceTimestamp;
        result.runId = work.runId;
        result.attempt = work.attempt;
        result.preview = provider.BuildPreview(work.record);
        {
            std::lock_guard<std::mutex> lock(completedMutex_);
            completed_.push_back(std::move(result));
        }
    }
}

void EditorAssetPreviewJobQueue::Touch() {
    ++revision_;
}

const char* ToString(EditorAssetPreviewJobStatus status) {
    switch (status) {
    case EditorAssetPreviewJobStatus::Queued:
        return "Queued";
    case EditorAssetPreviewJobStatus::Running:
        return "Running";
    case EditorAssetPreviewJobStatus::Ready:
        return "Ready";
    case EditorAssetPreviewJobStatus::Failed:
        return "Failed";
    case EditorAssetPreviewJobStatus::Stale:
        return "Stale";
    case EditorAssetPreviewJobStatus::Cancelled:
        return "Cancelled";
    }
    return "Unknown";
}

} // namespace editor
