#include "EditorProductionNavigationAuthoringPanel.h"

#include "EditorProductionNavigationAuthoringPipeline.h"
#include "../EditorAssetSelection.h"
#include "../EditorNotificationCenter.h"
#include "../documents/EditorDocumentManager.h"
#include "../../../externals/imgui/imgui.h"

#include <cstdio>

namespace editor {
namespace {
void Notify(EditorNotificationCenter* notifications, std::string message) {
    if (notifications != nullptr && !message.empty())
        notifications->Push(EditorNotificationSeverity::Error,
            "Navigation Authoring", std::move(message));
}

void DrawCompileStatus(const EditorProductionNavigationAuthoringPipeline& pipeline) {
    const auto& result = pipeline.CompileResult();
    ImGui::TextColored(result.succeeded ? ImVec4(0.35f, 0.9f, 0.45f, 1.0f)
                                             : ImVec4(1.0f, 0.4f, 0.35f, 1.0f),
        result.succeeded ? "Compiled / Runtime Published" : "Not compiled");
    ImGui::SameLine();
    ImGui::TextDisabled("fingerprint %llu",
        static_cast<unsigned long long>(result.program.sourceFingerprint));
    for (const auto& diagnostic : result.diagnostics)
        ImGui::BulletText("[%s] %s", diagnostic.code.c_str(), diagnostic.message.c_str());
}
} // namespace

void DrawEditorProductionNavigationAuthoringPanel(
    const EditorProductionNavigationAuthoringPanelContext& context) {
    if (context.pipeline == nullptr || context.documents == nullptr) return;
    auto& pipeline = *context.pipeline;
    if (pipeline.ActiveAsset() == nullptr) {
        const EditorAssetHandle* selected = context.assetSelection != nullptr
            ? context.assetSelection->Primary() : nullptr;
        if (selected != nullptr && selected->kind == EditorAssetKind::NavigationData) {
            if (ImGui::Button("Open Navigation Data")) {
                const auto result = context.documents->Open(
                    EditorDocumentTypes::NavigationData, selected->sourcePath);
                if (result.succeeded) {
                    context.documents->SetActive(result.id);
                    pipeline.SetActiveDocument(result.id);
                } else Notify(context.notifications, result.message);
            }
        } else ImGui::TextDisabled("Select a Navigation Data Asset (.navdata).");
        return;
    }

    DrawCompileStatus(pipeline);
    const auto& stats = pipeline.Stats();
    ImGui::Text("Mutations %u | Runtime publishes %u | Overlay %u",
        stats.mutations, stats.runtimePublishes, stats.overlayCommands);
    ImGui::Separator();

    auto* asset = pipeline.ActiveAsset();
    if (ImGui::CollapsingHeader("Area Costs", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& source : asset->areas) {
            auto area = source;
            ImGui::PushID(area.id.c_str());
            ImGui::TextUnformatted(area.id.c_str());
            ImGui::SameLine(150.0f);
            ImGui::SetNextItemWidth(110.0f);
            ImGui::DragFloat("Cost", &area.cost, 0.05f, 0.01f, 1000.0f, "%.2f");
            const bool costEdited = ImGui::IsItemDeactivatedAfterEdit();
            ImGui::SameLine();
            ImGui::ColorEdit3("Color", &area.debugColor.x,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            const bool colorEdited = ImGui::IsItemDeactivatedAfterEdit();
            ImGui::SameLine();
            const bool enabledEdited = ImGui::Checkbox("Enabled", &area.enabled);
            if (context.canMutate && (costEdited || colorEdited || enabledEdited)) {
                std::string error;
                if (!pipeline.UpdateArea(std::move(area), error)) Notify(context.notifications, error);
            }
            if (context.canMutate && source.id != "Default") {
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) {
                    std::string error;
                    if (!pipeline.RemoveArea(source.id, error)) Notify(context.notifications, error);
                }
            }
            ImGui::PopID();
        }
        static char newArea[64] = "Area";
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##newArea", newArea, sizeof(newArea));
        ImGui::SameLine();
        if (context.canMutate && ImGui::Button("Add Area")) {
            std::string error;
            if (!pipeline.AddArea({newArea, 1.0f, {0.25f, 0.75f, 1.0f}, true}, error))
                Notify(context.notifications, error);
        }
    }

    if (ImGui::CollapsingHeader("Agent Profiles", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& source : asset->agentProfiles) {
            auto profile = source;
            ImGui::PushID(profile.id.c_str());
            if (ImGui::TreeNode(profile.id.c_str())) {
                bool edited = false;
                ImGui::DragFloat("Radius", &profile.radius, 0.02f, 0.01f, 20.0f);
                edited |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::DragFloat("Height", &profile.height, 0.02f, 0.01f, 50.0f);
                edited |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::DragFloat("Max Step", &profile.maximumStepHeight, 0.02f, 0.0f, 20.0f);
                edited |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::DragFloat("Max Slope", &profile.maximumSlopeDegrees, 0.25f, 0.0f, 89.0f);
                edited |= ImGui::IsItemDeactivatedAfterEdit();
                if (context.canMutate && edited) {
                    std::string error;
                    if (!pipeline.UpdateAgentProfile(std::move(profile), error)) Notify(context.notifications, error);
                }
                if (context.canMutate && source.id != "Default" && ImGui::Button("Remove Profile")) {
                    std::string error;
                    if (!pipeline.RemoveAgentProfile(source.id, error)) Notify(context.notifications, error);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        static char newProfile[64] = "Agent";
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##newProfile", newProfile, sizeof(newProfile));
        ImGui::SameLine();
        if (context.canMutate && ImGui::Button("Add Profile")) {
            std::string error;
            if (!pipeline.AddAgentProfile({newProfile}, error)) Notify(context.notifications, error);
        }
    }

    if (ImGui::CollapsingHeader("Off-Mesh Links", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& source : asset->offMeshLinks) {
            auto link = source;
            ImGui::PushID(link.id.c_str());
            if (ImGui::TreeNode(link.id.c_str())) {
                bool edited = false;
                ImGui::DragFloat3("Start", &link.start.x, 0.1f); edited |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::DragFloat3("End", &link.end.x, 0.1f); edited |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::DragFloat("Radius", &link.radius, 0.05f, 0.01f, 100.0f); edited |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::DragFloat("Cost Multiplier", &link.costMultiplier, 0.05f, 0.01f, 1000.0f); edited |= ImGui::IsItemDeactivatedAfterEdit();
                edited |= ImGui::Checkbox("Bidirectional", &link.bidirectional);
                edited |= ImGui::Checkbox("Enabled", &link.enabled);
                if (ImGui::BeginCombo("Area", link.areaId.c_str())) {
                    for (const auto& area : asset->areas) {
                        const bool selected = link.areaId == area.id;
                        if (ImGui::Selectable(area.id.c_str(), selected)) {
                            link.areaId = area.id;
                            edited = true;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::BeginCombo("Agent Profile", link.agentProfileId.c_str())) {
                    for (const auto& profile : asset->agentProfiles) {
                        const bool selected = link.agentProfileId == profile.id;
                        if (ImGui::Selectable(profile.id.c_str(), selected)) {
                            link.agentProfileId = profile.id;
                            edited = true;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if (context.canMutate && edited) {
                    std::string error;
                    if (!pipeline.UpdateOffMeshLink(std::move(link), error)) Notify(context.notifications, error);
                }
                if (context.canMutate && ImGui::Button("Remove Link")) {
                    std::string error;
                    if (!pipeline.RemoveOffMeshLink(source.id, error)) Notify(context.notifications, error);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        static char newLink[64] = "Link";
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##newLink", newLink, sizeof(newLink));
        ImGui::SameLine();
        if (context.canMutate && ImGui::Button("Add Link")) {
            std::string error;
            EditorNavigationOffMeshLink link;
            link.id = newLink;
            link.end = {4.0f, 0.0f, 0.0f};
            if (!pipeline.AddOffMeshLink(std::move(link), error)) Notify(context.notifications, error);
        }
    }
}

} // namespace editor
