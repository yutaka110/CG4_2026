#include "EditorVfxGraphPanel.h"

#include "EditorVfxGraph.h"
#include "../EditorAssetSelection.h"
#include "../EditorNotificationCenter.h"
#include "../documents/EditorDocumentManager.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <iterator>
#include <string>

#include "../../../externals/imgui/imgui.h"

namespace editor {
namespace {

struct PinChoice {
    std::string nodeId;
    std::string pinId;
    std::string label;
};

void NotifyError(EditorNotificationCenter* notifications, const std::string& message) {
    if (notifications != nullptr) {
        notifications->Push(EditorNotificationSeverity::Error, "Advanced VFX Graph", message);
    }
}

std::vector<PinChoice> BuildPinChoices(const EditorVfxGraphAsset& asset,
    const EditorGraphSchema& schema, EditorGraphPinDirection direction) {
    std::vector<PinChoice> choices;
    for (const EditorGraphNode& node : asset.graph.nodes) {
        const EditorGraphNodeTypeDefinition* type = schema.FindNodeType(node.typeId);
        if (type == nullptr) continue;
        for (const EditorGraphPinDefinition& pin : type->pins) {
            if (pin.direction != direction) continue;
            choices.push_back({node.id, pin.id,
                node.label + " / " + pin.label + " [" + pin.typeId + "]"});
        }
    }
    return choices;
}

void DrawCanvas(const EditorVfxGraphAsset& asset, const EditorGraphSchema& schema,
    std::string& selectedNodeId) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 size((std::max)(available.x, 360.0f), (std::max)(available.y, 240.0f));
    ImGui::InvisibleButton("##vfxGraphCanvas", size);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
        IM_COL32(15, 22, 27, 255));
    const ImVec2 offset(origin.x + 30.0f, origin.y + 30.0f);
    const auto pinPosition = [&](const EditorGraphNode& node, std::string_view pinId, bool output) {
        const EditorGraphNodeTypeDefinition* type = schema.FindNodeType(node.typeId);
        int index = 0;
        if (type != nullptr) {
            for (const EditorGraphPinDefinition& pin : type->pins) {
                if ((pin.direction == EditorGraphPinDirection::Output) == output) {
                    if (pin.id == pinId) break;
                    ++index;
                }
            }
        }
        return ImVec2(offset.x + node.positionX + (output ? 180.0f : 0.0f),
            offset.y + node.positionY + 40.0f + 18.0f * static_cast<float>(index));
    };
    for (const EditorGraphLink& link : asset.graph.links) {
        const EditorGraphNode* from = FindEditorGraphNode(asset.graph, link.fromNodeId);
        const EditorGraphNode* to = FindEditorGraphNode(asset.graph, link.toNodeId);
        if (from == nullptr || to == nullptr) continue;
        const ImVec2 a = pinPosition(*from, link.fromPinId, true);
        const ImVec2 b = pinPosition(*to, link.toPinId, false);
        draw->AddBezierCubic(a, ImVec2(a.x + 65.0f, a.y), ImVec2(b.x - 65.0f, b.y), b,
            IM_COL32(85, 220, 155, 235), 2.0f);
    }
    for (const EditorGraphNode& node : asset.graph.nodes) {
        const ImVec2 minimum(offset.x + node.positionX, offset.y + node.positionY);
        const ImVec2 maximum(minimum.x + 180.0f, minimum.y + 104.0f);
        const bool selected = selectedNodeId == node.id;
        draw->AddRectFilled(minimum, maximum,
            selected ? IM_COL32(31, 92, 75, 255) : IM_COL32(37, 48, 52, 255), 5.0f);
        draw->AddRect(minimum, maximum,
            selected ? IM_COL32(80, 235, 170, 255) : IM_COL32(76, 98, 102, 255), 5.0f, 0, 2.0f);
        draw->AddText(ImVec2(minimum.x + 8.0f, minimum.y + 8.0f), IM_COL32_WHITE, node.label.c_str());
        draw->AddText(ImVec2(minimum.x + 8.0f, minimum.y + 27.0f),
            IM_COL32(145, 172, 173, 255), node.typeId.c_str());
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            ImGui::IsMouseHoveringRect(minimum, maximum)) selectedNodeId = node.id;
    }
}

} // namespace

void DrawEditorVfxGraphPanel(const EditorVfxGraphPanelContext& context) {
    if (context.service == nullptr || context.documents == nullptr) {
        ImGui::TextDisabled("Advanced VFX Graph services are unavailable.");
        return;
    }
    EditorVfxGraphService& service = *context.service;
    if (service.ActiveAsset() == nullptr) {
        const EditorAssetHandle* selected = context.assetSelection != nullptr
            ? context.assetSelection->Primary() : nullptr;
        if (selected == nullptr || selected->kind != EditorAssetKind::VfxGraph) {
            ImGui::TextDisabled("Select a VFX Graph asset in the Content Browser.");
            return;
        }
        ImGui::Text("Selected: %s", selected->displayName.c_str());
        if (ImGui::Button("Open VFX Graph")) {
            const EditorDocumentOpenResult result = context.documents->Open(
                EditorDocumentTypes::VfxGraph, selected->sourcePath);
            if (result.succeeded) {
                context.documents->SetActive(result.id);
                service.SetActiveDocument(result.id);
            } else NotifyError(context.notifications, result.message);
        }
        return;
    }

    EditorVfxGraphAsset& asset = *service.ActiveAsset();
    const EditorVfxCompileArtifact& artifact = service.LastCompileArtifact();
    ImGui::Text("%s  %s  Capacity %u  Emitters %u", asset.name.c_str(),
        ToString(asset.simulationTarget), asset.maxParticles,
        static_cast<unsigned>(artifact.emitters.size()));
    ImGui::SameLine();
    ImGui::TextColored(artifact.succeeded ? ImVec4(0.45f, 0.9f, 0.55f, 1.0f)
        : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        artifact.succeeded ? "Compiled %016llx" : "Compile failed",
        static_cast<unsigned long long>(artifact.sourceFingerprint));
    ImGui::SameLine();
    if (ImGui::Button("Compile")) {
        std::string error;
        if (!service.Recompile(error)) NotifyError(context.notifications, error);
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply Preview")) {
        std::string error;
        if (!service.ApplyPreview(error)) NotifyError(context.notifications, error);
    }

    static std::string selectedNodeId;
    static int nodeTypeIndex = 0;
    ImGui::BeginDisabled(!context.canMutate);
    static std::string settingsDocument;
    static int simulationTarget = 1;
    static int maxParticles = 65536;
    static float fixedTimeStep = 1.0f / 60.0f;
    if (settingsDocument != service.ActiveDocument().Key()) {
        settingsDocument = service.ActiveDocument().Key();
        simulationTarget = asset.simulationTarget == EditorVfxSimulationTarget::CPU ? 0 : 1;
        maxParticles = static_cast<int>(asset.maxParticles);
        fixedTimeStep = asset.fixedTimeStep;
    }
    bool settingsChanged = false;
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::Combo("Simulation", &simulationTarget, "CPU\0GPU\0")) settingsChanged = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::InputInt("Max Particles", &maxParticles)) settingsChanged = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputFloat("Fixed Step", &fixedTimeStep, 0.0f, 0.0f, "%.6f")) settingsChanged = true;
    if (settingsChanged) {
        std::string error;
        if (!service.SetSimulationSettings(
                simulationTarget == 0 ? EditorVfxSimulationTarget::CPU : EditorVfxSimulationTarget::GPU,
                maxParticles > 0 ? static_cast<uint32_t>(maxParticles) : 0u,
                fixedTimeStep, error)) NotifyError(context.notifications, error);
        ImGui::EndDisabled();
        return;
    }
    if (const EditorGraphNode* selected = FindEditorGraphNode(asset.graph, selectedNodeId);
        selected != nullptr && !selected->properties.empty()) {
        static std::string propertyNodeId;
        static int propertyIndex = 0;
        if (propertyNodeId != selected->id) {
            propertyNodeId = selected->id;
            propertyIndex = 0;
        }
        propertyIndex = (std::clamp)(propertyIndex, 0,
            static_cast<int>(selected->properties.size() - 1));
        auto property = selected->properties.begin();
        std::advance(property, propertyIndex);
        ImGui::SetNextItemWidth(190.0f);
        if (ImGui::BeginCombo("Property", property->first.c_str())) {
            int index = 0;
            for (const auto& [key, value] : selected->properties) {
                (void)value;
                if (ImGui::Selectable(key.c_str(), index == propertyIndex)) propertyIndex = index;
                ++index;
            }
            ImGui::EndCombo();
            property = selected->properties.begin();
            std::advance(property, propertyIndex);
        }
        static std::string editedIdentity;
        static std::array<char, 256> propertyBuffer{};
        const std::string identity = selected->id + ":" + property->first;
        if (editedIdentity != identity) {
            editedIdentity = identity;
            std::snprintf(propertyBuffer.data(), propertyBuffer.size(), "%s", property->second.c_str());
        }
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::InputText(property->first.c_str(), propertyBuffer.data(), propertyBuffer.size(),
                ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string error;
            if (!service.SetNodeProperty(selected->id, property->first,
                    propertyBuffer.data(), error)) NotifyError(context.notifications, error);
            ImGui::EndDisabled();
            return;
        }
    }
    const auto& nodeTypes = service.Schema().NodeTypes();
    if (!nodeTypes.empty()) {
        nodeTypeIndex = (std::clamp)(nodeTypeIndex, 0, static_cast<int>(nodeTypes.size() - 1));
        ImGui::SetNextItemWidth(210.0f);
        if (ImGui::BeginCombo("##vfxNodeType", nodeTypes[static_cast<std::size_t>(nodeTypeIndex)].displayName.c_str())) {
            for (int index = 0; index < static_cast<int>(nodeTypes.size()); ++index) {
                if (ImGui::Selectable(nodeTypes[static_cast<std::size_t>(index)].displayName.c_str(),
                        index == nodeTypeIndex)) nodeTypeIndex = index;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Node")) {
            std::string error;
            if (!service.AddNode(nodeTypes[static_cast<std::size_t>(nodeTypeIndex)].typeId,
                    40.0f, 140.0f + 30.0f * static_cast<float>(asset.graph.nodes.size()),
                    &selectedNodeId, error)) NotifyError(context.notifications, error);
            ImGui::EndDisabled();
            return;
        }
    }
    if (!selectedNodeId.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Delete Selected")) {
            std::string error;
            if (service.RemoveNode(selectedNodeId, error)) selectedNodeId.clear();
            else NotifyError(context.notifications, error);
            ImGui::EndDisabled();
            return;
        }
    }
    const std::vector<PinChoice> outputs = BuildPinChoices(asset, service.Schema(), EditorGraphPinDirection::Output);
    const std::vector<PinChoice> inputs = BuildPinChoices(asset, service.Schema(), EditorGraphPinDirection::Input);
    static int outputIndex = 0;
    static int inputIndex = 0;
    if (!outputs.empty() && !inputs.empty()) {
        outputIndex = (std::clamp)(outputIndex, 0, static_cast<int>(outputs.size() - 1));
        inputIndex = (std::clamp)(inputIndex, 0, static_cast<int>(inputs.size() - 1));
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::BeginCombo("From", outputs[static_cast<std::size_t>(outputIndex)].label.c_str())) {
            for (int i = 0; i < static_cast<int>(outputs.size()); ++i) {
                if (ImGui::Selectable(outputs[static_cast<std::size_t>(i)].label.c_str(), i == outputIndex)) outputIndex = i;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::BeginCombo("To", inputs[static_cast<std::size_t>(inputIndex)].label.c_str())) {
            for (int i = 0; i < static_cast<int>(inputs.size()); ++i) {
                if (ImGui::Selectable(inputs[static_cast<std::size_t>(i)].label.c_str(), i == inputIndex)) inputIndex = i;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Connect")) {
            std::string error;
            const PinChoice& from = outputs[static_cast<std::size_t>(outputIndex)];
            const PinChoice& to = inputs[static_cast<std::size_t>(inputIndex)];
            if (!service.Connect(from.nodeId, from.pinId, to.nodeId, to.pinId, error)) {
                NotifyError(context.notifications, error);
            }
            ImGui::EndDisabled();
            return;
        }
    }
    ImGui::EndDisabled();

    if (ImGui::BeginTabBar("##vfxGraphTabs")) {
        if (ImGui::BeginTabItem("Graph")) {
            DrawCanvas(asset, service.Schema(), selectedNodeId);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Diagnostics")) {
            if (artifact.diagnostics.empty()) ImGui::TextDisabled("No compile diagnostics.");
            for (const EditorVfxCompileDiagnostic& diagnostic : artifact.diagnostics) {
                ImGui::TextWrapped("[%s] %s: %s", diagnostic.code.c_str(),
                    diagnostic.nodeId.empty() ? "Graph" : diagnostic.nodeId.c_str(),
                    diagnostic.message.c_str());
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Execution Program")) {
            ImGui::TextUnformatted(artifact.generatedProgram.c_str());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Simulation HLSL")) {
            ImGui::TextUnformatted(artifact.simulationHlsl.c_str());
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

} // namespace editor
