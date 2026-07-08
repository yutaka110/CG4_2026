#include "EditorAssetBrowserPanel.h"

#include "EditorAssetMutationExecutor.h"
#include "EditorAssetMutationSafety.h"
#include "EditorAssetImportService.h"
#include "EditorAssetThumbnailService.h"
#include "EditorNotificationCenter.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

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

std::string AssetLabel(const EditorAssetRecord& record) {
    return std::string(ToString(record.kind)) + ":" + record.id;
}

const char* ThumbnailLabel(const EditorAssetThumbnailEntry& thumbnail) {
    if (thumbnail.status == EditorAssetThumbnailStatus::Unsupported) {
        return "Icon";
    }
    return ToString(thumbnail.status);
}

ImVec4 ThumbnailStatusColor(EditorAssetThumbnailStatus status) {
    switch (status) {
    case EditorAssetThumbnailStatus::Ready:
        return ImVec4(0.42f, 0.86f, 0.58f, 1.0f);
    case EditorAssetThumbnailStatus::Unsupported:
        return ImVec4(0.58f, 0.68f, 0.84f, 1.0f);
    case EditorAssetThumbnailStatus::Pending:
        return ImVec4(1.0f, 0.72f, 0.22f, 1.0f);
    case EditorAssetThumbnailStatus::Missing:
    case EditorAssetThumbnailStatus::Failed:
        return ImVec4(1.0f, 0.25f, 0.22f, 1.0f);
    }
    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
}

EditorAssetThumbnailEntry ResolveThumbnail(
    const EditorAssetRecord& record,
    const EditorAssetThumbnailService* thumbnails) {
    if (thumbnails != nullptr) {
        return thumbnails->Resolve(record);
    }
    EditorAssetThumbnailEntry entry{};
    entry.key = BuildEditorAssetThumbnailKey(record);
    entry.kind = record.kind;
    entry.assetId = record.id;
    entry.label = ToString(record.kind);
    entry.detail = "Thumbnail service is unavailable.";
    entry.status = EditorAssetThumbnailStatus::Pending;
    entry.fallbackIcon = true;
    return entry;
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

std::vector<const EditorAssetRecord*> FilterVisibleAssets(
    const EditorAssetRegistry& registry,
    EditorAssetKind kindFilter,
    const char* textFilter) {
    std::vector<const EditorAssetRecord*> visible;
    for (const EditorAssetRecord& record : registry.Records()) {
        if (kindFilter != EditorAssetKind::Unknown && record.kind != kindFilter) {
            continue;
        }
        if (!ContainsFilterText(record, textFilter)) {
            continue;
        }
        visible.push_back(&record);
    }
    return visible;
}

void SelectAssetRecord(
    const EditorAssetRecord& record,
    EditorAssetRegistry& registry,
    EditorAssetSelection* assetSelection) {
    if (assetSelection == nullptr) {
        return;
    }
    assetSelection->SetPrimary(MakeEditorAssetHandle(record, registry.Revision()));
}

void DrawReferenceRow(
    const char* relationship,
    const EditorAssetRecord& record,
    const char* state,
    EditorAssetRegistry& registry,
    EditorAssetSelection* assetSelection) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(relationship);
    ImGui::TableNextColumn();
    const std::string label = AssetLabel(record);
    if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
        SelectAssetRecord(record, registry, assetSelection);
    }
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(state);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(record.sourcePath.empty() ? "-" : record.sourcePath.c_str());
}

void DrawReferenceTokenRow(
    const char* relationship,
    const std::string& token,
    const char* state) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(relationship);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(token.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(state);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("-");
}

void DrawReferenceInspector(
    const EditorAssetRecord& selectedRecord,
    EditorAssetRegistry& registry,
    EditorAssetSelection* assetSelection,
    const EditorAssetMutationSafetyReport& renameSafety,
    const EditorAssetMutationSafetyReport& moveSafety,
    const EditorAssetMutationSafetyReport& deleteSafety) {
    const std::vector<const EditorAssetRecord*> dependencies =
        registry.FindDependencies(selectedRecord);
    const std::vector<const EditorAssetRecord*> dependents =
        registry.FindDependents(selectedRecord);
    const std::vector<std::string> missingTokens =
        registry.FindMissingDependencyTokens(selectedRecord);
    const std::vector<std::string> malformedTokens =
        registry.FindMalformedDependencyTokens(selectedRecord);

    ImGui::Separator();
    ImGui::Text(
        "Reference Inspector  Token %s  Direct %u  Referenced By %u  Missing %u",
        BuildEditorAssetDependencyToken(selectedRecord).c_str(),
        static_cast<unsigned int>(dependencies.size()),
        static_cast<unsigned int>(dependents.size()),
        static_cast<unsigned int>(missingTokens.size() + malformedTokens.size()));
    ImGui::Text(
        "Mutation impact  Rename %s  Move %s  Delete %s",
        ToString(renameSafety.risk),
        ToString(moveSafety.risk),
        ToString(deleteSafety.risk));

    if (!ImGui::BeginTable(
            "EditorAssetReferenceInspector",
            4,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable)) {
        return;
    }

    ImGui::TableSetupColumn("Relation", ImGuiTableColumnFlags_WidthFixed, 112.0f);
    ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthFixed, 230.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 92.0f);
    ImGui::TableSetupColumn("Source");
    ImGui::TableHeadersRow();

    bool anyRow = false;
    for (const EditorAssetRecord* dependency : dependencies) {
        if (dependency == nullptr) {
            continue;
        }
        anyRow = true;
        DrawReferenceRow(
            "Depends On",
            *dependency,
            dependency->missing ? "missing file" : "ok",
            registry,
            assetSelection);
    }
    for (const EditorAssetRecord* dependent : dependents) {
        if (dependent == nullptr) {
            continue;
        }
        anyRow = true;
        DrawReferenceRow(
            "Referenced By",
            *dependent,
            dependent->missing ? "missing file" : "ok",
            registry,
            assetSelection);
    }
    for (const std::string& token : missingTokens) {
        anyRow = true;
        DrawReferenceTokenRow("Missing", token, "unregistered");
    }
    for (const std::string& token : malformedTokens) {
        anyRow = true;
        DrawReferenceTokenRow("Malformed", token, "invalid token");
    }

    if (!anyRow) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("-");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("No indexed asset references.");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("-");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("-");
    }

    ImGui::EndTable();
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

void PushAssetImportNotification(
    EditorNotificationCenter* notifications,
    const EditorAssetImportResult& result) {
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

EditorAssetExternalImportCollisionPolicy CollisionPolicyFromIndex(int index) {
    switch (index) {
    case 0:
        return EditorAssetExternalImportCollisionPolicy::Rename;
    case 1:
        return EditorAssetExternalImportCollisionPolicy::Skip;
    case 2:
        return EditorAssetExternalImportCollisionPolicy::Overwrite;
    }
    return EditorAssetExternalImportCollisionPolicy::Rename;
}

const char* CollisionPolicyLabel(int index) {
    return ToString(CollisionPolicyFromIndex(index));
}

void DrawProductionImportControls(
    EditorAssetRegistry& registry,
    EditorAssetSelection* assetSelection,
    EditorAssetThumbnailService* thumbnails,
    EditorNotificationCenter* notifications,
    const EditorAssetRecord* selectedRecord) {
    static std::array<char, 260> externalSourceBuffer{};
    static std::array<char, 128> destinationFolderBuffer{};
    static int collisionIndex = 0;
    static bool initialized = false;
    if (!initialized) {
        std::snprintf(destinationFolderBuffer.data(), destinationFolderBuffer.size(), "%s", "Imported");
        initialized = true;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Production Import");
    ImGui::SetNextItemWidth(340.0f);
    ImGui::InputText("External Source", externalSourceBuffer.data(), externalSourceBuffer.size());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::BeginCombo("Collision", CollisionPolicyLabel(collisionIndex))) {
        for (int index = 0; index < 3; ++index) {
            const bool selected = index == collisionIndex;
            if (ImGui::Selectable(CollisionPolicyLabel(index), selected)) {
                collisionIndex = index;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Destination", destinationFolderBuffer.data(), destinationFolderBuffer.size());
    ImGui::SameLine();
    if (ImGui::SmallButton("Import")) {
        EditorAssetExternalImportPolicy policy{};
        policy.destinationFolder =
            destinationFolderBuffer[0] != '\0'
                ? std::filesystem::path(destinationFolderBuffer.data())
                : std::filesystem::path("Imported");
        policy.collisionPolicy = CollisionPolicyFromIndex(collisionIndex);
        EditorAssetImportService importService(registry, thumbnails);
        const EditorAssetImportResult result =
            importService.ImportExternal(externalSourceBuffer.data(), policy);
        PushAssetImportNotification(notifications, result);
        if (result.succeeded && assetSelection != nullptr) {
            assetSelection->SetPrimary(MakeEditorAssetHandle(result.record, registry.Revision()));
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Copies an external file into Resources/<Destination>/ and imports it through the formal asset pipeline.");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Batch Meta")) {
        EditorAssetImportService importService(registry, thumbnails);
        const EditorAssetImportResult result = importService.BatchMigrateMetadata();
        PushAssetImportNotification(notifications, result);
        if (assetSelection != nullptr) {
            if (const EditorAssetHandle* selected = assetSelection->Primary()) {
                const EditorAssetHandle refreshed = RefreshEditorAssetHandle(registry, *selected);
                if (refreshed.Valid()) {
                    assetSelection->SetPrimary(refreshed);
                }
            }
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Creates durable .meta files for legacy/provisional assets.");
    }
    ImGui::SameLine();
    const bool canReimport = selectedRecord != nullptr && !selectedRecord->runtimeOnly;
    if (!canReimport) {
        ImGui::BeginDisabled();
    }
    if (ImGui::SmallButton("Reimport")) {
        EditorAssetImportService importService(registry, thumbnails);
        const EditorAssetImportResult result =
            importService.Reimport(selectedRecord->kind, selectedRecord->id);
        PushAssetImportNotification(notifications, result);
        if (result.succeeded && assetSelection != nullptr) {
            assetSelection->SetPrimary(MakeEditorAssetHandle(result.record, registry.Revision()));
        }
    }
    if (!canReimport) {
        ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reimports the selected asset while preserving its GUID.");
    }
}

void DrawMutationControls(
    const EditorAssetRecord& selectedRecord,
    EditorAssetRegistry& registry,
    EditorAssetSelection* assetSelection,
    EditorTransactionStack* transactions,
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
                    {},
                    transactions});
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
                    moveBuffer.data(),
                    transactions});
        PushAssetMutationNotification(notifications, result);
        if (result.succeeded && assetSelection != nullptr) {
            assetSelection->SetPrimary(
                MakeEditorAssetHandle(result.updatedRecord, registry.Revision()));
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Moves the asset and .meta file. Destination must stay under Resources/.");
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Delete")) {
        EditorAssetMutationExecutor executor(registry);
        const EditorAssetMutationResult result =
            executor.Execute(
                EditorAssetMutationRequest{
                    EditorAssetMutationKind::Delete,
                    selectedRecord.kind,
                    selectedRecord.id,
                    {},
                    {},
                    transactions});
        PushAssetMutationNotification(notifications, result);
        if (result.succeeded && assetSelection != nullptr) {
            assetSelection->Clear();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Deletes the asset and .meta file only when no indexed dependents exist.");
    }
}

void DrawThumbnailPreviewLine(
    const EditorAssetRecord& record,
    const EditorAssetThumbnailEntry& thumbnail) {
    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();
    ImGui::TextColored(
        ThumbnailStatusColor(thumbnail.status),
        "%s",
        ThumbnailLabel(thumbnail));
    ImGui::SameLine();
    ImGui::Text(
        "Key %s  Gen %u  Stamp %llu",
        thumbnail.key.empty() ? "-" : thumbnail.key.c_str(),
        thumbnail.generation,
        static_cast<unsigned long long>(thumbnail.sourceTimestamp));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "%s\nSource: %s\nRecord key: %s",
            thumbnail.detail.c_str(),
            record.sourcePath.empty() ? "-" : record.sourcePath.c_str(),
            record.thumbnailKey.empty() ? "-" : record.thumbnailKey.c_str());
    }
}

void DrawThumbnailTile(
    const EditorAssetRecord& record,
    bool selected,
    const EditorAssetThumbnailEntry& thumbnail,
    EditorAssetRegistry& registry,
    EditorAssetSelection* assetSelection) {
    ImGui::PushID((std::string(ToString(record.kind)) + ":" + record.id).c_str());
    const ImVec4 color = ThumbnailStatusColor(thumbnail.status);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 0.32f, color.y * 0.32f, color.z * 0.32f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.46f, color.y * 0.46f, color.z * 0.46f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 0.58f, color.y * 0.58f, color.z * 0.58f, 1.0f));
    if (ImGui::Button(thumbnail.label.empty() ? ToString(record.kind) : thumbnail.label.c_str(), ImVec2(92.0f, 58.0f))) {
        SelectAssetRecord(record, registry, assetSelection);
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "%s\n%s\n%s",
            AssetLabel(record).c_str(),
            ThumbnailLabel(thumbnail),
            thumbnail.detail.c_str());
    }
    if (selected) {
        ImGui::TextColored(ImVec4(0.42f, 0.86f, 0.58f, 1.0f), "%s", record.id.c_str());
    } else {
        ImGui::TextUnformatted(record.id.c_str());
    }
    ImGui::TextDisabled(
        "%s  %s",
        ToString(record.kind),
        ThumbnailLabel(thumbnail));
    ImGui::PopID();
}

void DrawAssetGrid(
    const std::vector<const EditorAssetRecord*>& visibleAssets,
    EditorAssetRegistry& registry,
    EditorAssetSelection* assetSelection,
    const EditorAssetHandle* selectedAsset,
    const EditorAssetThumbnailService* thumbnails) {
    const float tileWidth = 124.0f;
    const float availableWidth = (std::max)(tileWidth, ImGui::GetContentRegionAvail().x);
    const int columns = (std::max)(1, static_cast<int>(availableWidth / tileWidth));
    if (!ImGui::BeginTable(
            "EditorAssetBrowserGrid",
            columns,
            ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    int column = 0;
    for (const EditorAssetRecord* record : visibleAssets) {
        if (record == nullptr) {
            continue;
        }
        if (column == 0) {
            ImGui::TableNextRow();
        }
        ImGui::TableSetColumnIndex(column);
        const bool selected =
            selectedAsset != nullptr &&
            selectedAsset->kind == record->kind &&
            selectedAsset->id == record->id;
        DrawThumbnailTile(
            *record,
            selected,
            ResolveThumbnail(*record, thumbnails),
            registry,
            assetSelection);
        column = (column + 1) % columns;
    }

    ImGui::EndTable();
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
    static bool gridView = false;

    const EditorAssetThumbnailService* thumbnails = context.thumbnails;
    ImGui::Text(
        "Registry  Assets %u  Mesh %u  Meta %u  Auto GUID %u  Deps %u  Missing %u  Revision %u",
        static_cast<unsigned int>(registry.Count()),
        static_cast<unsigned int>(registry.Count(EditorAssetKind::Mesh)),
        static_cast<unsigned int>(registry.CountWithMetadata()),
        static_cast<unsigned int>(registry.CountWithProvisionalGuid()),
        static_cast<unsigned int>(registry.CountWithDependencies()),
        static_cast<unsigned int>(registry.CountMissing()),
        registry.Revision());
    if (thumbnails != nullptr) {
        ImGui::Text(
            "Preview Cache  Items %u  Ready %u  Icon %u  Failed %u  Missing %u  Revision %u",
            static_cast<unsigned int>(thumbnails->Count()),
            static_cast<unsigned int>(thumbnails->Count(EditorAssetThumbnailStatus::Ready)),
            static_cast<unsigned int>(thumbnails->Count(EditorAssetThumbnailStatus::Unsupported)),
            static_cast<unsigned int>(thumbnails->Count(EditorAssetThumbnailStatus::Failed)),
            static_cast<unsigned int>(thumbnails->Count(EditorAssetThumbnailStatus::Missing)),
            thumbnails->Revision());
    } else {
        ImGui::TextUnformatted("Preview Cache  unavailable");
    }

    const EditorAssetHandle* selectedAsset =
        context.assetSelection != nullptr ? context.assetSelection->Primary() : nullptr;
    const EditorAssetRecord* activeSelectedRecord = nullptr;
    if (selectedAsset != nullptr) {
        const EditorAssetHandleResolveResult selectedResolve =
            ResolveEditorAssetHandle(registry, *selectedAsset);
        ImGui::Text(
            "Selected  %s:%s  GUID %s  Source %s",
            ToString(selectedAsset->kind),
            selectedAsset->id.c_str(),
            selectedAsset->guid.empty() ? "-" : selectedAsset->guid.c_str(),
            selectedAsset->sourcePath.empty() ? "-" : selectedAsset->sourcePath.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled(
            "%s / %s / %s",
            selectedAsset->referenceable ? "Referenceable" : "Index only",
            selectedAsset->hasMetadata ? "meta" : "auto-guid",
            selectedResolve.Current() ? "current" : (selectedResolve.found ? "stale" : "missing"));
        if (const EditorAssetRecord* selectedRecord = selectedResolve.record) {
            activeSelectedRecord = selectedRecord;
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
            DrawThumbnailPreviewLine(
                *selectedRecord,
                ResolveThumbnail(*selectedRecord, thumbnails));
            DrawReferenceInspector(
                *selectedRecord,
                registry,
                context.assetSelection,
                renameSafety,
                moveSafety,
                deleteSafety);
            DrawMutationControls(
                *selectedRecord,
                registry,
                context.assetSelection,
                context.transactions,
                context.notifications);
        }
    } else {
        ImGui::TextUnformatted("Selected  none");
    }

    DrawProductionImportControls(
        registry,
        context.assetSelection,
        context.thumbnails,
        context.notifications,
        activeSelectedRecord);

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
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &gridView);
    ImGui::Separator();

    const std::vector<const EditorAssetRecord*> visibleAssets =
        FilterVisibleAssets(registry, kindFilter, textFilter.data());
    if (gridView) {
        DrawAssetGrid(
            visibleAssets,
            registry,
            context.assetSelection,
            selectedAsset,
            thumbnails);
        return;
    }

    if (!ImGui::BeginTable(
            "EditorAssetBrowserTable",
            11,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 210.0f);
    ImGui::TableSetupColumn("Thumb", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("GUID", ImGuiTableColumnFlags_WidthFixed, 122.0f);
    ImGui::TableSetupColumn("Display", ImGuiTableColumnFlags_WidthFixed, 210.0f);
    ImGui::TableSetupColumn("Meta", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Missing", ImGuiTableColumnFlags_WidthFixed, 68.0f);
    ImGui::TableSetupColumn("Ref", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn("Deps", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Refs", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("Source");
    ImGui::TableHeadersRow();

    for (const EditorAssetRecord* recordPtr : visibleAssets) {
        if (recordPtr == nullptr) {
            continue;
        }
        const EditorAssetRecord& record = *recordPtr;

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
        const EditorAssetThumbnailEntry thumbnail = ResolveThumbnail(record, thumbnails);
        ImGui::TextColored(
            ThumbnailStatusColor(thumbnail.status),
            "%s",
            ThumbnailLabel(thumbnail));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "%s\nKey: %s\nGeneration: %u",
                thumbnail.detail.c_str(),
                thumbnail.key.empty() ? "-" : thumbnail.key.c_str(),
                thumbnail.generation);
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
        const std::size_t dependentCount = registry.CountDependents(record);
        ImGui::Text("%u", static_cast<unsigned int>(dependentCount));
        if (ImGui::IsItemHovered() && dependentCount > 0) {
            std::string tooltip = "Referenced By";
            for (const EditorAssetRecord* dependent : registry.FindDependents(record)) {
                if (dependent != nullptr) {
                    tooltip += "\n";
                    tooltip += AssetLabel(*dependent);
                }
            }
            ImGui::SetTooltip("%s", tooltip.c_str());
        }
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.sourcePath.c_str());
    }

    ImGui::EndTable();
}

} // namespace editor
