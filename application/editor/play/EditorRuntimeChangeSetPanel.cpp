#include "EditorRuntimeChangeSetPanel.h"

#include "../EditorCommandRegistry.h"
#include "../EditorPlaySessionIsolationSnapshot.h"
#include "../EditorPlaySessionState.h"
#include "EditorRuntimeChangeSet.h"
#include "../../../externals/imgui/imgui.h"

#include <string>

namespace editor {

void DrawEditorRuntimeChangeSetPanel(
    EditorPlaySessionIsolationSnapshot& isolation,
    const EditorPlaySessionState& session,
    CourseAsset* course,
    AppRuntimeState* runtimeState,
    EffectRuntime* effectRuntime,
    PostProcessStack* postProcessStack,
    EditorCommandRegistry& commands) {
    ImGui::Text(
        "Runtime Changes  Providers %u  Session %llu",
        static_cast<unsigned int>(isolation.ProviderCount()),
        static_cast<unsigned long long>(session.SessionSerial()));
    ImGui::Separator();

    if (!session.IsActive()) {
        ImGui::TextDisabled("Enter Play or Simulate to inspect runtime differences.");
        return;
    }

    std::string error;
    if (!isolation.RefreshRuntimeChangeSet(
            EditorPlaySessionIsolationSnapshotTarget{
                course, runtimeState, effectRuntime, postProcessStack},
            &error)) {
        ImGui::TextWrapped("ChangeSet error: %s", error.c_str());
        return;
    }

    EditorRuntimeChangeSet& changes = isolation.RuntimeChanges();
    if (!changes.HasChanges()) {
        ImGui::TextDisabled("Runtime matches the captured Authoring state.");
        return;
    }

    if (ImGui::SmallButton("Select All")) changes.SelectAll(true);
    ImGui::SameLine();
    if (ImGui::SmallButton("Ignore All")) changes.SelectAll(false);
    ImGui::Separator();

    for (const EditorRuntimeChange& change : changes.Changes()) {
        bool selected = change.selected;
        const std::string widgetId = change.label + "##" + change.providerId + ":" + change.changeId;
        if (ImGui::Checkbox(widgetId.c_str(), &selected)) {
            changes.SetSelected(change.providerId, change.changeId, selected);
        }
        ImGui::TextDisabled(
            "  %s  %016llx -> %016llx",
            change.providerId.c_str(),
            static_cast<unsigned long long>(change.beforeFingerprint),
            static_cast<unsigned long long>(change.afterFingerprint));
    }

    ImGui::Separator();
    const bool canApply = changes.HasSelectedChanges();
    if (!canApply) ImGui::BeginDisabled();
    if (ImGui::Button("Keep Selected Changes")) {
        commands.Execute("editor.applyRuntimeChanges");
    }
    if (!canApply) ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled(
        "%u / %u selected",
        static_cast<unsigned int>(changes.SelectedCount()),
        static_cast<unsigned int>(changes.Count()));
}

} // namespace editor
