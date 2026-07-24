#include "EditorGimmickRuntimeCommandQueue.h"

#include <utility>

namespace editor {
namespace {

void SetError(
    std::string* errorMessage,
    std::string message) {
    if (errorMessage != nullptr) {
        *errorMessage = std::move(message);
    }
}

} // namespace

bool EditorGimmickRuntimeCommandQueue::Enqueue(
    EditorGimmickRuntimeCommand command,
    std::string* errorMessage) {
    if (command.targetEntityGuid.empty() ||
        command.targetEntityGuid.size() > 256 ||
        command.sourceEntityGuid.size() > 256 ||
        command.payload.size() > 4096 ||
        command.hopCount >
            EditorGimmickRuntimeCommand::kMaximumHopCount) {
        SetError(
            errorMessage,
            "Runtime Gimmick command contains an invalid target, "
            "source, or payload.");
        ++droppedCount_;
        return false;
    }
    if (pending_.size() + processing_.size() >=
        maximumCommands_) {
        SetError(
            errorMessage,
            "Runtime Gimmick command queue capacity was exceeded.");
        ++droppedCount_;
        return false;
    }
    command.sequence = nextSequence_++;
    if (nextSequence_ == 0) nextSequence_ = 1;
    pending_.push_back(std::move(command));
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickRuntimeCommandQueue::BeginFrame(
    std::string* errorMessage) {
    if (!processing_.empty()) {
        SetError(
            errorMessage,
            "Runtime Gimmick command frame cannot begin before the "
            "current processing buffer is drained.");
        return false;
    }
    processing_.swap(pending_);
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

bool EditorGimmickRuntimeCommandQueue::Pop(
    EditorGimmickRuntimeCommand& output) {
    if (processing_.empty()) return false;
    output = std::move(processing_.front());
    processing_.pop_front();
    return true;
}

void EditorGimmickRuntimeCommandQueue::Clear() noexcept {
    pending_.clear();
    processing_.clear();
    nextSequence_ = 1;
    droppedCount_ = 0;
}

bool EditorGimmickRuntimeCommandQueue::SetMaximumCommands(
    std::size_t maximumCommands,
    std::string* errorMessage) {
    if (maximumCommands == 0 ||
        maximumCommands > 1048576 ||
        pending_.size() + processing_.size() >
            maximumCommands) {
        SetError(
            errorMessage,
            "Runtime Gimmick command queue capacity is invalid or "
            "smaller than the current queue.");
        return false;
    }
    maximumCommands_ = maximumCommands;
    if (errorMessage != nullptr) errorMessage->clear();
    return true;
}

const char* ToString(
    EditorGimmickRuntimeCommandKind kind) noexcept {
    switch (kind) {
    case EditorGimmickRuntimeCommandKind::Activate:
        return "ACTIVATE";
    case EditorGimmickRuntimeCommandKind::Deactivate:
        return "DEACTIVATE";
    case EditorGimmickRuntimeCommandKind::Toggle:
        return "TOGGLE";
    case EditorGimmickRuntimeCommandKind::Reset:
        return "RESET";
    case EditorGimmickRuntimeCommandKind::Enable:
        return "ENABLE";
    case EditorGimmickRuntimeCommandKind::Disable:
        return "DISABLE";
    }
    return "UNKNOWN";
}

} // namespace editor
