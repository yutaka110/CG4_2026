#include "EditorModeRegistry.h"

#include <algorithm>
#include <utility>

namespace editor {
namespace {

class EditorSelectionInspectorTool final : public IEditorInteractiveTool {
public:
    bool Activate(
        const EditorInteractiveToolEnvironment& environment,
        std::string& outError) override {
        if (environment.selection == nullptr || environment.selection->Empty()) {
            outError = "Select one or more objects before starting Selection Inspector.";
            return false;
        }
        selectionCount_ = environment.selection->Count();
        if (const EditorObjectHandle* primary = environment.selection->Primary()) {
            primary_ = primary->displayName.empty() ? primary->stableId : primary->displayName;
            domain_ = ToString(primary->domain);
        }
        return true;
    }

    void Tick(
        const EditorInteractiveToolEnvironment&,
        const EditorInteractiveToolFrameInput&) override {}

    EditorInteractiveToolAcceptResult BuildAccept(
        const EditorInteractiveToolEnvironment&) override {
        return EditorInteractiveToolAcceptResult::Success("Selection inspection completed.");
    }

    void Cancel(EditorInteractiveToolEndReason) override {}

    std::string ViewportHint() const override {
        return "Inspecting " + std::to_string(selectionCount_) +
            " selected object(s). Enter: finish, Esc: cancel";
    }

    std::vector<EditorInteractiveToolProperty> Properties() const override {
        return {
            {"Selection Count", std::to_string(selectionCount_),
                "Selection captured when the tool was activated."},
            {"Primary", primary_.empty() ? "None" : primary_, {}},
            {"Domain", domain_.empty() ? "Unknown" : domain_, {}}};
    }

private:
    std::size_t selectionCount_ = 0;
    std::string primary_;
    std::string domain_;
};

} // namespace

void EditorModeRegistry::Clear() {
    modes_.clear();
    tools_.clear();
    diagnostics_.clear();
    Touch();
}

bool EditorModeRegistry::RegisterMode(EditorModeDescriptor descriptor) {
    if (descriptor.id.empty() || descriptor.label.empty()) {
        AddError(descriptor.id, "Mode id and label are required.");
        return false;
    }
    if (FindMode(descriptor.id) != nullptr) {
        AddError(descriptor.id, "Mode id is already registered.");
        return false;
    }
    modes_.push_back(std::move(descriptor));
    Touch();
    return true;
}

bool EditorModeRegistry::RegisterTool(EditorInteractiveToolDescriptor descriptor) {
    if (descriptor.id.empty() || descriptor.modeId.empty() || descriptor.label.empty()) {
        AddError(descriptor.id, "Tool id, mode id, and label are required.");
        return false;
    }
    if (FindTool(descriptor.id) != nullptr) {
        AddError(descriptor.id, "Tool id is already registered.");
        return false;
    }
    if (FindMode(descriptor.modeId) == nullptr) {
        AddError(descriptor.id, "Tool references an unregistered editor mode.");
        return false;
    }
    if (!descriptor.build) {
        AddError(descriptor.id, "Tool builder is required.");
        return false;
    }
    tools_.push_back(std::move(descriptor));
    Touch();
    return true;
}

const EditorModeDescriptor* EditorModeRegistry::FindMode(std::string_view id) const {
    const auto found = std::find_if(
        modes_.begin(), modes_.end(),
        [id](const EditorModeDescriptor& mode) { return mode.id == id; });
    return found != modes_.end() ? &*found : nullptr;
}

const EditorInteractiveToolDescriptor* EditorModeRegistry::FindTool(
    std::string_view id) const {
    const auto found = std::find_if(
        tools_.begin(), tools_.end(),
        [id](const EditorInteractiveToolDescriptor& tool) { return tool.id == id; });
    return found != tools_.end() ? &*found : nullptr;
}

std::vector<const EditorModeDescriptor*> EditorModeRegistry::Modes() const {
    std::vector<const EditorModeDescriptor*> result;
    result.reserve(modes_.size());
    for (const EditorModeDescriptor& mode : modes_) result.push_back(&mode);
    std::stable_sort(
        result.begin(), result.end(),
        [](const EditorModeDescriptor* lhs, const EditorModeDescriptor* rhs) {
            return lhs->sortOrder < rhs->sortOrder;
        });
    return result;
}

std::vector<const EditorInteractiveToolDescriptor*> EditorModeRegistry::ToolsForMode(
    std::string_view modeId) const {
    std::vector<const EditorInteractiveToolDescriptor*> result;
    for (const EditorInteractiveToolDescriptor& tool : tools_) {
        if (tool.modeId == modeId) result.push_back(&tool);
    }
    std::stable_sort(
        result.begin(), result.end(),
        [](const EditorInteractiveToolDescriptor* lhs,
           const EditorInteractiveToolDescriptor* rhs) {
            if (lhs->category != rhs->category) return lhs->category < rhs->category;
            return lhs->sortOrder < rhs->sortOrder;
        });
    return result;
}

void EditorModeRegistry::AddError(std::string id, std::string message) {
    diagnostics_.push_back(EditorModeRegistryDiagnostic{
        EditorModeRegistryDiagnosticSeverity::Error,
        std::move(id), std::move(message)});
}

void EditorModeRegistry::Touch() {
    ++revision_;
    if (revision_ == 0) ++revision_;
}

void RegisterDefaultEditorModes(EditorModeRegistry& registry) {
    registry.RegisterMode(EditorModeDescriptor{
        "editor.mode.select", "Select", "Select and transform scene objects.", "Shift+1", 100});
    registry.RegisterMode(EditorModeDescriptor{
        "editor.mode.inspect", "Inspect", "Run read-only interactive inspection tools.", "Shift+2", 200});
    registry.RegisterTool(EditorInteractiveToolDescriptor{
        "editor.tool.selectionInspector",
        "editor.mode.inspect",
        "Selection Inspector",
        "Inspection",
        "Capture and inspect the current editor selection without modifying authoring data.",
        "I",
        100,
        true,
        false,
        false,
        true,
        EditorInteractiveToolTransactionPolicy::None,
        []() { return std::make_unique<EditorSelectionInspectorTool>(); }});
}

const char* ToString(EditorModeRegistryDiagnosticSeverity severity) {
    switch (severity) {
    case EditorModeRegistryDiagnosticSeverity::Warning: return "Warning";
    case EditorModeRegistryDiagnosticSeverity::Error: return "Error";
    }
    return "Unknown";
}

} // namespace editor
