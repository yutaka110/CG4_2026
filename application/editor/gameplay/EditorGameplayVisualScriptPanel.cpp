#include "EditorGameplayVisualScriptPanel.h"

#include "EditorGameplayVisualScript.h"
#include "../EditorAssetSelection.h"
#include "../EditorNotificationCenter.h"
#include "../documents/EditorDocumentManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include "../../../externals/imgui/imgui.h"

namespace editor {
namespace {
struct PinChoice { std::string node; std::string pin; std::string label; };

void Notify(EditorNotificationCenter* center, const std::string& message) {
    if (center != nullptr) center->Push(EditorNotificationSeverity::Error,
        "Gameplay Visual Script", message);
}
std::vector<PinChoice> Pins(const EditorGameplayVisualScriptAsset& asset,
    const EditorGraphSchema& schema, EditorGraphPinDirection direction) {
    std::vector<PinChoice> result;
    for (const auto& node : asset.graph.nodes) {
        const auto* type = schema.FindNodeType(node.typeId); if (type == nullptr) continue;
        for (const auto& pin : type->pins) if (pin.direction == direction)
            result.push_back({node.id, pin.id, node.label + " / " + pin.label});
    }
    return result;
}
void Canvas(const EditorGameplayVisualScriptAsset& asset, std::string& selected) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 size((std::max)(available.x, 360.0f), (std::max)(available.y, 240.0f));
    ImGui::InvisibleButton("##gameplayVisualScriptCanvas", size);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(18, 24, 28, 255));
    const ImVec2 offset(origin.x + 35.0f, origin.y + 35.0f);
    auto center = [&](const EditorGraphNode& node) {
        return ImVec2(offset.x + node.positionX + 85.0f, offset.y + node.positionY + 38.0f);
    };
    for (const auto& link : asset.graph.links) {
        const auto* from = FindEditorGraphNode(asset.graph, link.fromNodeId);
        const auto* to = FindEditorGraphNode(asset.graph, link.toNodeId);
        if (from == nullptr || to == nullptr) continue;
        const ImVec2 a = center(*from), b = center(*to);
        const bool exec = link.fromPinId == "exec" || link.fromPinId == "then" ||
            link.fromPinId == "true" || link.fromPinId == "false";
        draw->AddBezierCubic(a, ImVec2(a.x + 55, a.y), ImVec2(b.x - 55, b.y), b,
            exec ? IM_COL32(240, 240, 240, 230) : IM_COL32(75, 190, 235, 230), 2.0f);
    }
    for (const auto& node : asset.graph.nodes) {
        const ImVec2 minimum(offset.x + node.positionX, offset.y + node.positionY);
        const ImVec2 maximum(minimum.x + 170.0f, minimum.y + 76.0f);
        const bool chosen = selected == node.id;
        const bool event = node.typeId.find("gameplay.event.") == 0;
        draw->AddRectFilled(minimum, maximum, event ? IM_COL32(92, 42, 44, 255) :
            (chosen ? IM_COL32(36, 83, 102, 255) : IM_COL32(38, 48, 54, 255)), 7.0f);
        draw->AddRect(minimum, maximum, chosen ? IM_COL32(80, 210, 255, 255) :
            IM_COL32(95, 115, 124, 255), 7.0f, 0, 2.0f);
        draw->AddText(ImVec2(minimum.x + 9, minimum.y + 9), IM_COL32_WHITE, node.label.c_str());
        draw->AddText(ImVec2(minimum.x + 9, minimum.y + 31), IM_COL32(155, 175, 184, 255), node.typeId.c_str());
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            ImGui::IsMouseHoveringRect(minimum, maximum)) selected = node.id;
    }
}
GameplayValue DefaultValueForType(int type) {
    switch (type) {
    case 0: return GameplayValue::Bool(false);
    case 2: return GameplayValue::Int(0);
    case 3: return GameplayValue::String({});
    default: return GameplayValue::Float(0.0f);
    }
}
} // namespace

void DrawEditorGameplayVisualScriptPanel(const EditorGameplayVisualScriptPanelContext& context) {
    if (context.service == nullptr || context.documents == nullptr) return;
    auto& service = *context.service;
    if (service.ActiveAsset() == nullptr) {
        const EditorAssetHandle* selected = context.assetSelection ? context.assetSelection->Primary() : nullptr;
        if (selected == nullptr || selected->kind != EditorAssetKind::GameplayVisualScript) {
            ImGui::TextDisabled("Select a Gameplay Visual Script Asset."); return;
        }
        if (ImGui::Button("Open Gameplay Visual Script")) {
            const auto result = context.documents->Open(EditorDocumentTypes::GameplayVisualScript, selected->sourcePath);
            if (result.succeeded) { context.documents->SetActive(result.id); service.SetActiveDocument(result.id); }
            else Notify(context.notifications, result.message);
        }
        return;
    }
    auto& asset = *service.ActiveAsset();
    const auto& artifact = service.LastCompileArtifact();
    ImGui::Text("%s  Nodes %u  Variables %u  Budget %u", asset.name.c_str(),
        static_cast<unsigned>(asset.graph.nodes.size()), static_cast<unsigned>(asset.variables.size()),
        asset.instructionBudget);
    ImGui::SameLine();
    ImGui::TextColored(artifact.succeeded ? ImVec4(.4f,.9f,.55f,1) : ImVec4(1,.5f,.3f,1),
        artifact.succeeded ? "Compiled %016llx" : "Compile failed",
        static_cast<unsigned long long>(artifact.sourceFingerprint));
    ImGui::SameLine();
    if (ImGui::Button("Compile / Reset")) { std::string error; if (!service.ResetPreview(error)) Notify(context.notifications, error); }
    ImGui::SameLine();
    if (ImGui::Button("Run BeginPlay")) { std::string error; if (!service.ExecutePreview("BeginPlay", 0, error)) Notify(context.notifications, error); }
    ImGui::SameLine();
    if (ImGui::Button("Run Tick")) { std::string error; if (!service.ExecutePreview("Tick", 1.0f/60.0f, error)) Notify(context.notifications, error); }
    ImGui::Text("Preview: %s, instructions %u", ToString(service.LastPreviewResult().status),
        service.LastPreviewResult().instructionsExecuted);

    static std::string selectedNode;
    ImGui::BeginDisabled(!context.canMutate);
    const EditorGraphNode* selected = FindEditorGraphNode(asset.graph, selectedNode);
    if (selected != nullptr && !selected->properties.empty()) {
        static std::string propertyNode; static int propertyIndex = 0;
        if (propertyNode != selected->id) { propertyNode = selected->id; propertyIndex = 0; }
        propertyIndex = (std::clamp)(propertyIndex, 0, static_cast<int>(selected->properties.size() - 1));
        auto property = selected->properties.begin(); std::advance(property, propertyIndex);
        ImGui::SetNextItemWidth(160);
        if (ImGui::BeginCombo("Property", property->first.c_str())) {
            int index = 0; for (const auto& [key, value] : selected->properties) {
                (void)value; if (ImGui::Selectable(key.c_str(), index == propertyIndex)) propertyIndex = index; ++index;
            }
            ImGui::EndCombo(); property = selected->properties.begin(); std::advance(property, propertyIndex);
        }
        static std::string identity; static std::array<char, 256> buffer{};
        const std::string nextIdentity = selected->id + ":" + property->first;
        if (identity != nextIdentity) { identity = nextIdentity; std::snprintf(buffer.data(), buffer.size(), "%s", property->second.c_str()); }
        ImGui::SetNextItemWidth(260);
        if (ImGui::InputText("Value", buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string error; if (!service.SetNodeProperty(selected->id, property->first, buffer.data(), error)) Notify(context.notifications, error);
            ImGui::EndDisabled(); return;
        }
    }
    const auto& types = service.Schema().NodeTypes(); static int nodeType = 0;
    nodeType = (std::clamp)(nodeType, 0, static_cast<int>(types.size() - 1));
    ImGui::SetNextItemWidth(190);
    if (ImGui::BeginCombo("##gameplayNodeType", types[nodeType].displayName.c_str())) {
        for (int i=0; i<static_cast<int>(types.size()); ++i)
            if (ImGui::Selectable(types[i].displayName.c_str(), i==nodeType)) nodeType=i;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Node")) {
        std::string error; if (!service.AddNode(types[nodeType].typeId, 80,
            130 + static_cast<float>(asset.graph.nodes.size()) * 28, &selectedNode, error)) Notify(context.notifications, error);
        ImGui::EndDisabled(); return;
    }
    if (!selectedNode.empty()) { ImGui::SameLine(); if (ImGui::Button("Delete Selected")) {
        std::string error; if (service.RemoveNode(selectedNode, error)) selectedNode.clear(); else Notify(context.notifications, error);
        ImGui::EndDisabled(); return;
    }}
    const auto outputs = Pins(asset, service.Schema(), EditorGraphPinDirection::Output);
    const auto inputs = Pins(asset, service.Schema(), EditorGraphPinDirection::Input);
    static int outputIndex=0, inputIndex=0;
    if (!outputs.empty() && !inputs.empty()) {
        outputIndex=(std::clamp)(outputIndex,0,static_cast<int>(outputs.size()-1));
        inputIndex=(std::clamp)(inputIndex,0,static_cast<int>(inputs.size()-1));
        ImGui::SetNextItemWidth(210); if (ImGui::BeginCombo("From", outputs[outputIndex].label.c_str())) {
            for(int i=0;i<static_cast<int>(outputs.size());++i) if(ImGui::Selectable(outputs[i].label.c_str(),i==outputIndex)) outputIndex=i; ImGui::EndCombo(); }
        ImGui::SameLine(); ImGui::SetNextItemWidth(210); if (ImGui::BeginCombo("To", inputs[inputIndex].label.c_str())) {
            for(int i=0;i<static_cast<int>(inputs.size());++i) if(ImGui::Selectable(inputs[i].label.c_str(),i==inputIndex)) inputIndex=i; ImGui::EndCombo(); }
        ImGui::SameLine(); if(ImGui::Button("Connect")) { std::string error; const auto& a=outputs[outputIndex]; const auto& b=inputs[inputIndex];
            if(!service.Connect(a.node,a.pin,b.node,b.pin,error)) Notify(context.notifications,error); ImGui::EndDisabled(); return; }
    }
    static std::array<char,64> variableName{}; static int variableType=1;
    ImGui::SetNextItemWidth(140); ImGui::InputText("New Variable",variableName.data(),variableName.size()); ImGui::SameLine();
    ImGui::SetNextItemWidth(90); ImGui::Combo("Type",&variableType,"Bool\0Float\0Int\0String\0"); ImGui::SameLine();
    if(ImGui::Button("Add Variable")){std::string error;if(!service.AddVariable(variableName.data(),DefaultValueForType(variableType),error))Notify(context.notifications,error);variableName.fill(0);ImGui::EndDisabled();return;}
    static int variableIndex=0;
    if(!asset.variables.empty()){variableIndex=(std::clamp)(variableIndex,0,static_cast<int>(asset.variables.size()-1));ImGui::SetNextItemWidth(160);
        if(ImGui::BeginCombo("Existing Variable",asset.variables[variableIndex].name.c_str())){for(int i=0;i<static_cast<int>(asset.variables.size());++i)if(ImGui::Selectable(asset.variables[i].name.c_str(),i==variableIndex))variableIndex=i;ImGui::EndCombo();}
        ImGui::SameLine();if(ImGui::Button("Remove Variable")){std::string error;if(!service.RemoveVariable(asset.variables[variableIndex].name,error))Notify(context.notifications,error);ImGui::EndDisabled();return;}}
    int budget=static_cast<int>(asset.instructionBudget); ImGui::SetNextItemWidth(140);
    if(ImGui::InputInt("Instruction Budget",&budget,0,0,ImGuiInputTextFlags_EnterReturnsTrue)){std::string error;if(!service.SetInstructionBudget(static_cast<uint32_t>((std::max)(budget,1)),error))Notify(context.notifications,error);ImGui::EndDisabled();return;}
    ImGui::EndDisabled();

    if(ImGui::BeginTabBar("##gameplayTabs")){
        if(ImGui::BeginTabItem("Graph")){Canvas(asset,selectedNode);ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Preview")){for(const auto& line:service.PreviewOutput())ImGui::TextUnformatted(line.c_str());
            for(const auto& node:service.PreviewTrace())ImGui::BulletText("%s",node.c_str());ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Diagnostics")){for(const auto& issue:artifact.diagnostics)ImGui::TextWrapped("[%s] %s",issue.code.c_str(),issue.message.c_str());ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Generated Program")){ImGui::TextUnformatted(artifact.generatedProgram.c_str());ImGui::EndTabItem();}
        ImGui::EndTabBar();
    }
}

} // namespace editor
