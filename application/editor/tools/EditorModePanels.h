#pragma once

#include "EditorInteractiveTool.h"

#include <cstddef>
#include <string>

namespace editor {

struct EditorContext;

std::size_t ResolveEditorInteractiveToolChoiceIndex(
    const EditorInteractiveToolProperty& property) noexcept;
std::string SerializeEditorInteractiveToolChoice(
    const EditorInteractiveToolProperty& property,
    std::size_t choiceIndex);

void DrawEditorModePalettePanel(EditorContext& context);
void DrawEditorToolPropertiesPanel(EditorContext& context);

} // namespace editor
