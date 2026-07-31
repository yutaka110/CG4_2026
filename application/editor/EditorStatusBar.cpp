#include "EditorStatusBar.h"

#include "EditorAssetRegistry.h"
#include "EditorAssetSelection.h"
#include "EditorAssetThumbnailService.h"
#include "EditorAuthoringMutationGuard.h"
#include "EditorCommandRegistry.h"
#include "EditorContentBrowserState.h"
#include "EditorContext.h"
#include "EditorDirtyStateService.h"
#include "EditorLayoutService.h"
#include "EditorNotificationCenter.h"
#include "EditorPlaySessionState.h"
#include "EditorValidation.h"
#include "documents/EditorDocumentManager.h"
#include "tools/EditorToolManager.h"

#include "../../externals/imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace editor {
namespace {

std::string CountLabel(const char* label, std::size_t count) {
    return std::string(label) + " " + std::to_string(count);
}

std::string ValidationLabel(uint32_t errors, uint32_t warnings) {
    return "E:" + std::to_string(errors) + " W:" + std::to_string(warnings);
}

const EditorAssetRecord* SelectedAssetRecord(const EditorContext& context) {
    if (context.assets == nullptr || context.assetSelection == nullptr) return nullptr;
    const EditorAssetHandle* handle = context.assetSelection->Primary();
    if (handle == nullptr) return nullptr;
    const EditorAssetHandleResolveResult resolved =
        ResolveEditorAssetHandle(*context.assets, *handle);
    return resolved.record;
}

void DrawStatusItem(const char* id, const std::string& label, const char* tooltip = nullptr) {
    ImGui::PushID(id);
    ImGui::TextUnformatted(label.c_str());
    if (tooltip != nullptr && tooltip[0] != '\0' && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    ImGui::PopID();
}

struct StatusItem {
    const char* id = nullptr;
    std::string label;
    std::string detail;
    int priority = 0;
};

void DrawStatusPopup(const EditorStatusBarSnapshot& snapshot, const EditorContext& context) {
    ImGui::TextDisabled("Project Health");
    ImGui::Separator();
    ImGui::Text("Validation       %s", ValidationLabel(snapshot.errorCount, snapshot.warningCount).c_str());
    ImGui::Text("Dirty Documents  %u", static_cast<unsigned int>(snapshot.dirtyDocumentCount));
    ImGui::Text("Autosave Pending %u", static_cast<unsigned int>(snapshot.autosavePendingCount));
    ImGui::Text("Background Tasks %u", static_cast<unsigned int>(snapshot.backgroundTaskCount));
    ImGui::Text("Asset Import     %s", snapshot.assetImport.c_str());
    ImGui::Text("Shader Compile   %s", snapshot.shaderCompile.c_str());
    ImGui::Separator();
    ImGui::TextDisabled("Workspace");
    ImGui::Text("Source Control   %s", snapshot.sourceControl.c_str());
    ImGui::Text("Cook             %s", snapshot.cook.c_str());
    ImGui::Text("GPU              %s", snapshot.gpu.c_str());
    ImGui::Text("Memory           %s", snapshot.memory.c_str());
    ImGui::Separator();
    ImGui::TextDisabled("Editor");
    ImGui::Text("Document         %s", snapshot.activeDocument.c_str());
    ImGui::Text("Session          %s", snapshot.session.c_str());
    ImGui::Text("Selection        %u", static_cast<unsigned int>(snapshot.selectedObjectCount));
    ImGui::Text("Command          %s", snapshot.command.c_str());
    ImGui::Text("Interactive Tool %s", snapshot.interactiveTool.c_str());
    if (context.notifications != nullptr) {
        if (const EditorNotification* latest = context.notifications->Latest()) {
            ImGui::Separator();
            ImGui::TextWrapped("Latest: [%s] %s", latest->source.c_str(), latest->message.c_str());
        }
    }
}

} // namespace

EditorStatusBarSnapshot BuildEditorStatusBarSnapshot(const EditorContext& context) {
    EditorStatusBarSnapshot value;
    if (context.validationReport != nullptr) {
        value.errorCount = context.validationReport->errorCount;
        value.warningCount = context.validationReport->warningCount;
    }
    value.selectedObjectCount = context.selection != nullptr ? context.selection->Count() : 0;

    if (context.documentManager != nullptr) {
        value.dirtyDocumentCount = context.documentManager->DirtyCount();
        for (const EditorDocumentRecord* document : context.documentManager->OpenDocuments()) {
            if (document != nullptr && document->dirty &&
                document->autosaveRevision < document->editRevision) {
                ++value.autosavePendingCount;
            }
        }
        if (const EditorDocumentRecord* active = context.documentManager->Active()) {
            value.activeDocument = active->displayName.empty() ? active->id.type : active->displayName;
            if (active->dirty) value.activeDocument += "*";
            if (active->conflict != EditorDocumentConflictState::None) {
                value.activeDocument += " [Conflict]";
            }
        }
    } else if (context.dirtyState != nullptr) {
        value.dirtyDocumentCount = context.dirtyState->Count();
    }

    if (context.playSession != nullptr) {
        value.session = ToString(context.playSession->Mode());
        if (context.playSession->Mode() != EditorPlaySessionMode::Stopped) {
            value.session += " f:" + std::to_string(context.playSession->FrameCount());
            value.session += context.playSession->RuntimePaused()
                ? " [Frozen]"
                : " [Live]";
            value.session += context.playSession->ViewportEjected()
                ? " [Free Camera]"
                : " [Game Camera]";
        }
    }

    if (context.assetThumbnails != nullptr) {
        const EditorAssetPreviewJobQueue& previews = context.assetThumbnails->PreviewJobs();
        value.backgroundTaskCount =
            previews.Count(EditorAssetPreviewJobStatus::Queued) +
            previews.Count(EditorAssetPreviewJobStatus::Running) +
            context.assetThumbnails->GpuThumbnails().Count(EditorAssetGpuThumbnailStatus::Queued) +
            context.assetThumbnails->GpuThumbnails().Count(EditorAssetGpuThumbnailStatus::Rendering);
        value.gpu = "Ready " + std::to_string(
            context.assetThumbnails->GpuThumbnails().Count(EditorAssetGpuThumbnailStatus::Ready));
        const std::size_t failed =
            context.assetThumbnails->GpuThumbnails().Count(EditorAssetGpuThumbnailStatus::Failed);
        if (failed > 0) value.gpu += " Failed " + std::to_string(failed);
    }

    if (const EditorAssetRecord* selected = SelectedAssetRecord(context)) {
        if (context.assetWorkspaceStatus != nullptr) {
            const EditorAssetWorkspaceStatus status =
                context.assetWorkspaceStatus->QueryStatus(*selected);
            value.sourceControl = ToString(status.sourceControl);
            value.cook = ToString(status.cook);
        }
    } else if (context.assetWorkspaceStatus != nullptr) {
        value.sourceControl = "No Selection";
        value.cook = "No Selection";
    }

    if (context.commands != nullptr) {
        if (const EditorCommandExecutionStatus* status = context.commands->ExecutionStatus();
            status != nullptr && status->hasResult) {
            value.command = status->commandId + (status->succeeded ? " OK" : " Failed");
        }
    }
    if (context.interactiveTools != nullptr) {
        const EditorToolManagerSnapshot tool = context.interactiveTools->Snapshot();
        value.interactiveTool = tool.modeLabel.empty() ? "No Mode" : tool.modeLabel;
        if (!tool.toolLabel.empty()) value.interactiveTool += " / " + tool.toolLabel;
        value.interactiveTool += " [" + std::string(ToString(tool.state)) + "]";
    }
    return value;
}

float EditorStatusBarHeight() {
    return 26.0f;
}

void DrawEditorStatusBar(EditorContext& context) {
    if (!context.developerToolsVisible) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
    const ImVec2 workSize = viewport ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    const float statusHeight =
        context.layout != nullptr ? context.layout->StatusBarHeight() : EditorStatusBarHeight();
    if (statusHeight <= 0.0f) return;

    ImGui::SetNextWindowPos(
        ImVec2(workPos.x, workPos.y + workSize.y - statusHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(workSize.x, statusHeight), ImGuiCond_Always);
    if (!ImGui::Begin("Editor Status Bar", nullptr, flags)) {
        ImGui::End();
        return;
    }

    const EditorStatusBarSnapshot snapshot = BuildEditorStatusBarSnapshot(context);
    const std::vector<StatusItem> items{
        {"validation", ValidationLabel(snapshot.errorCount, snapshot.warningCount),
            "Validation error and warning totals.", 100},
        {"dirty", CountLabel("Dirty", snapshot.dirtyDocumentCount),
            "Documents with unsaved changes.", 90},
        {"autosave", CountLabel("Autosave", snapshot.autosavePendingCount),
            "Dirty document revisions not covered by autosave.", 80},
        {"tasks", CountLabel("Tasks", snapshot.backgroundTaskCount),
            "Queued/running preview and GPU thumbnail tasks.", 70},
        {"session", snapshot.session, "Current Play/Simulate session.", 60},
        {"tool", snapshot.interactiveTool, "Active editor mode and interactive tool.", 55},
        {"document", snapshot.activeDocument, "Active editor document.", 50},
        {"command", snapshot.command, "Latest command execution result.", 40},
    };

    bool drew = false;
    std::size_t firstHidden = items.size();
    float remainingWidth = ImGui::GetContentRegionAvail().x;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const StatusItem& item = items[i];
        const float width = ImGui::CalcTextSize(item.label.c_str()).x +
            (drew ? ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize("|").x : 0.0f);
        if (remainingWidth < width + 64.0f) {
            firstHidden = i;
            break;
        }
        if (drew) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
        }
        DrawStatusItem(item.id, item.label, item.detail.c_str());
        drew = true;
        remainingWidth = (std::max)(0.0f, remainingWidth - width);
    }

    if (drew) ImGui::SameLine();
    if (ImGui::SmallButton(firstHidden < items.size() ? "Status..." : "Details")) {
        ImGui::OpenPopup("EditorStatusDetails");
    }
    if (ImGui::BeginPopup("EditorStatusDetails")) {
        DrawStatusPopup(snapshot, context);
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace editor
