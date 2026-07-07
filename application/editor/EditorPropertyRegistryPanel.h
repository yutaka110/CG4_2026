#pragma once

#include "EditorPropertyRegistry.h"
#include "EditorTransactionStack.h"

namespace editor {

void DrawEditorPropertyRegistryPanel(
    const EditorPropertyRegistry& registry,
    const EditorTransactionStack* transactions);

} // namespace editor
