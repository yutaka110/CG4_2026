#include "EditorCommandRegistry.h"

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
        return {false, "Command not found."};
    }
    if (!IsEnabled(*command)) {
        return {false, "Command is disabled."};
    }
    if (!command->execute) {
        return {false, "Command has no execute callback."};
    }
    return command->execute();
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

} // namespace editor
