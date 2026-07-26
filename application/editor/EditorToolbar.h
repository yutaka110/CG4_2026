#pragma once

namespace editor {

struct EditorContext;
class EditorToolRegistry;
struct EditorToolbarItemDescriptor;

void DrawEditorToolbar(EditorContext& context);
float EditorToolbarHeight();
void RegisterDefaultEditorToolbar(EditorToolRegistry& registry);
bool EditorToolbarItemMatchesContext(
    const EditorContext& context,
    const EditorToolbarItemDescriptor& item);

} // namespace editor
