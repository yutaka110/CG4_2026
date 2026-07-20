#pragma once

#include <cstdint>
#include <functional>

#include "EditorPanelLayoutService.h"

struct ImDrawList;

namespace editor {

struct EditorContext;
class EditorViewportOverlayService;

struct EditorViewportPanelRenderInput {
    uint64_t textureId = 0;
    float sourceWidth = 0.0f;
    float sourceHeight = 0.0f;
    bool preserveAspect = true;
    std::function<void(EditorViewportOverlayService&)> buildOverlay;
};

EditorPanelRect ResolveEditorViewportRenderSurfaceRect(
    const EditorPanelRect& panelRect,
    const EditorViewportPanelRenderInput& renderInput);
bool EditorViewportOverlayUiContains(
    const EditorPanelRect& panelRect,
    float displayX,
    float displayY,
    float controlHeight);

void DrawEditorViewportPanel(
    EditorContext& context,
    const EditorViewportPanelRenderInput& renderInput = {});
void DrawEditorViewportPanelContent(
    EditorContext& context,
    const EditorPanelRect& rect,
    const EditorViewportPanelRenderInput& renderInput = {});

} // namespace editor
