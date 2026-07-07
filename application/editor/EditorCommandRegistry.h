#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

struct EditorCommandResult {
    bool succeeded = false;
    std::string message;
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
    bool IsEnabled(const EditorCommand& command) const;
    std::string DisabledReason(const EditorCommand& command) const;

    std::size_t Count() const { return commands_.size(); }
    uint32_t Revision() const { return revision_; }
    const std::vector<EditorCommand>& Commands() const { return commands_; }

private:
    void Touch();

    std::vector<EditorCommand> commands_;
    uint32_t revision_ = 0;
};

} // namespace editor
