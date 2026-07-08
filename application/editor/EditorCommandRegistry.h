#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

class EditorNotificationCenter;

struct EditorCommandResult {
    bool succeeded = false;
    std::string message;
    bool warning = false;
};

struct EditorCommandExecutionStatus {
    bool hasResult = false;
    std::string commandId;
    bool succeeded = false;
    std::string message;
    uint32_t revision = 0;
};

struct EditorCommand {
    std::string id;
    std::string displayName;
    std::string category;
    std::string shortcut;
    std::function<bool()> enabled;
    std::function<std::string()> disabledReason;
    std::function<EditorCommandResult()> execute;
};

class EditorCommandRegistry {
public:
    void Clear();
    bool Register(EditorCommand command);

    const EditorCommand* Find(std::string_view id) const;
    EditorCommand* Find(std::string_view id);
    EditorCommandResult Execute(std::string_view id);
    void SetExecutionStatus(EditorCommandExecutionStatus* status) { executionStatus_ = status; }
    void SetNotificationCenter(EditorNotificationCenter* notifications) { notifications_ = notifications; }
    const EditorCommandExecutionStatus* ExecutionStatus() const { return executionStatus_; }
    bool IsEnabled(const EditorCommand& command) const;
    std::string DisabledReason(const EditorCommand& command) const;

    std::size_t Count() const { return commands_.size(); }
    uint32_t Revision() const { return revision_; }
    const std::vector<EditorCommand>& Commands() const { return commands_; }

private:
    void Touch();
    void RecordExecution(std::string_view id, const EditorCommandResult& result);

    std::vector<EditorCommand> commands_;
    EditorCommandExecutionStatus* executionStatus_ = nullptr;
    EditorNotificationCenter* notifications_ = nullptr;
    uint32_t revision_ = 0;
};

} // namespace editor
