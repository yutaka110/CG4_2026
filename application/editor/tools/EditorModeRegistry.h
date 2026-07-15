#pragma once

#include "EditorInteractiveTool.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

struct EditorModeDescriptor {
    std::string id;
    std::string label;
    std::string description;
    std::string shortcut;
    int sortOrder = 0;
};

struct EditorInteractiveToolDescriptor {
    std::string id;
    std::string modeId;
    std::string label;
    std::string category;
    std::string description;
    std::string shortcut;
    int sortOrder = 0;
    bool requiresSelection = false;
    bool requiresViewport = true;
    bool requiresAuthoring = true;
    bool cancelOnSelectionChange = true;
    EditorInteractiveToolTransactionPolicy transactionPolicy =
        EditorInteractiveToolTransactionPolicy::SingleCommandOnAccept;
    std::function<std::unique_ptr<IEditorInteractiveTool>()> build;
};

enum class EditorModeRegistryDiagnosticSeverity {
    Warning,
    Error,
};

struct EditorModeRegistryDiagnostic {
    EditorModeRegistryDiagnosticSeverity severity =
        EditorModeRegistryDiagnosticSeverity::Error;
    std::string id;
    std::string message;
};

class EditorModeRegistry {
public:
    void Clear();
    bool RegisterMode(EditorModeDescriptor descriptor);
    bool RegisterTool(EditorInteractiveToolDescriptor descriptor);

    const EditorModeDescriptor* FindMode(std::string_view id) const;
    const EditorInteractiveToolDescriptor* FindTool(std::string_view id) const;
    std::vector<const EditorModeDescriptor*> Modes() const;
    std::vector<const EditorInteractiveToolDescriptor*> ToolsForMode(
        std::string_view modeId) const;

    const std::vector<EditorModeRegistryDiagnostic>& Diagnostics() const {
        return diagnostics_;
    }
    std::size_t ModeCount() const { return modes_.size(); }
    std::size_t ToolCount() const { return tools_.size(); }
    uint32_t Revision() const { return revision_; }

private:
    void AddError(std::string id, std::string message);
    void Touch();

    std::vector<EditorModeDescriptor> modes_;
    std::vector<EditorInteractiveToolDescriptor> tools_;
    std::vector<EditorModeRegistryDiagnostic> diagnostics_;
    uint32_t revision_ = 0;
};

void RegisterDefaultEditorModes(EditorModeRegistry& registry);

const char* ToString(EditorModeRegistryDiagnosticSeverity severity);

} // namespace editor
