#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>

namespace editor {

enum class EditorGimmickRuntimeCommandKind : uint32_t {
    Activate = 0,
    Deactivate,
    Toggle,
    Reset,
    Enable,
    Disable,
};

struct EditorGimmickRuntimeCommand {
    static constexpr uint32_t kMaximumHopCount = 64;

    uint64_t sequence = 0;
    uint32_t hopCount = 0;
    EditorGimmickRuntimeCommandKind kind =
        EditorGimmickRuntimeCommandKind::Activate;
    std::string targetEntityGuid;
    std::string sourceEntityGuid;
    std::string payload;
};

class EditorGimmickRuntimeCommandQueue {
public:
    static constexpr std::size_t kDefaultMaximumCommands = 4096;

    bool Enqueue(
        EditorGimmickRuntimeCommand command,
        std::string* errorMessage = nullptr);
    bool BeginFrame(std::string* errorMessage = nullptr);
    bool Pop(EditorGimmickRuntimeCommand& output);
    void Clear() noexcept;

    bool SetMaximumCommands(
        std::size_t maximumCommands,
        std::string* errorMessage = nullptr);
    std::size_t MaximumCommands() const noexcept {
        return maximumCommands_;
    }
    std::size_t PendingCount() const noexcept {
        return pending_.size();
    }
    std::size_t ProcessingCount() const noexcept {
        return processing_.size();
    }
    uint64_t NextSequence() const noexcept {
        return nextSequence_;
    }
    uint64_t DroppedCount() const noexcept {
        return droppedCount_;
    }

private:
    std::deque<EditorGimmickRuntimeCommand> pending_;
    std::deque<EditorGimmickRuntimeCommand> processing_;
    std::size_t maximumCommands_ = kDefaultMaximumCommands;
    uint64_t nextSequence_ = 1;
    uint64_t droppedCount_ = 0;
};

const char* ToString(
    EditorGimmickRuntimeCommandKind kind) noexcept;

} // namespace editor
