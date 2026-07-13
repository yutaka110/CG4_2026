#pragma once

namespace editor {

struct EditorContext;
class EditorCommandRegistry;
class EditorToolRegistry;

void DrawEditorMenuBar(EditorContext& context);
void RegisterDefaultEditorMenu(EditorToolRegistry& tools, const EditorCommandRegistry& commands);

} // namespace editor
