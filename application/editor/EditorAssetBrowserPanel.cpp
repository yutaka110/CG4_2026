#include "EditorAssetBrowserPanel.h"

#include "../../externals/imgui/imgui.h"

#include <array>
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
    return record.id.find(filter) != std::string::npos ||
        record.displayName.find(filter) != std::string::npos ||
        record.sourcePath.find(filter) != std::string::npos ||
        std::string(ToString(record.kind)).find(filter) != std::string::npos;
}

} // namespace

void DrawEditorAssetBrowserPanel(const EditorAssetBrowserPanelContext& context) {
    if (context.registry == nullptr) {
        ImGui::TextUnformatted("Asset registry unavailable.");
        return;
    }

    const EditorAssetRegistry& registry = *context.registry;
    static EditorAssetKind kindFilter = EditorAssetKind::Unknown;
    static std::array<char, 128> textFilter{};

    ImGui::Text(
        "Registry  Assets %u  Mesh %u  Revision %u",
        static_cast<unsigned int>(registry.Count()),
        static_cast<unsigned int>(registry.Count(EditorAssetKind::Mesh)),
        registry.Revision());

    const EditorAssetHandle* selectedAsset =
        context.assetSelection != nullptr ? context.assetSelection->Primary() : nullptr;
    if (selectedAsset != nullptr) {
        ImGui::Text(
            "Selected  %s:%s  Source %s",
            ToString(selectedAsset->kind),
            selectedAsset->id.c_str(),
            selectedAsset->sourcePath.empty() ? "-" : selectedAsset->sourcePath.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled(selectedAsset->referenceable ? "Referenceable" : "Index only");
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
            6,
            ImGuiTableFlags_Borders |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY,
            ImVec2(0.0f, 0.0f))) {
        return;
    }

    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 210.0f);
    ImGui::TableSetupColumn("Display", ImGuiTableColumnFlags_WidthFixed, 210.0f);
    ImGui::TableSetupColumn("Runtime", ImGuiTableColumnFlags_WidthFixed, 74.0f);
    ImGui::TableSetupColumn("Ref", ImGuiTableColumnFlags_WidthFixed, 52.0f);
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
        ImGui::TextUnformatted(record.displayName.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.runtimeOnly ? "yes" : "no");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.referenceable ? "yes" : "no");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(record.sourcePath.c_str());
    }

    ImGui::EndTable();
}

} // namespace editor
