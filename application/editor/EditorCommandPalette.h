#pragma once

#include "EditorCommandRegistry.h"

#include <array>
#include <string>

namespace editor {

struct EditorContext;

class EditorCommandPalette {
public:
    void Open();
    void Close();
    void Draw(EditorContext& context);
    void Draw(EditorCommandRegistry& registry);

    bool IsOpen() const { return open_; }
    const std::string& LastResult() const { return lastResult_; }

private:
    bool CommandMatchesFilter(const EditorCommand& command) const;
    bool Execute(EditorCommandRegistry& registry, const EditorCommand& command);

    bool open_ = false;
    bool openRequested_ = false;
    bool focusSearchOnOpen_ = false;
    std::array<char, 128> filter_{};
    std::string lastResult_;
};

} // namespace editor
