#include "EditorStatusBar.h"

#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorAuthoringMutationGuard.h"
#include "EditorCommandRegistry.h"
#include "EditorContext.h"
#include "EditorDirtyStateService.h"
#include "EditorDocumentLifecycleService.h"
#include "EditorLayoutService.h"
#include "EditorModalConfirmService.h"
#include "EditorNotificationCenter.h"
#include "EditorSaveApplyPolicy.h"
#include "EditorSelection.h"
#include "EditorValidation.h"

#include "../../externals/imgui/imgui.h"

namespace editor {

float EditorStatusBarHeight() {
    return 26.0f;
}

void DrawEditorStatusBar(EditorContext& context) {
    if (!context.developerToolsVisible) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 workSize = viewport ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    const float statusHeight =
        context.layout != nullptr ? context.layout->StatusBarHeight() : EditorStatusBarHeight();
    if (statusHeight <= 0.0f) {
        return;
    }

    ImGui::SetNextWindowPos(
        ImVec2(workPos.x, workPos.y + workSize.y - statusHeight),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(workSize.x, statusHeight), ImGuiCond_Always);
    if (!ImGui::Begin("Editor Status Bar", nullptr, flags)) {
        ImGui::End();
        return;
    }

    const uint32_t errors = context.validationReport != nullptr ? context.validationReport->errorCount : 0;
    const uint32_t warnings = context.validationReport != nullptr ? context.validationReport->warningCount : 0;
    const std::size_t selectedObjects = context.selection != nullptr ? context.selection->Count() : 0;
    const std::size_t commandCount = context.commands != nullptr ? context.commands->Count() : 0;

    ImGui::Text(
        "Validation E:%u W:%u  Selection:%u  Commands:%u",
        errors,
        warnings,
        static_cast<unsigned int>(selectedObjects),
        static_cast<unsigned int>(commandCount));

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    if (context.assetSelection != nullptr) {
        const EditorAssetHandle* selectedAsset = context.assetSelection->Primary();
        if (selectedAsset != nullptr) {
            ImGui::Text(
                "Asset %s:%s",
                ToString(selectedAsset->kind),
                selectedAsset->id.c_str());
        } else {
            ImGui::TextUnformatted("Asset none");
        }
    } else {
        ImGui::TextUnformatted("Asset unavailable");
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    if (context.playSession != nullptr) {
        const EditorAuthoringMutationGuard mutationGuard =
            MakeEditorAuthoringMutationGuard(context.playSession);
        ImGui::Text(
            "Session %s f:%u %s",
            ToString(context.playSession->Mode()),
            static_cast<unsigned int>(context.playSession->FrameCount()),
            mutationGuard.StateLabel());
        if (ImGui::IsItemHovered()) {
            if (mutationGuard.LockedByPlaySession()) {
                ImGui::SetTooltip(
                    "%s %s",
                    mutationGuard.DisabledReason(),
                    context.playSession->RuntimeIsolationSnapshotActive()
                        ? "Snapshot isolation is active."
                        : "Snapshot isolation is pending.");
            } else if (context.playSession->RuntimeIsolationRestored()) {
                ImGui::SetTooltip("Authoring state was restored from the last Play/Sim snapshot.");
            } else if (context.playSession->RuntimeIsolationPending()) {
                ImGui::SetTooltip("Runtime clone/isolation is not implemented yet; boundary state only.");
            }
        }
    } else {
        ImGui::TextUnformatted("Session unavailable");
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    if (context.dirtyState != nullptr) {
        ImGui::Text(
            "Dirty %u",
            static_cast<unsigned int>(context.dirtyState->Count()));
        if (ImGui::IsItemHovered()) {
            const std::string summary = context.dirtyState->Summary();
            ImGui::SetTooltip("%s", summary.c_str());
        }
    } else {
        ImGui::TextUnformatted("Dirty unavailable");
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    if (context.saveApplyPolicy != nullptr) {
        const std::string policySummary =
            BuildEditorSaveApplyPolicySummary(*context.saveApplyPolicy);
        ImGui::TextUnformatted("Policy");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", policySummary.c_str());
        }
    } else {
        ImGui::TextUnformatted("Policy unavailable");
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    if (context.documentLifecycle != nullptr) {
        ImGui::Text(
            "Doc %s",
            ToString(context.documentLifecycle->LastAction()));
        if (ImGui::IsItemHovered()) {
            const std::string& message = context.documentLifecycle->LastMessage();
            ImGui::SetTooltip(
                "%s",
                message.empty() ? "Document lifecycle idle." : message.c_str());
        }
    } else {
        ImGui::TextUnformatted("Doc unavailable");
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    if (context.confirmService != nullptr) {
        const EditorModalConfirmRequest* pending = context.confirmService->Pending();
        if (pending != nullptr) {
            ImGui::Text("Confirm %s", ToString(pending->severity));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "%s\n%s",
                    pending->title.c_str(),
                    pending->message.c_str());
            }
        } else {
            ImGui::TextUnformatted("Confirm idle");
        }
    } else {
        ImGui::TextUnformatted("Confirm unavailable");
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    if (context.notifications != nullptr) {
        const EditorNotification* latest = context.notifications->Latest();
        if (latest != nullptr) {
            ImGui::Text(
                "Notify %s #%llu",
                ToString(latest->severity),
                static_cast<unsigned long long>(latest->id));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "%s\n%s",
                    latest->source.c_str(),
                    latest->message.c_str());
            }
        } else {
            ImGui::TextUnformatted("Notify none");
        }
    } else {
        ImGui::TextUnformatted("Notify unavailable");
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    const EditorCommandExecutionStatus* status =
        context.commands != nullptr ? context.commands->ExecutionStatus() : nullptr;
    if (status != nullptr && status->hasResult) {
        ImGui::Text(
            "Command %s %s",
            status->commandId.c_str(),
            status->succeeded ? "ok" : "failed");
        if (!status->message.empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", status->message.c_str());
        }
    } else {
        ImGui::TextUnformatted("Command idle");
    }

    ImGui::End();
}

} // namespace editor
