#include "EditorMaterialGraphPanel.h"

#include "EditorMaterialGraph.h"
#include "../EditorAssetSelection.h"
#include "../EditorNotificationCenter.h"
#include "../documents/EditorDocumentManager.h"

#include <algorithm>
#include <array>
#include <cstdio>
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
        notifications->Push(EditorNotificationSeverity::Error, "Material Graph", message);
    }
}

std::vector<PinChoice> BuildPinChoices(
    const EditorMaterialGraphAsset& asset,
    const EditorGraphSchema& schema,
    EditorGraphPinDirection direction) {
    std::vector<PinChoice> choices;
    for (const EditorGraphNode& node : asset.graph.nodes) {
        const EditorGraphNodeTypeDefinition* type = schema.FindNodeType(node.typeId);
        if (type == nullptr) continue;
        for (const EditorGraphPinDefinition& pin : type->pins) {
            if (pin.direction != direction) continue;
            choices.push_back({node.id, pin.id, node.label + " / " + pin.label + " [" + pin.typeId + "]"});
        }
    }
    return choices;
}

void DrawCanvas(
    const EditorMaterialGraphAsset& asset,
    const EditorGraphSchema& schema,
    std::string& selectedNodeId) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 size((std::max)(available.x, 320.0f), (std::max)(available.y, 220.0f));
    ImGui::InvisibleButton("##materialGraphCanvas", size);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(20, 24, 30, 255));
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
        return ImVec2(offset.x + node.positionX + (output ? 160.0f : 0.0f),
            offset.y + node.positionY + 38.0f + 18.0f * static_cast<float>(index));
    };
    for (const EditorGraphLink& link : asset.graph.links) {
        const EditorGraphNode* from = FindEditorGraphNode(asset.graph, link.fromNodeId);
        const EditorGraphNode* to = FindEditorGraphNode(asset.graph, link.toNodeId);
        if (from == nullptr || to == nullptr) continue;
        const ImVec2 a = pinPosition(*from, link.fromPinId, true);
        const ImVec2 b = pinPosition(*to, link.toPinId, false);
        draw->AddBezierCubic(a, ImVec2(a.x + 60.0f, a.y), ImVec2(b.x - 60.0f, b.y), b,
            IM_COL32(94, 190, 218, 230), 2.0f);
    }
    for (const EditorGraphNode& node : asset.graph.nodes) {
        const ImVec2 minimum(offset.x + node.positionX, offset.y + node.positionY);
        const ImVec2 maximum(minimum.x + 160.0f, minimum.y + 96.0f);
        const bool selected = selectedNodeId == node.id;
        draw->AddRectFilled(minimum, maximum, selected ? IM_COL32(42, 80, 116, 255) : IM_COL32(40, 45, 54, 255), 5.0f);
        draw->AddRect(minimum, maximum, selected ? IM_COL32(100, 200, 255, 255) : IM_COL32(80, 90, 105, 255), 5.0f, 0, 2.0f);
        draw->AddText(ImVec2(minimum.x + 8.0f, minimum.y + 8.0f), IM_COL32_WHITE, node.label.c_str());
        draw->AddText(ImVec2(minimum.x + 8.0f, minimum.y + 27.0f), IM_COL32(155, 165, 178, 255), node.typeId.c_str());
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(minimum, maximum)) {
            selectedNodeId = node.id;
        }
    }
}

} // namespace

void DrawEditorMaterialGraphPanel(const EditorMaterialGraphPanelContext& context) {
    if (context.service == nullptr || context.documents == nullptr) {
        ImGui::TextDisabled("Material Graph services are unavailable.");
        return;
    }
    EditorMaterialGraphService& service = *context.service;
    if (service.ActiveAsset() == nullptr) {
        const EditorAssetHandle* selected = context.assetSelection != nullptr
            ? context.assetSelection->Primary()
            : nullptr;
        if (selected == nullptr || selected->kind != EditorAssetKind::MaterialGraph) {
            ImGui::TextDisabled("Select a Material Graph asset in the Content Browser.");
            return;
        }
        ImGui::Text("Selected: %s", selected->displayName.c_str());
        if (ImGui::Button("Open Material Graph")) {
            const EditorDocumentOpenResult result = context.documents->Open(
                EditorDocumentTypes::MaterialGraph, selected->sourcePath);
            if (result.succeeded) {
                context.documents->SetActive(result.id);
                service.SetActiveDocument(result.id);
            } else {
                NotifyError(context.notifications, result.message);
            }
        }
        return;
    }

    EditorMaterialGraphAsset& asset = *service.ActiveAsset();
    const EditorMaterialCompileArtifact& artifact = service.LastCompileArtifact();
    ImGui::Text("%s  Nodes %u  Links %u", asset.name.c_str(),
        static_cast<unsigned>(asset.graph.nodes.size()), static_cast<unsigned>(asset.graph.links.size()));
    ImGui::SameLine();
    ImGui::TextColored(artifact.succeeded ? ImVec4(0.45f, 0.9f, 0.55f, 1.0f) : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        artifact.succeeded ? "Compiled %016llx" : "Compile failed",
        static_cast<unsigned long long>(artifact.sourceFingerprint));
    ImGui::SameLine();
    if (ImGui::Button("Compile")) {
        std::string error;
        if (!service.Recompile(error)) NotifyError(context.notifications, error);
    }

    static std::string selectedNodeId;
    static int nodeTypeIndex = 0;
    bool mutated = false;
    ImGui::BeginDisabled(!context.canMutate);
    if (const EditorGraphNode* selectedNode = FindEditorGraphNode(asset.graph, selectedNodeId)) {
        std::string propertyKey;
        if (selectedNode->typeId == "material.constant.scalar" ||
            selectedNode->typeId == "material.constant.vector3") {
            propertyKey = "value";
        } else if (selectedNode->typeId == "material.texture2d") {
            propertyKey = "assetGuid";
        }
        if (!propertyKey.empty()) {
            static std::string editedNodeId;
            static std::string editedPropertyKey;
            static std::array<char, 256> propertyBuffer{};
            if (editedNodeId != selectedNode->id || editedPropertyKey != propertyKey) {
                editedNodeId = selectedNode->id;
                editedPropertyKey = propertyKey;
                const auto found = selectedNode->properties.find(propertyKey);
                const std::string value = found == selectedNode->properties.end() ? std::string{} : found->second;
                std::snprintf(propertyBuffer.data(), propertyBuffer.size(), "%s", value.c_str());
            }
            ImGui::SetNextItemWidth(300.0f);
            if (ImGui::InputText(
                    propertyKey == "assetGuid" ? "Texture Asset GUID" : "Value",
                    propertyBuffer.data(), propertyBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::string error;
                if (service.SetNodeProperty(
                        selectedNode->id, propertyKey, propertyBuffer.data(), error)) {
                    mutated = true;
                } else {
                    NotifyError(context.notifications, error);
                }
            }
        }
    }
    const auto& nodeTypes = service.Schema().NodeTypes();
    if (!mutated && !nodeTypes.empty()) {
        nodeTypeIndex = (std::clamp)(nodeTypeIndex, 0, static_cast<int>(nodeTypes.size() - 1));
        ImGui::SetNextItemWidth(190.0f);
        if (ImGui::BeginCombo("##materialNodeType", nodeTypes[static_cast<std::size_t>(nodeTypeIndex)].displayName.c_str())) {
            for (int index = 0; index < static_cast<int>(nodeTypes.size()); ++index) {
                if (ImGui::Selectable(nodeTypes[static_cast<std::size_t>(index)].displayName.c_str(), index == nodeTypeIndex)) nodeTypeIndex = index;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Node")) {
            std::string error;
            if (service.AddNode(nodeTypes[static_cast<std::size_t>(nodeTypeIndex)].typeId,
                    40.0f, 140.0f + 36.0f * static_cast<float>(asset.graph.nodes.size()),
                    &selectedNodeId, error)) mutated = true;
            else NotifyError(context.notifications, error);
        }
    }
    if (!mutated && !selectedNodeId.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Delete Selected")) {
            std::string error;
            if (service.RemoveNode(selectedNodeId, error)) {
                selectedNodeId.clear();
                mutated = true;
            }
            else NotifyError(context.notifications, error);
        }
    }

    std::vector<PinChoice> outputs = !mutated
        ? BuildPinChoices(asset, service.Schema(), EditorGraphPinDirection::Output)
        : std::vector<PinChoice>{};
    std::vector<PinChoice> inputs = !mutated
        ? BuildPinChoices(asset, service.Schema(), EditorGraphPinDirection::Input)
        : std::vector<PinChoice>{};
    static int outputIndex = 0;
    static int inputIndex = 0;
    if (!outputs.empty() && !inputs.empty()) {
        outputIndex = (std::clamp)(outputIndex, 0, static_cast<int>(outputs.size() - 1));
        inputIndex = (std::clamp)(inputIndex, 0, static_cast<int>(inputs.size() - 1));
        ImGui::SetNextItemWidth(250.0f);
        if (ImGui::BeginCombo("From", outputs[static_cast<std::size_t>(outputIndex)].label.c_str())) {
            for (int index = 0; index < static_cast<int>(outputs.size()); ++index) {
                if (ImGui::Selectable(outputs[static_cast<std::size_t>(index)].label.c_str(), index == outputIndex)) outputIndex = index;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250.0f);
        if (ImGui::BeginCombo("To", inputs[static_cast<std::size_t>(inputIndex)].label.c_str())) {
            for (int index = 0; index < static_cast<int>(inputs.size()); ++index) {
                if (ImGui::Selectable(inputs[static_cast<std::size_t>(index)].label.c_str(), index == inputIndex)) inputIndex = index;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Connect")) {
            const PinChoice& from = outputs[static_cast<std::size_t>(outputIndex)];
            const PinChoice& to = inputs[static_cast<std::size_t>(inputIndex)];
            std::string error;
            if (service.Connect(from.nodeId, from.pinId, to.nodeId, to.pinId, error)) mutated = true;
            else NotifyError(context.notifications, error);
        }
    }
    ImGui::EndDisabled();
    if (mutated) return;

    if (ImGui::BeginTabBar("##materialGraphTabs")) {
        if (ImGui::BeginTabItem("Graph")) {
            DrawCanvas(asset, service.Schema(), selectedNodeId);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Diagnostics")) {
            if (artifact.diagnostics.empty()) ImGui::TextDisabled("No compile diagnostics.");
            for (const EditorMaterialCompileDiagnostic& diagnostic : artifact.diagnostics) {
                ImGui::TextWrapped("[%s] %s: %s", diagnostic.code.c_str(),
                    diagnostic.nodeId.empty() ? "Graph" : diagnostic.nodeId.c_str(), diagnostic.message.c_str());
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Generated HLSL")) {
            ImGui::BeginChild("##materialHlsl", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(artifact.hlslSource.c_str());
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

} // namespace editor
