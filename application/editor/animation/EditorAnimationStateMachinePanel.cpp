#include "EditorAnimationStateMachinePanel.h"

#include "EditorAnimationStateMachine.h"
#include "../EditorAssetSelection.h"
#include "../EditorNotificationCenter.h"
#include "../documents/EditorDocumentManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <unordered_map>

#include "../../../externals/imgui/imgui.h"

namespace editor {
namespace {

struct PinChoice { std::string node; std::string pin; std::string label; };

void Notify(EditorNotificationCenter* center, const std::string& message) {
    if (center != nullptr) center->Push(EditorNotificationSeverity::Error,
        "Animation State Machine", message);
}

std::vector<PinChoice> Pins(const EditorAnimationStateMachineAsset& asset,
    const EditorGraphSchema& schema, EditorGraphPinDirection direction) {
    std::vector<PinChoice> result;
    for (const EditorGraphNode& node : asset.graph.nodes) {
        const auto* type = schema.FindNodeType(node.typeId);
        if (type == nullptr) continue;
        for (const auto& pin : type->pins) if (pin.direction == direction) {
            result.push_back({node.id, pin.id, node.label + " / " + pin.label});
        }
    }
    return result;
}

void Canvas(const EditorAnimationStateMachineAsset& asset,
    const EditorAnimationStateMachineArtifact& artifact, std::string& selected) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 size((std::max)(available.x, 360.0f), (std::max)(available.y, 240.0f));
    ImGui::InvisibleButton("##animationStateMachineCanvas", size);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(22, 19, 29, 255));
    const ImVec2 offset(origin.x + 35.0f, origin.y + 35.0f);
    const auto center = [&](const EditorGraphNode& node) {
        return ImVec2(offset.x + node.positionX + 85.0f, offset.y + node.positionY + 42.0f);
    };
    for (const EditorGraphLink& link : asset.graph.links) {
        const auto* from = FindEditorGraphNode(asset.graph, link.fromNodeId);
        const auto* to = FindEditorGraphNode(asset.graph, link.toNodeId);
        if (from == nullptr || to == nullptr) continue;
        const ImVec2 a = center(*from);
        const ImVec2 b = center(*to);
        draw->AddBezierCubic(a, ImVec2(a.x + 55.0f, a.y), ImVec2(b.x - 55.0f, b.y), b,
            IM_COL32(196, 132, 255, 230), 2.0f);
        const ImVec2 direction(b.x - a.x, b.y - a.y);
        const float length = (std::max)(1.0f, std::sqrt(direction.x * direction.x + direction.y * direction.y));
        const ImVec2 tip(b.x - direction.x / length * 18.0f, b.y - direction.y / length * 18.0f);
        draw->AddCircleFilled(tip, 3.5f, IM_COL32(225, 190, 255, 255));
    }
    for (const EditorGraphNode& node : asset.graph.nodes) {
        const ImVec2 minimum(offset.x + node.positionX, offset.y + node.positionY);
        const ImVec2 maximum(minimum.x + 170.0f, minimum.y + 84.0f);
        const bool active = artifact.succeeded && artifact.program.entryState < artifact.program.states.size() &&
            artifact.program.states[artifact.program.entryState].id == node.id;
        const bool chosen = selected == node.id;
        draw->AddRectFilled(minimum, maximum, active ? IM_COL32(54, 84, 48, 255) :
            (chosen ? IM_COL32(79, 49, 104, 255) : IM_COL32(49, 42, 58, 255)), 8.0f);
        draw->AddRect(minimum, maximum, chosen ? IM_COL32(210, 150, 255, 255) :
            IM_COL32(110, 91, 125, 255), 8.0f, 0, 2.0f);
        draw->AddText(ImVec2(minimum.x + 9.0f, minimum.y + 9.0f), IM_COL32_WHITE, node.label.c_str());
        draw->AddText(ImVec2(minimum.x + 9.0f, minimum.y + 30.0f),
            IM_COL32(175, 158, 190, 255), node.typeId.c_str());
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            ImGui::IsMouseHoveringRect(minimum, maximum)) selected = node.id;
    }
}

} // namespace

void DrawEditorAnimationStateMachinePanel(
    const EditorAnimationStateMachinePanelContext& context) {
    if (context.service == nullptr || context.documents == nullptr) return;
    auto& service = *context.service;
    if (service.ActiveAsset() == nullptr) {
        const EditorAssetHandle* selected = context.assetSelection != nullptr
            ? context.assetSelection->Primary() : nullptr;
        if (selected == nullptr || selected->kind != EditorAssetKind::AnimationStateMachine) {
            ImGui::TextDisabled("Select an Animation State Machine Asset.");
            return;
        }
        if (ImGui::Button("Open Animation State Machine")) {
            const auto result = context.documents->Open(
                EditorDocumentTypes::AnimationStateMachine, selected->sourcePath);
            if (result.succeeded) {
                context.documents->SetActive(result.id);
                service.SetActiveDocument(result.id);
            } else Notify(context.notifications, result.message);
        }
        return;
    }

    EditorAnimationStateMachineAsset& asset = *service.ActiveAsset();
    const auto& artifact = service.LastCompileArtifact();
    ImGui::Text("%s  States %u  Transitions %u  Parameters %u", asset.name.c_str(),
        static_cast<unsigned>(artifact.program.states.size()),
        static_cast<unsigned>(artifact.program.transitions.size()),
        static_cast<unsigned>(asset.parameters.size()));
    ImGui::SameLine();
    ImGui::TextColored(artifact.succeeded ? ImVec4(0.45f, 0.9f, 0.55f, 1.0f)
        : ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
        artifact.succeeded ? "Compiled %016llx" : "Compile failed",
        static_cast<unsigned long long>(artifact.sourceFingerprint));
    ImGui::SameLine();
    if (ImGui::Button("Compile / Reset Preview")) {
        std::string error;
        if (!service.ResetPreview(error)) Notify(context.notifications, error);
    }
    ImGui::SameLine();
    if (ImGui::Button("Step 1/60")) {
        std::string error;
        if (!service.StepPreview(1.0f / 60.0f, error)) Notify(context.notifications, error);
    }
    const auto& sample = service.PreviewSample();
    if (sample.valid && !service.LastSuccessfulArtifact().program.states.empty()) {
        const auto& program = service.LastSuccessfulArtifact().program;
        const std::string& current = program.states[sample.currentState].name;
        const std::string& next = program.states[sample.nextState].name;
        ImGui::Text("Preview: %s -> %s  time %.3f  blend %.2f",
            current.c_str(), next.c_str(), sample.currentTime, sample.blendAlpha);
    }

    static std::string selectedNode;
    static int nodeType = 0;
    ImGui::BeginDisabled(!context.canMutate);
    if (const EditorGraphNode* selected = FindEditorGraphNode(asset.graph, selectedNode);
        selected != nullptr && !selected->properties.empty()) {
        static std::string propertyNode;
        static int propertyIndex = 0;
        if (propertyNode != selected->id) { propertyNode = selected->id; propertyIndex = 0; }
        propertyIndex = (std::clamp)(propertyIndex, 0, static_cast<int>(selected->properties.size() - 1));
        auto property = selected->properties.begin();
        std::advance(property, propertyIndex);
        ImGui::SetNextItemWidth(180.0f);
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
        static std::string identity;
        static std::array<char, 256> buffer{};
        const std::string nextIdentity = selected->id + ":" + property->first;
        if (identity != nextIdentity) {
            identity = nextIdentity;
            std::snprintf(buffer.data(), buffer.size(), "%s", property->second.c_str());
        }
        ImGui::SetNextItemWidth(280.0f);
        if (ImGui::InputText("Value", buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string error;
            if (!service.SetNodeProperty(selected->id, property->first, buffer.data(), error))
                Notify(context.notifications, error);
            ImGui::EndDisabled();
            return;
        }
    }
    const auto& types = service.Schema().NodeTypes();
    nodeType = (std::clamp)(nodeType, 0, static_cast<int>(types.size() - 1));
    ImGui::SetNextItemWidth(190.0f);
    if (ImGui::BeginCombo("##animationNodeType", types[static_cast<std::size_t>(nodeType)].displayName.c_str())) {
        for (int index = 0; index < static_cast<int>(types.size()); ++index) {
            if (ImGui::Selectable(types[static_cast<std::size_t>(index)].displayName.c_str(), index == nodeType)) nodeType = index;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Node")) {
        std::string error;
        if (!service.AddNode(types[static_cast<std::size_t>(nodeType)].typeId, 60.0f,
                150.0f + static_cast<float>(asset.graph.nodes.size()) * 30.0f,
                &selectedNode, error)) Notify(context.notifications, error);
        ImGui::EndDisabled();
        return;
    }
    if (!selectedNode.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Delete Selected")) {
            std::string error;
            if (service.RemoveNode(selectedNode, error)) selectedNode.clear();
            else Notify(context.notifications, error);
            ImGui::EndDisabled();
            return;
        }
    }
    const auto outputs = Pins(asset, service.Schema(), EditorGraphPinDirection::Output);
    const auto inputs = Pins(asset, service.Schema(), EditorGraphPinDirection::Input);
    static int outputIndex = 0;
    static int inputIndex = 0;
    if (!outputs.empty() && !inputs.empty()) {
        outputIndex = (std::clamp)(outputIndex, 0, static_cast<int>(outputs.size() - 1));
        inputIndex = (std::clamp)(inputIndex, 0, static_cast<int>(inputs.size() - 1));
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("From", outputs[static_cast<std::size_t>(outputIndex)].label.c_str())) {
            for (int i = 0; i < static_cast<int>(outputs.size()); ++i)
                if (ImGui::Selectable(outputs[static_cast<std::size_t>(i)].label.c_str(), i == outputIndex)) outputIndex = i;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("To", inputs[static_cast<std::size_t>(inputIndex)].label.c_str())) {
            for (int i = 0; i < static_cast<int>(inputs.size()); ++i)
                if (ImGui::Selectable(inputs[static_cast<std::size_t>(i)].label.c_str(), i == inputIndex)) inputIndex = i;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Connect")) {
            std::string error;
            const auto& from = outputs[static_cast<std::size_t>(outputIndex)];
            const auto& to = inputs[static_cast<std::size_t>(inputIndex)];
            if (!service.Connect(from.node, from.pin, to.node, to.pin, error)) Notify(context.notifications, error);
            ImGui::EndDisabled();
            return;
        }
    }
    static std::array<char, 64> parameterName{};
    static int parameterType = 1;
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputText("New Parameter", parameterName.data(), parameterName.size());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("Type", &parameterType, "Bool\0Float\0Int\0Trigger\0");
    ImGui::SameLine();
    if (ImGui::Button("Add Parameter")) {
        std::string error;
        if (!service.AddParameter(parameterName.data(), static_cast<AnimationParameterType>(parameterType), 0.0f, error))
            Notify(context.notifications, error);
        parameterName.fill(0);
        ImGui::EndDisabled();
        return;
    }
    static int existingParameter = 0;
    if (!asset.parameters.empty()) {
        existingParameter = (std::clamp)(existingParameter, 0,
            static_cast<int>(asset.parameters.size() - 1));
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("Existing Parameter",
                asset.parameters[static_cast<std::size_t>(existingParameter)].name.c_str())) {
            for (int index = 0; index < static_cast<int>(asset.parameters.size()); ++index) {
                if (ImGui::Selectable(asset.parameters[static_cast<std::size_t>(index)].name.c_str(),
                        index == existingParameter)) existingParameter = index;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Parameter")) {
            std::string error;
            if (!service.RemoveParameter(
                    asset.parameters[static_cast<std::size_t>(existingParameter)].name, error))
                Notify(context.notifications, error);
            ImGui::EndDisabled();
            return;
        }
    }
    ImGui::EndDisabled();

    if (ImGui::BeginTabBar("##animationStateMachineTabs")) {
        if (ImGui::BeginTabItem("Graph")) { Canvas(asset, artifact, selectedNode); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Parameters")) {
            static std::unordered_map<std::string, float> previewValues;
            static std::unordered_map<std::string, bool> previewBools;
            for (const auto& parameter : asset.parameters) {
                ImGui::Text("%s [%s] default %.3f", parameter.name.c_str(), ToString(parameter.type), parameter.defaultValue);
                ImGui::SameLine();
                if (parameter.type == AnimationParameterType::Bool) {
                    bool& value = previewBools[parameter.name];
                    if (ImGui::Checkbox(("##bool_" + parameter.name).c_str(), &value))
                        service.SetPreviewBool(parameter.name, value);
                } else if (parameter.type == AnimationParameterType::Float) {
                    float& value = previewValues[parameter.name];
                    ImGui::SetNextItemWidth(120.0f);
                    if (ImGui::InputFloat(("##float_" + parameter.name).c_str(), &value))
                        service.SetPreviewFloat(parameter.name, value);
                } else if (parameter.type == AnimationParameterType::Int) {
                    float& stored = previewValues[parameter.name];
                    int value = static_cast<int>(stored);
                    ImGui::SetNextItemWidth(120.0f);
                    if (ImGui::InputInt(("##int_" + parameter.name).c_str(), &value)) {
                        stored = static_cast<float>(value);
                        service.SetPreviewInt(parameter.name, value);
                    }
                } else if (ImGui::SmallButton(("Fire##" + parameter.name).c_str())) {
                    service.FirePreviewTrigger(parameter.name);
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Diagnostics")) {
            for (const auto& issue : artifact.diagnostics)
                ImGui::TextWrapped("[%s] %s", issue.code.c_str(), issue.message.c_str());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Generated Program")) {
            ImGui::TextUnformatted(artifact.generatedProgram.c_str());
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

} // namespace editor
