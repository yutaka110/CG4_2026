#include "EditorAssetBrowserPanel.h"

#include "EditorAssetMutationExecutor.h"
#include "EditorAssetMutationSafety.h"
#include "EditorNotificationCenter.h"

#include "../../externals/imgui/imgui.h"

#include <array>
#include <cstdio>
#include <string>

namespace editor {
namespace {

constexpr EditorAssetKind kAssetKindFilters[] = {
    EditorAssetKind::Unknown,
    EditorAssetKind::Mesh,
    EditorAssetKind::Effect,
    EditorAssetKind::Course,
    EditorAssetKind::Texture,
    EditorAssetKind::Audio,
};

const char* FilterLabel(EditorAssetKind kind) {
    return kind == EditorAssetKind::Unknown ? "All" : ToString(kind);
}

bool ContainsFilterText(const EditorAssetRecord& record, const char* filterText) {
    if (filterText == nullptr || filterText[0] == '\0') {
        return true;
    }

    const std::string filter = filterText;
    if (record.id.find(filter) != std::string::npos ||
        record.guid.find(filter) != std::string::npos ||
        record.logicalPath.find(filter) != std::string::npos ||
        record.displayName.find(filter) != std::string::npos ||
        record.sourcePath.find(filter) != std::string::npos ||
        std::string(ToString(record.kind)).find(filter) != std::string::npos) {
        return true;
    }
    for (const std::string& tag : record.tags) {
        if (tag.find(filter) != std::string::npos) {
            return true;
        }
    }
    for (const std::string& dependency : record.dependencies) {
        if (dependency.find(filter) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string ShortGuid(const std::string& guid) {
    if (guid.size() <= 13) {
        return guid.empty() ? std::string("-") : guid;
    }
    return guid.substr(0, 13);
}

const char* MetadataLabel(const EditorAssetRecord& record) {
    if (record.hasMetadata) {
        return "meta";
    }
    return record.provisionalGuid ? "auto" : "-";
}

ImVec4 SafetyColor(EditorAssetMutationRisk risk) {
    switch (risk) {
    case EditorAssetMutationRisk::Allowed:
        return ImVec4(0.42f, 0.86f, 0.58f, 1.0f);
    case EditorAssetMutationRisk::Warning:
        return ImVec4(1.0f, 0.72f, 0.22f, 1.0f);
    case EditorAssetMutationRisk::Blocked:
        return ImVec4(1.0f, 0.25f, 0.22f, 1.0f);
    }
    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
}

void PushAssetMutationNotification(
    EditorNotificationCenter* notifications,
    const EditorAssetMutationResult& result) {
    if (notifications == nullptr || result.message.empty()) {
        return;
    }
    notifications->Push(
        result.succeeded
            ? (result.warning ? EditorNotificationSeverity::Warning : EditorNotificationSeverity::Info)
            : EditorNotificationSeverity::Error,
        "Asset",
        result.message);
}

void DrawMutationControls(
    const EditorAssetRecord& selectedRecord,
    EditorAssetRegistry& registry,
    EditorAssetSelection* assetSelection,
    EditorNotificationCenter* notifications) {
    static EditorAssetKind editedKind = EditorAssetKind::Unknown;
    static std::string editedId;
    static std::array<char, 128> renameBuffer{};
    static std::array<char, 260> moveBuffer{};

    if (editedKind != selectedRecord.kind || editedId != selectedRecord.id) {
        editedKind = selectedRecord.kind;
        editedId = selectedRecord.id;
        std::snprintf(renameBuffer.data(), renameBuffer.size(), "%s", selectedRecord.id.c_str());
        std::snprintf(moveBuffer.data(), moveBuffer.size(), "%s", selectedRecord.sourcePath.c_str());
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Mutation");
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputText("Rename Id", renameBuffer.data(), renameBuffer.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Rename")) {
        EditorAssetMutationExecutor executor(registry);
        const EditorAssetMutationResult result =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Rename,
                    selectedRecord.kind,
                    selectedRecord.id,
                    renameBuffer.data(),
                    {}});
        PushAssetMutationNotification(notifications, result);
        if (result.succeeded && assetSelection != nullptr) {
            assetSelection->SetPrimary(
                MakeEditorAssetHandle(result.updatedRecord, registry.Revision()));
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Renames the asset file stem, .meta file, registry id, and logical path.");
    }

    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputText("Move Path", moveBuffer.data(), moveBuffer.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Move")) {
        EditorAssetMutationExecutor executor(registry);
        const EditorAssetMutationResult result =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Move,
                    selectedRecord.kind,
                    selectedRecord.id,
                    {},
                    moveBuffer.data()});
        PushAssetMutationNotification(notifications, result);
        if (result.succeeded && assetSelection != nullptr) {
            assetSelection->SetPrimary(
                MakeEditorAssetHandle(result.updatedRecord, registry.Revision()));
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Moves the asset and .meta file. Destination must stay under Resources/.");
    }
}

} // namespace

void DrawEditorAssetBrowserPanel(const EditorAssetBrowserPanelContext& context) {
    if (context.registry == nullptr) {
        ImGui::TextUnformatted("Asset registry unavailable.");
        return;
    }

    EditorAssetRegistry& registry = *context.registry;
    static EditorAssetKind kindFilter = EditorAssetKind::Unknown;
    static std::array<char, 128> textFilter{};

    ImGui::Text(
        "Registry  Assets %u  Mesh %u  Meta %u  Auto GUID %u  Deps %u  Missing %u  Revision %u",
        static_cast<unsigned int>(registry.Count()),
        static_cast<unsigned int>(registry.Count(EditorAssetKind::Mesh)),
        static_cast<unsigned int>(registry.CountWithMetadata()),
        static_cast<unsigned int>(registry.CountWithProvisionalGuid()),
        static_cast<unsigned int>(registry.CountWithDependencies()),
        static_cast<unsigned int>(registry.CountMissing()),
        registry.Revision());

    const EditorAssetHandle* selectedAsset =
        context.assetSelection != nullptr ? context.assetSelection->Primary() : nullptr;
    if (selectedAsset != nullptr) {
        ImGui::Text(
            "Selected  %s:%s  GUID %s  Source %s",
            ToString(selectedAsset->kind),
            selectedAsset->id.c_str(),
            selectedAsset->guid.empty() ? "-" : selectedAsset->guid.c_str(),
            selectedAsset->sourcePath.empty() ? "-" : selectedAsset->sourcePath.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled(
            "%s / %s",
            selectedAsset->referenceable ? "Referenceable" : "Index only",
            selectedAsset->hasMetadata ? "meta" : "auto-guid");
        if (const EditorAssetRecord* selectedRecord =
                registry.Find(selectedAsset->kind, selectedAsset->id)) {
            const EditorAssetMutationSafetyReport renameSafety =
                EvaluateEditorAssetMutationSafety(
                    registry,
                    *selectedRecord,
                    EditorAssetMutationKind::Rename);
            const EditorAssetMutationSafetyReport moveSafety =
                EvaluateEditorAssetMutationSafety(
                    registry,
                    *selectedRecord,
                    EditorAssetMutationKind::Move);
            const EditorAssetMutationSafetyReport deleteSafety =
                EvaluateEditorAssetMutationSafety(
                    registry,
                    *selectedRecord,
                    EditorAssetMutationKind::Delete);
            ImGui::TextUnformatted("Safety");
            ImGui::SameLine();
            ImGui::TextColored(
                SafetyColor(renameSafety.risk),
                "Rename %s",
                ToString(renameSafety.risk));
            if (ImGui::IsItemHovered()) {
                const std::string report = FormatEditorAssetMutationSafetyReport(renameSafety);
                ImGui::SetTooltip("%s", report.c_str());
            }
            ImGui::SameLine();
            ImGui::TextColored(
                SafetyColor(moveSafety.risk),
                "Move %s",
                ToString(moveSafety.risk));
            if (ImGui::IsItemHovered()) {
                const std::string report = FormatEditorAssetMutationSafetyReport(moveSafety);
                ImGui::SetTooltip("%s", report.c_str());
            }
            ImGui::SameLine();
            ImGui::TextColored(
                SafetyColor(deleteSafety.risk),
                "Delete %s",
                ToString(deleteSafety.risk));
            if (ImGui::IsItemHovered()) {
                const std::string report = FormatEditorAssetMutationSafetyReport(deleteSafety);
                ImGui::SetTooltip("%s", report.c_str());
            }
            DrawMutationControls(
                *selectedRecord,
                registry,
                context.assetSelection,
                context.notifications);
        }
    } else {
        ImGui::TextUnformatted("Selected  none");
    }

    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("Kind", FilterLabel(kindFilter))) {
        for (EditorAssetKind candidate : kAssetKindFilters) {
            const bool selected = candidate == kindFilter;
            if (ImGui::Selectable(FilterLabel(candidate), selected)) {
                kindFilter = candidate;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputText("Filter", textFilter.data(), textFilter.size());
    ImGui::Separator();

    if (!ImGui::BeginTable(
            "EditorAssetBrowserTable",
            9,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 210.0f);
    ImGui::TableSetupColumn("GUID", ImGuiTableColumnFlags_WidthFixed, 122.0f);
    ImGui::TableSetupColumn("Display", ImGuiTableColumnFlags_WidthFixed, 210.0f);
    ImGui::TableSetupColumn("Meta", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Missing", ImGuiTableColumnFlags_WidthFixed, 68.0f);
    ImGui::TableSetupColumn("Ref", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn("Deps", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Source");
    ImGui::TableHeadersRow();

    for (const EditorAssetRecord& record : registry.Records()) {
        if (kindFilter != EditorAssetKind::Unknown && record.kind != kindFilter) {
            continue;
        }
        if (!ContainsFilterText(record, textFilter.data())) {
            continue;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(ToString(record.kind));
        ImGui::TableNextColumn();
        const bool selected =
            selectedAsset != nullptr &&
            selectedAsset->kind == record.kind &&
            selectedAsset->id == record.id;
        if (ImGui::Selectable(
                record.id.c_str(),
                selected,
                ImGuiSelectableFlags_SpanAllColumns)) {
            if (context.assetSelection != nullptr) {
                context.assetSelection->SetPrimary(
                    MakeEditorAssetHandle(record, registry.Revision()));
            }
        }
        ImGui::TableNextColumn();
        const std::string shortGuid = ShortGuid(record.guid);
        ImGui::TextUnformatted(shortGuid.c_str());
        if (ImGui::IsItemHovered() && !record.guid.empty()) {
            ImGui::SetTooltip(
                "GUID: %s\nLogical: %s\nMeta: %s",
                record.guid.c_str(),
                record.logicalPath.empty() ? "-" : record.logicalPath.c_str(),
                record.metadataPath.empty() ? "-" : record.metadataPath.c_str());
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.displayName.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(MetadataLabel(record));
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.missing ? "yes" : "no");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.referenceable ? "yes" : "no");
        ImGui::TableNextColumn();
        ImGui::Text("%u", static_cast<unsigned int>(record.dependencies.size()));
        if (ImGui::IsItemHovered() && !record.dependencies.empty()) {
            std::string tooltip = "Dependencies";
            for (const std::string& dependency : record.dependencies) {
                tooltip += "\n";
                tooltip += dependency;
            }
            ImGui::SetTooltip("%s", tooltip.c_str());
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.sourcePath.c_str());
    }

    ImGui::EndTable();
}

} // namespace editor
