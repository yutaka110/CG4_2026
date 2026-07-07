#pragma once

namespace editor {

class EditorAssetRegistry;
class EditorAssetSelection;
class EditorCommandInputRouter;
class EditorCommandPalette;
class EditorCommandRegistry;
class EditorPropertyAccessor;
class EditorPropertyRegistry;
class EditorSelection;
class EditorTransactionStack;
struct EditorCommandContext;
struct EditorValidationReport;

struct EditorContext {
    EditorSelection* selection = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorAssetRegistry* assets = nullptr;
    EditorAssetSelection* assetSelection = nullptr;
    EditorPropertyRegistry* propertyRegistry = nullptr;
    EditorPropertyAccessor* propertyAccessor = nullptr;
    const EditorValidationReport* validationReport = nullptr;

    EditorCommandRegistry* commands = nullptr;
    const EditorCommandContext* commandContext = nullptr;
    EditorCommandInputRouter* commandInputRouter = nullptr;
    EditorCommandPalette* commandPalette = nullptr;

    bool developerToolsVisible = false;

    bool HasCommandServices() const {
        return commands != nullptr && commandContext != nullptr;
    }
};

} // namespace editor
