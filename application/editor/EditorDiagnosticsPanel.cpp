#include "EditorDiagnosticsPanel.h"

#include "EditorAssetReferenceDiagnosticsAdapter.h"
#include "EditorAssetSelection.h"

#include "../../externals/imgui/imgui.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace editor {
namespace {

ImVec4 SeverityColor(EditorValidationSeverity severity) {
    switch (severity) {
    case EditorValidationSeverity::Info:
        return ImVec4(0.48f, 0.78f, 1.0f, 1.0f);
    case EditorValidationSeverity::Warning:
        return ImVec4(1.0f, 0.72f, 0.22f, 1.0f);
    case EditorValidationSeverity::Error:
        return ImVec4(1.0f, 0.25f, 0.22f, 1.0f);
    }
    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
}

bool SeverityEnabled(
    EditorValidationSeverity severity,
    bool showErrors,
    bool showWarnings,
    bool showInfo) {
    switch (severity) {
    case EditorValidationSeverity::Error:
        return showErrors;
    case EditorValidationSeverity::Warning:
        return showWarnings;
    case EditorValidationSeverity::Info:
        return showInfo;
    }
    return false;
}

std::string ObjectLabel(const EditorObjectHandle& handle) {
    if (!handle.displayName.empty()) {
        return handle.displayName;
    }
    if (!handle.stableId.empty()) {
        return handle.stableId;
    }
    return "-";
}

bool IssueMatchesSelection(
    const EditorValidationIssue& issue,
    const EditorSelection* selection,
    const EditorAssetSelection* assetSelection) {
    if (selection != nullptr && !selection->Empty()) {
        for (const EditorObjectHandle& handle : selection->Handles()) {
            if (handle.SameObject(issue.target)) {
                return true;
            }
        }
    }

    if (issue.target.domain == EditorDomainId::Asset && assetSelection != nullptr) {
        const EditorAssetHandle* selectedAsset = assetSelection->Primary();
        return selectedAsset != nullptr &&
            issue.target.stableId ==
                BuildEditorAssetDiagnosticStableId(selectedAsset->kind, selectedAsset->id);
    }

    return false;
}

bool SelectAssetFromDiagnostic(
    const EditorValidationIssue& issue,
    const EditorAssetRegistry* registry,
    EditorAssetSelection* assetSelection) {
    if (issue.target.domain != EditorDomainId::Asset ||
        registry == nullptr ||
        assetSelection == nullptr) {
        return false;
    }

    constexpr std::string_view kPrefix = "asset:";
    const std::string_view stableId(issue.target.stableId);
    if (stableId.rfind(kPrefix, 0) != 0) {
        return false;
    }

    EditorAssetDependencyToken token{};
    if (!ParseEditorAssetDependencyToken(stableId.substr(kPrefix.size()), token)) {
        return false;
    }

    const EditorAssetRecord* record = registry->Find(token.kind, token.id);
    if (record == nullptr) {
        return false;
    }
    assetSelection->SetPrimary(MakeEditorAssetHandle(*record, registry->Revision()));
    return true;
}

} // namespace

void DrawEditorDiagnosticsPanel(const EditorDiagnosticsPanelContext& context) {
    if (context.validationReport == nullptr) {
        ImGui::TextUnformatted("Validation report unavailable.");
        return;
    }

    const EditorValidationReport& report = *context.validationReport;
    static bool selectedOnly = false;
    static bool showErrors = true;
    static bool showWarnings = true;
    static bool showInfo = true;

    ImGui::Text(
        "Diagnostics  Issues %u  Errors %u  Warnings %u  Info %u",
        static_cast<unsigned int>(report.issues.size()),
        report.errorCount,
        report.warningCount,
        report.infoCount);

    ImGui::Checkbox("Selected subject only", &selectedOnly);
    ImGui::SameLine();
    ImGui::Checkbox("Errors", &showErrors);
    ImGui::SameLine();
    ImGui::Checkbox("Warnings", &showWarnings);
    ImGui::SameLine();
    ImGui::Checkbox("Info", &showInfo);
    ImGui::Separator();

    if (!ImGui::BeginTable(
            "EditorDiagnosticsIssues",
            6,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 82.0f);
    ImGui::TableSetupColumn("Domain", ImGuiTableColumnFlags_WidthFixed, 135.0f);
    ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 230.0f);
    ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthFixed, 190.0f);
    ImGui::TableSetupColumn("Message");
    ImGui::TableHeadersRow();

    uint32_t visibleCount = 0;
    for (const EditorValidationIssue& issue : report.issues) {
        if (!SeverityEnabled(issue.severity, showErrors, showWarnings, showInfo)) {
            continue;
        }
        if (selectedOnly && !IssueMatchesSelection(issue, context.selection, context.assetSelection)) {
            continue;
        }

        ++visibleCount;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(SeverityColor(issue.severity), "%s", ToString(issue.severity));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ToString(issue.target.domain));
        ImGui::TableNextColumn();
        const std::string object = ObjectLabel(issue.target);
        if (issue.target.domain == EditorDomainId::Asset &&
            context.assetRegistry != nullptr &&
            context.assetSelection != nullptr) {
            if (ImGui::Selectable(
                    object.c_str(),
                    false,
                    ImGuiSelectableFlags_SpanAllColumns)) {
                SelectAssetFromDiagnostic(issue, context.assetRegistry, context.assetSelection);
            }
        } else {
            ImGui::TextUnformatted(object.c_str());
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(issue.propertyPath.empty() ? "-" : issue.propertyPath.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(issue.title.empty() ? "-" : issue.title.c_str());
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", issue.message.c_str());
    }

    if (visibleCount == 0) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("-");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("-");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("-");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("-");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("-");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("No diagnostics match the current filters.");
    }

    ImGui::EndTable();
}

} // namespace editor
