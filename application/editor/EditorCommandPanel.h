#pragma once

#include "EditorCommandContext.h"
#include "EditorContext.h"
#include "EditorCommandInputRouter.h"
#include "EditorCommandRegistry.h"

namespace editor {

void DrawEditorCommandPanel(EditorContext& context);

void DrawEditorCommandPanel(
    EditorCommandRegistry& registry,
    const EditorCommandInputRouter* inputRouter = nullptr,
    const EditorCommandContext* commandContext = nullptr);

} // namespace editor
