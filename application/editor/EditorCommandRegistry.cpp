#include "EditorCommandRegistry.h"

#include "EditorNotificationCenter.h"

#include <algorithm>
#include <utility>

namespace editor {

void EditorCommandRegistry::Clear() {
    if (commands_.empty()) {
        return;
    }
    commands_.clear();
    Touch();
}

bool EditorCommandRegistry::Register(EditorCommand command) {
    if (command.id.empty()) {
        return false;
    }

    auto it = std::find_if(
        commands_.begin(),
        commands_.end(),
        [&](const EditorCommand& existing) {
            return existing.id == command.id;
        });
    if (it != commands_.end()) {
        *it = std::move(command);
        Touch();
        return true;
    }

    commands_.push_back(std::move(command));
    Touch();
    return true;
}

const EditorCommand* EditorCommandRegistry::Find(std::string_view id) const {
    const auto it = std::find_if(
        commands_.begin(),
        commands_.end(),
        [&](const EditorCommand& command) {
            return command.id == id;
        });
    return it != commands_.end() ? &*it : nullptr;
}

EditorCommand* EditorCommandRegistry::Find(std::string_view id) {
    auto it = std::find_if(
        commands_.begin(),
        commands_.end(),
        [&](const EditorCommand& command) {
            return command.id == id;
        });
    return it != commands_.end() ? &*it : nullptr;
}

EditorCommandResult EditorCommandRegistry::Execute(std::string_view id) {
    EditorCommand* command = Find(id);
    if (command == nullptr) {
        const EditorCommandResult result{false, "Command not found."};
        RecordExecution(id, result);
        return result;
    }
    if (!IsEnabled(*command)) {
        const std::string reason = DisabledReason(*command);
        const EditorCommandResult result{false, reason.empty() ? std::string("Command is disabled.") : reason};
        RecordExecution(id, result);
        return result;
    }
    if (!command->execute) {
        const EditorCommandResult result{false, "Command has no execute callback."};
        RecordExecution(id, result);
        return result;
    }
    const EditorCommandResult result = command->execute();
    RecordExecution(id, result);
    return result;
}

bool EditorCommandRegistry::IsEnabled(const EditorCommand& command) const {
    return !command.enabled || command.enabled();
}

std::string EditorCommandRegistry::DisabledReason(const EditorCommand& command) const {
    if (IsEnabled(command)) {
        return {};
    }
    if (command.disabledReason) {
        return command.disabledReason();
    }
    return "Command is disabled.";
}

void EditorCommandRegistry::Touch() {
    ++revision_;
}

void EditorCommandRegistry::RecordExecution(std::string_view id, const EditorCommandResult& result) {
    if (executionStatus_ != nullptr) {
        executionStatus_->hasResult = true;
        executionStatus_->commandId = std::string(id);
        executionStatus_->succeeded = result.succeeded;
        executionStatus_->message = result.message;
        ++executionStatus_->revision;
    }

    if (notifications_ != nullptr) {
        notifications_->Push(
            result.succeeded
                ? (result.warning ? EditorNotificationSeverity::Warning : EditorNotificationSeverity::Info)
                : EditorNotificationSeverity::Error,
            std::string(id),
            result.message.empty()
                ? (result.succeeded ? std::string("Command succeeded.") : std::string("Command failed."))
                : result.message);
    }
}

} // namespace editor
