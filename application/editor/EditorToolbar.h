#pragma once

namespace editor {

struct EditorContext;
class EditorToolRegistry;

void DrawEditorToolbar(EditorContext& context);
float EditorToolbarHeight();
void RegisterDefaultEditorToolbar(EditorToolRegistry& registry);

} // namespace editor
