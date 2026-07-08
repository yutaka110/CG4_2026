#pragma once

#include <cstdint>

namespace editor {

struct EditorContext;

struct EditorViewportPanelRenderInput {
    uint64_t textureId = 0;
    float sourceWidth = 0.0f;
    float sourceHeight = 0.0f;
    bool preserveAspect = true;
};

void DrawEditorViewportPanel(
    EditorContext& context,
    const EditorViewportPanelRenderInput& renderInput = {});

} // namespace editor
