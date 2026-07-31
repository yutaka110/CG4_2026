#include "EditorWorldOutlinerPanel.h"

#include "../EditorNotificationCenter.h"
#include "../EditorSelection.h"
#include "../EditorTransactionStack.h"

#include "../../../externals/imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace editor {
namespace {

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool DirectMatch(
    const EditorWorldObjectRecord& record,
    const EditorWorldOutlinerState& state) {
    if (!state.showRuntimeObjects && record.runtimeOnly) return false;
    if (!state.showMissingObjects && record.missing) return false;
    if (state.domainFilter != EditorDomainId::Unknown && !record.virtualNode &&
        record.handle.domain != state.domainFilter) {
        return false;
    }
    const std::string search = Lower(state.search.data());
    if (search.empty()) return true;
    return Lower(record.displayName).find(search) != std::string::npos ||
        Lower(record.typeName).find(search) != std::string::npos ||
        Lower(record.handle.stableId).find(search) != std::string::npos;
}

bool SubtreeMatches(
    const EditorWorldObjectRecord& record,
    const EditorWorldModel& model,
    const EditorWorldOutlinerState& state,
    std::unordered_map<std::string, bool>& cache,
    std::unordered_set<std::string>& visiting) {
    const auto cached = cache.find(record.handle.stableId);
    if (cached != cache.end()) return cached->second;
    if (!visiting.insert(record.handle.stableId).second) return false;
    bool matches = DirectMatch(record, state);
    for (const EditorWorldObjectRecord* child : model.ChildrenOf(record.handle)) {
        if (child != nullptr && SubtreeMatches(*child, model, state, cache, visiting)) {
            matches = true;
        }
    }
    visiting.erase(record.handle.stableId);
    cache[record.handle.stableId] = matches;
    return matches;
}

std::vector<EditorObjectHandle> MutationTargets(
    const EditorWorldObjectRecord& clicked,
    const EditorSelection& selection) {
    if (selection.Contains(clicked.handle)) return selection.Handles();
    return {clicked.handle};
}

EditorWorldMutationResult Execute(
    EditorWorldMutationKind kind,
    std::vector<EditorObjectHandle> targets,
    const EditorWorldOutlinerPanelContext& context,
    std::string name = {},
    bool value = false,
    EditorObjectHandle parent = {}) {
    if (context.mutations == nullptr || context.transactions == nullptr) {
        return EditorWorldMutationResult{false, false, {}, {}, "World mutation service is unavailable."};
    }
    EditorWorldMutationRequest request{};
    request.kind = kind;
    request.targets = std::move(targets);
    request.newParent = std::move(parent);
    request.name = std::move(name);
    request.value = value;
    return context.mutations->Execute(
        request, *context.transactions, context.canMutateAuthoring);
}

void PublishMutation(
    const EditorWorldMutationResult& result,
    const EditorWorldOutlinerPanelContext& context) {
    if (result.succeeded) {
        if (context.selection != nullptr) context.selection->Set(result.resultingSelection);
        if (context.onSelectionChanged) {
            context.onSelectionChanged(
                result.resultingSelection.empty()
                    ? EditorObjectHandle{}
                    : result.resultingSelection.front());
        }
        if (context.onMutated) context.onMutated(result);
        if (context.notifications != nullptr) {
            context.notifications->Push(
                EditorNotificationSeverity::Info, "World Outliner", result.message);
        }
    } else if (context.notifications != nullptr) {
        context.notifications->Push(
            EditorNotificationSeverity::Error, "World Outliner", result.message);
    }
}

void BeginRename(EditorWorldOutlinerState& state, const EditorWorldObjectRecord& record) {
    state.renameStableId = record.handle.stableId;
    state.renameBuffer.fill('\0');
    strncpy_s(
        state.renameBuffer.data(), state.renameBuffer.size(),
        record.displayName.c_str(), _TRUNCATE);
}

void DrawRecord(
    const EditorWorldObjectRecord& record,
    EditorWorldOutlinerState& state,
    const EditorWorldOutlinerPanelContext& context,
    std::unordered_map<std::string, bool>& matchCache,
    std::unordered_set<std::string>& visiting,
    std::unordered_set<std::string>& rendered) {
    if (context.model == nullptr || context.selection == nullptr) return;
    if (!SubtreeMatches(record, *context.model, state, matchCache, visiting)) return;
    if (!rendered.insert(record.handle.stableId).second) return;

    const std::vector<const EditorWorldObjectRecord*> children =
        context.model->ChildrenOf(record.handle);
    const bool selected = context.selection->Contains(record.handle);
    ImGui::PushID(record.handle.stableId.c_str());

    if (!record.virtualNode) {
        const bool canVisibility = HasEditorWorldCapability(
            record.capabilities, EditorWorldObjectCapability::Visibility);
        ImGui::BeginDisabled(!context.canMutateAuthoring || !canVisibility || record.runtimeOnly);
        if (ImGui::SmallButton(record.visible ? "V" : "-")) {
            PublishMutation(
                Execute(EditorWorldMutationKind::SetVisibility,
                    MutationTargets(record, *context.selection), context, {}, !record.visible),
                context);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        const bool canRuntimeActivation = HasEditorWorldCapability(
            record.capabilities,
            EditorWorldObjectCapability::RuntimeActivation);
        ImGui::BeginDisabled(
            !context.canMutateAuthoring ||
            !canRuntimeActivation ||
            record.runtimeOnly);
        const char* runtimeLabel = !record.runtimeEnabled
            ? "X"
            : (record.runtimeActiveInHierarchy ? "R" : "r");
        if (ImGui::SmallButton(runtimeLabel)) {
            PublishMutation(
                Execute(
                    EditorWorldMutationKind::SetRuntimeEnabled,
                    MutationTargets(record, *context.selection),
                    context,
                    {},
                    !record.runtimeEnabled),
                context);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip(
                "%s",
                !record.runtimeEnabled
                    ? "Runtime Disabled on this Entity"
                    : (record.runtimeActiveInHierarchy
                        ? "Runtime Active"
                        : "Runtime Disabled by an ancestor"));
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        const bool canLock = HasEditorWorldCapability(
            record.capabilities, EditorWorldObjectCapability::Lock);
        ImGui::BeginDisabled(!context.canMutateAuthoring || !canLock || record.runtimeOnly);
        if (ImGui::SmallButton(record.locked ? "L" : "U")) {
            PublishMutation(
                Execute(EditorWorldMutationKind::SetLocked,
                    MutationTargets(record, *context.selection), context, {}, !record.locked),
                context);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (record.virtualNode) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    const std::string label = record.displayName.empty() ? record.typeName : record.displayName;
    const bool open = ImGui::TreeNodeEx("node", flags, "%s%s%s",
        record.runtimeOnly ? "[Runtime] " : "",
        record.missing ? "[Missing] " : "",
        label.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        if (ImGui::GetIO().KeyCtrl) context.selection->Toggle(record.handle);
        else context.selection->SetPrimary(record.handle);
        if (context.onSelectionChanged) context.onSelectionChanged(record.handle);
    }
    if (selected && state.scrollToStableId == record.handle.stableId) {
        ImGui::SetScrollHereY(0.5f);
        state.scrollToStableId.clear();
    }

    if (state.renameStableId == record.handle.stableId) {
        ImGui::Indent();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText(
                "##Rename", state.renameBuffer.data(), state.renameBuffer.size(),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            PublishMutation(
                Execute(EditorWorldMutationKind::Rename, {record.handle}, context,
                    state.renameBuffer.data()),
                context);
            state.renameStableId.clear();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) state.renameStableId.clear();
        ImGui::Unindent();
    }

    if (ImGui::BeginPopupContextItem("WorldObjectContext")) {
        const bool canCreate = context.canMutateAuthoring &&
            HasEditorWorldCapability(record.capabilities, EditorWorldObjectCapability::Create);
        ImGui::BeginDisabled(!canCreate);
        if (ImGui::MenuItem("Create Entity")) {
            PublishMutation(
                Execute(EditorWorldMutationKind::Create, {record.handle}, context, "Entity"),
                context);
        }
        ImGui::EndDisabled();
        if (canCreate) ImGui::Separator();
        const bool mutableObject = context.canMutateAuthoring && !record.runtimeOnly &&
            !record.virtualNode && !record.missing;
        ImGui::BeginDisabled(!mutableObject || record.locked ||
            !HasEditorWorldCapability(record.capabilities, EditorWorldObjectCapability::Rename));
        if (ImGui::MenuItem("Rename", "F2")) BeginRename(state, record);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!mutableObject || record.locked ||
            !HasEditorWorldCapability(record.capabilities, EditorWorldObjectCapability::Duplicate));
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            PublishMutation(
                Execute(EditorWorldMutationKind::Duplicate,
                    MutationTargets(record, *context.selection), context), context);
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!mutableObject || record.locked ||
            !HasEditorWorldCapability(record.capabilities, EditorWorldObjectCapability::Delete));
        if (ImGui::MenuItem("Delete", "Delete")) {
            state.pendingDelete = MutationTargets(record, *context.selection);
            ImGui::OpenPopup("Confirm World Delete");
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    if (HasEditorWorldCapability(record.capabilities, EditorWorldObjectCapability::Reparent) &&
        ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(
            "EDITOR_WORLD_OBJECT", record.handle.stableId.c_str(),
            record.handle.stableId.size() + 1);
        ImGui::TextUnformatted(label.c_str());
        ImGui::EndDragDropSource();
    }
    if ((record.virtualNode || HasEditorWorldCapability(
            record.capabilities, EditorWorldObjectCapability::Reparent)) &&
        ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_WORLD_OBJECT")) {
            const char* stableId = static_cast<const char*>(payload->Data);
            if (const EditorWorldObjectRecord* dragged = context.model->FindByStableId(stableId)) {
                PublishMutation(
                    Execute(EditorWorldMutationKind::Reparent,
                        {dragged->handle}, context, {}, false, record.handle), context);
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (open && !children.empty()) {
        for (const EditorWorldObjectRecord* child : children) {
            if (child != nullptr) DrawRecord(
                *child, state, context, matchCache, visiting, rendered);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

} // namespace

void DrawEditorWorldOutlinerPanel(
    EditorWorldOutlinerState& state,
    const EditorWorldOutlinerPanelContext& context) {
    if (context.model == nullptr || context.selection == nullptr) {
        ImGui::TextUnformatted("Editor World Model is unavailable.");
        return;
    }
    if (state.observedSelectionRevision != context.selection->Revision()) {
        state.observedSelectionRevision = context.selection->Revision();
        if (const EditorObjectHandle* primary = context.selection->Primary()) {
            state.scrollToStableId = primary->stableId;
        }
    }
    if (!ImGui::GetIO().WantTextInput) {
        if (const EditorObjectHandle* primary = context.selection->Primary()) {
            if (const EditorWorldObjectRecord* record = context.model->Resolve(*primary)) {
                if (ImGui::IsKeyPressed(ImGuiKey_F2) && context.canMutateAuthoring &&
                    !record->locked && !record->runtimeOnly &&
                    HasEditorWorldCapability(
                        record->capabilities, EditorWorldObjectCapability::Rename)) {
                    BeginRename(state, *record);
                }
                if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) &&
                    context.canMutateAuthoring && !record->locked &&
                    HasEditorWorldCapability(
                        record->capabilities, EditorWorldObjectCapability::Duplicate)) {
                    PublishMutation(
                        Execute(EditorWorldMutationKind::Duplicate,
                            context.selection->Handles(), context), context);
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Delete) && context.canMutateAuthoring &&
                    !record->locked && HasEditorWorldCapability(
                        record->capabilities, EditorWorldObjectCapability::Delete)) {
                    state.pendingDelete = context.selection->Handles();
                }
            }
        }
    }

    const EditorWorldObjectRecord* createRoot = nullptr;
    for (const EditorWorldObjectRecord& record : context.model->Objects()) {
        if (record.virtualNode && HasEditorWorldCapability(
                record.capabilities, EditorWorldObjectCapability::Create)) {
            createRoot = &record;
            break;
        }
    }
    ImGui::BeginDisabled(!context.canMutateAuthoring || createRoot == nullptr);
    if (ImGui::Button("+ Entity") && createRoot != nullptr) {
        PublishMutation(
            Execute(EditorWorldMutationKind::Create, {createRoot->handle}, context, "Entity"),
            context);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##WorldSearch", "Search world objects...",
        state.search.data(), state.search.size());
    const char* preview = state.domainFilter == EditorDomainId::Unknown
        ? "All Types" : ToString(state.domainFilter);
    if (ImGui::BeginCombo("Type", preview)) {
        if (ImGui::Selectable("All Types", state.domainFilter == EditorDomainId::Unknown)) {
            state.domainFilter = EditorDomainId::Unknown;
        }
        for (uint32_t value = static_cast<uint32_t>(EditorDomainId::Asset);
             value <= static_cast<uint32_t>(EditorDomainId::SceneEntity); ++value) {
            const EditorDomainId domain = static_cast<EditorDomainId>(value);
            if (ImGui::Selectable(ToString(domain), state.domainFilter == domain)) {
                state.domainFilter = domain;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Checkbox("Runtime", &state.showRuntimeObjects);
    ImGui::SameLine();
    ImGui::Checkbox("Missing", &state.showMissingObjects);
    ImGui::SameLine();
    ImGui::TextDisabled("Objects %u  Rev %llu",
        static_cast<unsigned int>(context.model->Count()),
        static_cast<unsigned long long>(context.model->Revision()));
    ImGui::Separator();

    if (ImGui::BeginChild("WorldHierarchy", ImVec2(0.0f, 0.0f), false)) {
        std::unordered_map<std::string, bool> matchCache;
        std::unordered_set<std::string> visiting;
        std::unordered_set<std::string> rendered;
        EditorObjectHandle root{};
        for (const EditorWorldObjectRecord* record : context.model->ChildrenOf(root)) {
            if (record != nullptr) DrawRecord(
                *record, state, context, matchCache, visiting, rendered);
        }
        if (state.showMissingObjects) {
            bool missingHeader = false;
            for (const EditorWorldObjectRecord& record : context.model->Objects()) {
                if (!record.missing || rendered.find(record.handle.stableId) != rendered.end()) continue;
                if (!missingHeader) {
                    ImGui::SeparatorText("Missing / Orphaned Objects");
                    missingHeader = true;
                }
                DrawRecord(record, state, context, matchCache, visiting, rendered);
            }
        }
    }
    ImGui::EndChild();

    if (!state.pendingDelete.empty()) ImGui::OpenPopup("Confirm World Delete");
    if (ImGui::BeginPopupModal("Confirm World Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete %u World object(s)?",
            static_cast<unsigned int>(state.pendingDelete.size()));
        ImGui::TextUnformatted("This operation is undoable until history eviction.");
        if (ImGui::Button("Delete")) {
            PublishMutation(
                Execute(EditorWorldMutationKind::Delete,
                    std::move(state.pendingDelete), context), context);
            state.pendingDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            state.pendingDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace editor
