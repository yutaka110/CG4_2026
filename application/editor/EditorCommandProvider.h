#pragma once

namespace editor {

struct EditorContext;

class EditorCommandProvider {
public:
    virtual ~EditorCommandProvider() = default;
    virtual void RegisterCommands(EditorContext& context) const = 0;
};

} // namespace editor
