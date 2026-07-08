#pragma once

#include <cstdint>
#include <functional>

#include "EditorPanelLayoutService.h"

struct ImDrawList;

namespace editor {

struct EditorContext;

struct EditorViewportPanelRenderInput {
    uint64_t textureId = 0;
    float sourceWidth = 0.0f;
    float sourceHeight = 0.0f;
    bool preserveAspect = true;
    std::function<void(ImDrawList*)> drawOverlay;
};

void DrawEditorViewportPanel(
    EditorContext& context,
    const EditorViewportPanelRenderInput& renderInput = {});
void DrawEditorViewportPanelContent(
    EditorContext& context,
    const EditorPanelRect& rect,
    const EditorViewportPanelRenderInput& renderInput = {});

} // namespace editor
