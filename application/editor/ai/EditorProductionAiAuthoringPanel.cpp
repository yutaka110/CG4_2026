#include "EditorProductionAiAuthoringPanel.h"

#include "EditorProductionAiAuthoringPipeline.h"
#include "../EditorAssetSelection.h"
#include "../EditorNotificationCenter.h"
#include "../documents/EditorDocumentManager.h"

#include "../../../externals/imgui/imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace editor {
namespace {
void Notify(EditorNotificationCenter* center, const std::string& message,
    EditorNotificationSeverity severity = EditorNotificationSeverity::Error) {
    if (center != nullptr) center->Push(severity, "Production AI", message);
}

std::string NextNodeId(const EditorBehaviorTreeAsset& asset) {
    for (uint32_t index = 1; index < kEditorBehaviorTreeMaximumNodes; ++index) {
        const std::string id = "node-" + std::to_string(index);
        if (std::none_of(asset.nodes.begin(), asset.nodes.end(),
                [&](const auto& node) { return node.id == id; })) return id;
    }
    return {};
}

std::string NextTestId(const EditorEqsAsset& asset) {
    for (uint32_t index = 1; index < kEditorEqsMaximumTests; ++index) {
        const std::string id = "test-" + std::to_string(index);
        if (std::none_of(asset.tests.begin(), asset.tests.end(),
                [&](const auto& test) { return test.id == id; })) return id;
    }
    return {};
}

const char* BlackboardValueText(const EditorBlackboardValue& value) {
    static thread_local std::array<char, 256> text{};
    switch (value.type) {
    case EditorBlackboardValueType::Bool:
        std::snprintf(text.data(), text.size(), "%s", value.boolValue ? "true" : "false"); break;
    case EditorBlackboardValueType::Int:
        std::snprintf(text.data(), text.size(), "%lld", static_cast<long long>(value.intValue)); break;
    case EditorBlackboardValueType::Float:
        std::snprintf(text.data(), text.size(), "%.3f", value.floatValue); break;
    case EditorBlackboardValueType::Vector3:
        std::snprintf(text.data(), text.size(), "(%.2f, %.2f, %.2f)",
            value.vectorValue.x, value.vectorValue.y, value.vectorValue.z); break;
    case EditorBlackboardValueType::Entity:
    case EditorBlackboardValueType::String:
        std::snprintf(text.data(), text.size(), "%s", value.textValue.c_str()); break;
    }
    return text.data();
}

void DrawBehaviorCanvas(const EditorBehaviorTreeAsset& asset, std::string_view selectedId) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 size((std::max)(available.x, 520.0f), 280.0f);
    ImGui::InvisibleButton("##behaviorTreeCanvas", size);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(18, 23, 28, 255));
    struct NodeBox { const EditorBehaviorTreeNode* node; ImVec2 minimum; ImVec2 maximum; };
    std::vector<NodeBox> boxes;
    boxes.reserve(asset.nodes.size());
    const auto depthOf = [&](const EditorBehaviorTreeNode& input) {
        uint32_t depth = 0;
        std::string parent = input.parentId;
        std::unordered_set<std::string> visited;
        while (!parent.empty() && visited.insert(parent).second) {
            const auto found = std::find_if(asset.nodes.begin(), asset.nodes.end(),
                [&](const auto& value) { return value.id == parent; });
            if (found == asset.nodes.end()) break;
            ++depth; parent = found->parentId;
        }
        return depth;
    };
    std::vector<uint32_t> rowCounts(16, 0);
    for (const auto& node : asset.nodes) {
        const uint32_t depth = (std::min)(depthOf(node), 15u);
        const uint32_t row = rowCounts[depth]++;
        const float x = origin.x + 18.0f + depth * 172.0f;
        const float y = origin.y + 18.0f + row * 62.0f;
        boxes.push_back({&node, {x, y}, {x + 148.0f, y + 46.0f}});
    }
    for (const auto& box : boxes) {
        if (box.node->parentId.empty()) continue;
        const auto parent = std::find_if(boxes.begin(), boxes.end(), [&](const auto& value) {
            return value.node->id == box.node->parentId;
        });
        if (parent == boxes.end()) continue;
        const ImVec2 a{parent->maximum.x, (parent->minimum.y + parent->maximum.y) * .5f};
        const ImVec2 b{box.minimum.x, (box.minimum.y + box.maximum.y) * .5f};
        draw->AddBezierCubic(a, {a.x + 38, a.y}, {b.x - 38, b.y}, b,
            IM_COL32(105, 158, 190, 220), 2.0f);
    }
    for (const auto& box : boxes) {
        const bool selected = box.node->id == selectedId;
        draw->AddRectFilled(box.minimum, box.maximum,
            selected ? IM_COL32(38, 91, 112, 255) : IM_COL32(43, 51, 60, 255), 6.0f);
        draw->AddRect(box.minimum, box.maximum,
            selected ? IM_COL32(79, 213, 255, 255) : IM_COL32(105, 128, 142, 255), 6.0f, 0, 2.0f);
        draw->AddText({box.minimum.x + 8, box.minimum.y + 6}, IM_COL32_WHITE, box.node->id.c_str());
        draw->AddText({box.minimum.x + 8, box.minimum.y + 24}, IM_COL32(160, 190, 205, 255),
            ToString(box.node->type));
    }
}

void DrawEqsCanvas(const EditorEqsAsset& asset, std::string_view selectedTest) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 size((std::max)(available.x, 520.0f), 220.0f);
    ImGui::InvisibleButton("##eqsCanvas", size);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(18, 23, 28, 255));
    const ImVec2 generatorMin{origin.x + 22, origin.y + 82};
    const ImVec2 generatorMax{generatorMin.x + 155, generatorMin.y + 55};
    draw->AddRectFilled(generatorMin, generatorMax, IM_COL32(75, 57, 105, 255), 7.0f);
    draw->AddRect(generatorMin, generatorMax, IM_COL32(173, 137, 225, 255), 7.0f, 0, 2.0f);
    draw->AddText({generatorMin.x + 9, generatorMin.y + 8}, IM_COL32_WHITE, "Generator");
    draw->AddText({generatorMin.x + 9, generatorMin.y + 28}, IM_COL32(218, 195, 245, 255),
        ToString(asset.generator));
    for (std::size_t index = 0; index < asset.tests.size(); ++index) {
        const uint32_t column = static_cast<uint32_t>(index / 3);
        const uint32_t row = static_cast<uint32_t>(index % 3);
        const ImVec2 minimum{origin.x + 235 + column * 174.0f, origin.y + 16 + row * 66.0f};
        const ImVec2 maximum{minimum.x + 150, minimum.y + 48};
        const bool selected = asset.tests[index].id == selectedTest;
        draw->AddBezierCubic({generatorMax.x, (generatorMin.y + generatorMax.y) * .5f},
            {generatorMax.x + 35, generatorMin.y}, {minimum.x - 35, minimum.y + 24},
            {minimum.x, minimum.y + 24}, IM_COL32(111, 165, 193, 210), 1.5f);
        draw->AddRectFilled(minimum, maximum,
            selected ? IM_COL32(39, 92, 111, 255) : IM_COL32(42, 55, 61, 255), 6.0f);
        draw->AddRect(minimum, maximum,
            selected ? IM_COL32(77, 213, 255, 255) : IM_COL32(99, 130, 143, 255), 6.0f, 0, 2.0f);
        draw->AddText({minimum.x + 8, minimum.y + 6}, IM_COL32_WHITE, asset.tests[index].id.c_str());
        draw->AddText({minimum.x + 8, minimum.y + 24}, IM_COL32(163, 195, 207, 255),
            ToString(asset.tests[index].type));
    }
}

void DrawBehaviorAuthoring(EditorProductionAiAuthoringPipeline& pipeline,
    EditorNotificationCenter* notifications, bool canMutate) {
    EditorBehaviorTreeAsset* asset = pipeline.ActiveBehaviorTree();
    if (asset == nullptr) { ImGui::TextDisabled("Open a Behavior Tree document."); return; }
    const auto& compile = pipeline.BehaviorCompileResult();
    ImGui::Text("%s  Nodes %u  Blackboard %u", asset->name.c_str(),
        static_cast<unsigned>(asset->nodes.size()), static_cast<unsigned>(asset->blackboard.size()));
    ImGui::SameLine();
    ImGui::TextColored(compile.succeeded ? ImVec4(.35f,.9f,.5f,1) : ImVec4(1,.4f,.25f,1),
        compile.succeeded ? "Compiled %016llx" : "Compile failed",
        static_cast<unsigned long long>(compile.program.sourceFingerprint));

    static int selectedNode = 0;
    selectedNode = asset->nodes.empty() ? 0 : (std::clamp)(selectedNode, 0,
        static_cast<int>(asset->nodes.size() - 1));
    if (!asset->nodes.empty()) {
        ImGui::SetNextItemWidth(210.0f);
        if (ImGui::BeginCombo("Node", asset->nodes[selectedNode].id.c_str())) {
            for (int index = 0; index < static_cast<int>(asset->nodes.size()); ++index)
                if (ImGui::Selectable(asset->nodes[index].id.c_str(), index == selectedNode)) selectedNode = index;
            ImGui::EndCombo();
        }
        EditorBehaviorTreeNode node = asset->nodes[selectedNode];
        bool breakpoint = pipeline.HasBreakpoint(node.id);
        ImGui::SameLine();
        if (ImGui::Checkbox("Breakpoint", &breakpoint))
            pipeline.SetBreakpoint({node.id, {}}, breakpoint);
        ImGui::Text("Type %s  Parent %s  Order %u", ToString(node.type),
            node.parentId.empty() ? "<none>" : node.parentId.c_str(), node.order);
        ImGui::Text("Blackboard %s  Operation %s  Value %s  Wait %.3fs",
            node.blackboardKey.c_str(), node.operation.c_str(), node.value.c_str(), node.durationSeconds);
        static std::string editIdentity;
        static std::array<char, 64> operation{};
        static std::array<char, 128> value{};
        if (editIdentity != asset->assetGuid + ":" + node.id) {
            editIdentity = asset->assetGuid + ":" + node.id;
            std::snprintf(operation.data(), operation.size(), "%s", node.operation.c_str());
            std::snprintf(value.data(), value.size(), "%s", node.value.c_str());
        }
        int editType = static_cast<int>(node.type);
        int editOrder = static_cast<int>(node.order);
        float editDuration = node.durationSeconds;
        int editParent = 0;
        for (int index = 0; index < static_cast<int>(asset->nodes.size()); ++index)
            if (asset->nodes[index].id == node.parentId) editParent = index;
        ImGui::BeginDisabled(!canMutate || node.type == EditorBehaviorNodeType::Root);
        ImGui::SetNextItemWidth(150); ImGui::Combo("Edit Type", &editType,
            "Root\0Selector\0Sequence\0Condition\0Set Blackboard\0Wait\0Move To\0Succeed\0Fail\0");
        ImGui::SetNextItemWidth(180);
        if (ImGui::BeginCombo("Edit Parent", asset->nodes[editParent].id.c_str())) {
            for (int index = 0; index < static_cast<int>(asset->nodes.size()); ++index) {
                if (asset->nodes[index].id == node.id) continue;
                if (ImGui::Selectable(asset->nodes[index].id.c_str(), index == editParent)) editParent = index;
            }
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(100); ImGui::InputInt("Order", &editOrder);
        if (!asset->blackboard.empty()) {
            int keyIndex = 0;
            for (int index = 0; index < static_cast<int>(asset->blackboard.size()); ++index)
                if (asset->blackboard[index].name == node.blackboardKey) keyIndex = index;
            ImGui::SetNextItemWidth(180);
            if (ImGui::BeginCombo("Blackboard Key", asset->blackboard[keyIndex].name.c_str())) {
                for (int index = 0; index < static_cast<int>(asset->blackboard.size()); ++index)
                    if (ImGui::Selectable(asset->blackboard[index].name.c_str(), index == keyIndex)) keyIndex = index;
                ImGui::EndCombo();
            }
            node.blackboardKey = asset->blackboard[keyIndex].name;
        }
        ImGui::SetNextItemWidth(150); ImGui::InputText("Operation", operation.data(), operation.size());
        ImGui::SameLine(); ImGui::SetNextItemWidth(180); ImGui::InputText("Value", value.data(), value.size());
        ImGui::SetNextItemWidth(120); ImGui::InputFloat("Wait Seconds", &editDuration);
        if (ImGui::Button("Apply Node Settings")) {
            node.type = static_cast<EditorBehaviorNodeType>(editType);
            node.parentId = asset->nodes[editParent].id;
            node.order = static_cast<uint32_t>((std::max)(0, editOrder));
            node.operation = operation.data();
            node.value = value.data();
            node.durationSeconds = editDuration;
            std::string error;
            if (!pipeline.UpdateBehaviorNode(std::move(node), error)) Notify(notifications, error);
            ImGui::EndDisabled(); return;
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!canMutate || node.type == EditorBehaviorNodeType::Root);
        if (ImGui::Button("Remove Selected Node")) {
            std::string error;
            if (!pipeline.RemoveBehaviorNode(node.id, error)) Notify(notifications, error);
            ImGui::EndDisabled(); return;
        }
        ImGui::EndDisabled();
        DrawBehaviorCanvas(*asset, node.id);
    }

    static int nodeType = static_cast<int>(EditorBehaviorNodeType::Succeed);
    static int parentIndex = 0;
    const char* nodeTypes = "Root\0Selector\0Sequence\0Condition\0Set Blackboard\0Wait\0Move To\0Succeed\0Fail\0";
    ImGui::BeginDisabled(!canMutate || asset->nodes.empty());
    ImGui::SetNextItemWidth(150); ImGui::Combo("New Type", &nodeType, nodeTypes);
    parentIndex = (std::clamp)(parentIndex, 0, static_cast<int>(asset->nodes.size() - 1));
    ImGui::SetNextItemWidth(180);
    if (ImGui::BeginCombo("Parent", asset->nodes[parentIndex].id.c_str())) {
        for (int index = 0; index < static_cast<int>(asset->nodes.size()); ++index)
            if (ImGui::Selectable(asset->nodes[index].id.c_str(), index == parentIndex)) parentIndex = index;
        ImGui::EndCombo();
    }
    if (ImGui::Button("Add Behavior Node")) {
        EditorBehaviorTreeNode node;
        node.id = NextNodeId(*asset);
        node.type = static_cast<EditorBehaviorNodeType>(nodeType);
        node.parentId = asset->nodes[parentIndex].id;
        node.order = static_cast<uint32_t>(asset->nodes.size());
        if (node.type == EditorBehaviorNodeType::Wait) node.durationSeconds = 0.1f;
        if (node.type == EditorBehaviorNodeType::Condition ||
            node.type == EditorBehaviorNodeType::SetBlackboard ||
            node.type == EditorBehaviorNodeType::MoveTo) {
            if (!asset->blackboard.empty()) node.blackboardKey = asset->blackboard.front().name;
            if (node.type == EditorBehaviorNodeType::Condition) node.operation = "equals";
        }
        std::string error;
        if (!pipeline.AddBehaviorNode(std::move(node), error)) Notify(notifications, error);
        ImGui::EndDisabled(); return;
    }
    ImGui::EndDisabled();

    if (ImGui::TreeNodeEx("Blackboard Schema", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& key : asset->blackboard)
            ImGui::BulletText("%s : %s = %s", key.name.c_str(), ToString(key.defaultValue.type),
                BlackboardValueText(key.defaultValue));
        static std::array<char, 64> name{};
        static int type = 0;
        ImGui::BeginDisabled(!canMutate);
        ImGui::SetNextItemWidth(150); ImGui::InputText("Key Name", name.data(), name.size());
        ImGui::SameLine(); ImGui::SetNextItemWidth(120);
        ImGui::Combo("Key Type", &type, "Bool\0Int\0Float\0Vector3\0Entity\0String\0");
        ImGui::SameLine();
        if (ImGui::Button("Add Key")) {
            EditorBlackboardKeyDefinition key;
            key.name = name.data();
            key.defaultValue.type = static_cast<EditorBlackboardValueType>(type);
            std::string error;
            if (!pipeline.AddBlackboardKey(std::move(key), error)) Notify(notifications, error);
            else name.fill(0);
            ImGui::EndDisabled(); ImGui::TreePop(); return;
        }
        ImGui::EndDisabled();
        ImGui::TreePop();
    }
    for (const auto& issue : compile.diagnostics)
        ImGui::TextWrapped("[%s] %s: %s", issue.code.c_str(), issue.nodeId.c_str(), issue.message.c_str());
}

void DrawEqsAuthoring(EditorProductionAiAuthoringPipeline& pipeline,
    EditorNotificationCenter* notifications, bool canMutate) {
    EditorEqsAsset* asset = pipeline.ActiveEqs();
    if (asset == nullptr) { ImGui::TextDisabled("Open an Environment Query document."); return; }
    const auto& compile = pipeline.EqsCompileResult();
    ImGui::Text("%s  Generator %s  Candidates %u  Tests %u", asset->name.c_str(),
        ToString(asset->generator), asset->candidateCount, static_cast<unsigned>(asset->tests.size()));
    ImGui::SameLine();
    ImGui::TextColored(compile.succeeded ? ImVec4(.35f,.9f,.5f,1) : ImVec4(1,.4f,.25f,1),
        compile.succeeded ? "Compiled %016llx" : "Compile failed",
        static_cast<unsigned long long>(compile.program.sourceFingerprint));
    int generator = static_cast<int>(asset->generator);
    float radius = asset->radius;
    float spacing = asset->spacing;
    int candidates = static_cast<int>(asset->candidateCount);
    static std::array<char, 64> smartType{};
    static std::string smartIdentity;
    if (smartIdentity != asset->assetGuid) {
        smartIdentity = asset->assetGuid;
        std::snprintf(smartType.data(), smartType.size(), "%s", asset->smartObjectType.c_str());
    }
    ImGui::BeginDisabled(!canMutate);
    ImGui::SetNextItemWidth(130); ImGui::Combo("Generator", &generator, "Ring\0Grid\0Smart Objects\0");
    ImGui::SetNextItemWidth(110); ImGui::InputFloat("Radius", &radius);
    ImGui::SameLine(); ImGui::SetNextItemWidth(110); ImGui::InputFloat("Spacing", &spacing);
    ImGui::SameLine(); ImGui::SetNextItemWidth(110); ImGui::InputInt("Candidates", &candidates);
    if (generator == static_cast<int>(EditorEqsGeneratorType::SmartObjects))
        ImGui::InputText("Smart Object Type", smartType.data(), smartType.size());
    if (ImGui::Button("Apply Generator")) {
        std::string error;
        if (!pipeline.SetEqsGenerator(static_cast<EditorEqsGeneratorType>(generator), radius, spacing,
                static_cast<uint32_t>((std::max)(1, candidates)), smartType.data(), error)) Notify(notifications, error);
        ImGui::EndDisabled(); return;
    }
    ImGui::Separator();
    static int selectedTest = 0;
    selectedTest = (std::clamp)(selectedTest, 0, static_cast<int>(asset->tests.size() - 1));
    if (ImGui::BeginCombo("Test", asset->tests[selectedTest].id.c_str())) {
        for (int index = 0; index < static_cast<int>(asset->tests.size()); ++index)
            if (ImGui::Selectable(asset->tests[index].id.c_str(), index == selectedTest)) selectedTest = index;
        ImGui::EndCombo();
    }
    EditorEqsTestDefinition test = asset->tests[selectedTest];
    int testType = static_cast<int>(test.type);
    ImGui::SetNextItemWidth(160); ImGui::Combo("Test Type", &testType,
        "Distance\0Path Cost\0Visibility\0Crowding\0Smart Object Available\0");
    test.type = static_cast<EditorEqsTestType>(testType);
    ImGui::InputFloat("Weight", &test.weight); ImGui::SameLine();
    ImGui::InputFloat("Minimum", &test.minimum); ImGui::SameLine(); ImGui::InputFloat("Maximum", &test.maximum);
    ImGui::Checkbox("Filter", &test.filter); ImGui::SameLine(); ImGui::Checkbox("Prefer Higher", &test.preferHigher);
    if (ImGui::Button("Apply Test")) {
        std::string error; if (!pipeline.UpdateEqsTest(test, error)) Notify(notifications, error);
        ImGui::EndDisabled(); return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Test")) {
        test.id = NextTestId(*asset);
        std::string error; if (!pipeline.AddEqsTest(test, error)) Notify(notifications, error);
        ImGui::EndDisabled(); return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove Test")) {
        std::string error; if (!pipeline.RemoveEqsTest(test.id, error)) Notify(notifications, error);
        ImGui::EndDisabled(); return;
    }
    ImGui::EndDisabled();
    DrawEqsCanvas(*asset, test.id);
    for (const auto& issue : compile.diagnostics)
        ImGui::TextWrapped("[%s] %s: %s", issue.code.c_str(), issue.testId.c_str(), issue.message.c_str());
}

void DrawDebugger(EditorProductionAiAuthoringPipeline& pipeline,
    EditorNotificationCenter* notifications) {
    if (pipeline.Paused()) {
        if (ImGui::Button("Resume")) pipeline.Resume();
    } else if (ImGui::Button("Pause")) pipeline.Pause();
    ImGui::SameLine(); if (ImGui::Button("Step Live")) pipeline.RequestStep();
    ImGui::SameLine();
    if (!pipeline.Recording()) {
        if (ImGui::Button("Record")) pipeline.BeginRecording();
    } else if (ImGui::Button("Stop Record")) pipeline.StopRecording();
    ImGui::SameLine();
    if (!pipeline.Replaying()) {
        if (ImGui::Button("Replay")) { std::string error; if (!pipeline.BeginReplay(&error)) Notify(notifications, error); }
    } else {
        if (ImGui::Button("Live")) pipeline.EndReplay();
        ImGui::SameLine(); if (ImGui::Button("<")) pipeline.StepReplay(-1);
        ImGui::SameLine(); if (ImGui::Button(">")) pipeline.StepReplay(1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Export")) {
        std::string error;
        if (!pipeline.ExportRecording("logs/editor_ai_simulation.record", &error)) Notify(notifications, error);
        else Notify(notifications, "AI recording exported to logs/editor_ai_simulation.record.",
            EditorNotificationSeverity::Info);
    }
    ImGui::SameLine();
    if (ImGui::Button("Import")) {
        std::string error;
        if (!pipeline.ImportRecording("logs/editor_ai_simulation.record", &error)) Notify(notifications, error);
    }
    const auto& stats = pipeline.Stats();
    ImGui::Text("State %s%s  Frames %u/%u  Breakpoints %u  Hits %u  Overlay %u reject %u",
        pipeline.Paused() ? "Paused" : "Running", pipeline.Replaying() ? " / Replay" : "",
        stats.recordedFrames, pipeline.Policy().maximumRecordedFrames,
        static_cast<unsigned>(pipeline.Breakpoints().size()), stats.breakpointHits,
        stats.overlayCommands, stats.overlayBudgetRejected);
    if (!pipeline.LastBreakpointNode().empty())
        ImGui::TextColored(ImVec4(1,.65f,.25f,1), "Breakpoint %s on %s",
            pipeline.LastBreakpointNode().c_str(), pipeline.LastBreakpointAgent().c_str());
    const EditorAiSimulationFrame* frame = pipeline.DisplayFrame();
    if (frame == nullptr) { ImGui::TextDisabled("No AI runtime frame captured."); return; }
    ImGui::Text("Frame %llu  Behavior %llu  World %llu  Fingerprint %016llx",
        static_cast<unsigned long long>(frame->frameIndex),
        static_cast<unsigned long long>(frame->behaviorGeneration),
        static_cast<unsigned long long>(frame->worldGeneration),
        static_cast<unsigned long long>(frame->fingerprint));
    for (const auto& agent : frame->agents) {
        if (!ImGui::TreeNode(agent.entityGuid.c_str(), "%s  %s  nodes=%u perceived=%u",
                agent.entityGuid.c_str(), ToString(agent.status), agent.executedNodes,
                static_cast<unsigned>(agent.perceived.size()))) continue;
        ImGui::Text("Trace:");
        for (const auto& node : agent.activeNodeTrace) {
            const bool hit = pipeline.HasBreakpoint(node) || pipeline.HasBreakpoint(node, agent.entityGuid);
            ImGui::BulletText("%s%s", hit ? "[B] " : "", node.c_str());
        }
        if (ImGui::TreeNode("Live Blackboard")) {
            for (const auto& key : agent.blackboard)
                ImGui::Text("%s : %s = %s", key.name.c_str(), ToString(key.defaultValue.type),
                    BlackboardValueText(key.defaultValue));
            ImGui::TreePop();
        }
        ImGui::Text("Path points %u", static_cast<unsigned>(agent.lastPath.size()));
        ImGui::TreePop();
    }
}
} // namespace

void DrawEditorProductionAiAuthoringPanel(
    const EditorProductionAiAuthoringPanelContext& context) {
    if (context.pipeline == nullptr || context.documents == nullptr) return;
    auto& pipeline = *context.pipeline;
    if (pipeline.ActiveBehaviorTree() == nullptr && pipeline.ActiveEqs() == nullptr) {
        const EditorAssetHandle* selected = context.assetSelection != nullptr
            ? context.assetSelection->Primary() : nullptr;
        if (selected != nullptr && (selected->kind == EditorAssetKind::BehaviorTree ||
                selected->kind == EditorAssetKind::EnvironmentQuery)) {
            if (ImGui::Button(selected->kind == EditorAssetKind::BehaviorTree
                    ? "Open Behavior Tree" : "Open Environment Query")) {
                const std::string_view type = selected->kind == EditorAssetKind::BehaviorTree
                    ? EditorDocumentTypes::BehaviorTree : EditorDocumentTypes::EnvironmentQuery;
                const EditorDocumentOpenResult result = context.documents->Open(type, selected->sourcePath);
                if (result.succeeded) {
                    context.documents->SetActive(result.id);
                    pipeline.SetActiveDocument(result.id);
                } else Notify(context.notifications, result.message);
            }
        } else ImGui::TextDisabled("Select a Behavior Tree or Environment Query Asset.");
    }
    if (ImGui::BeginTabBar("##productionAiAuthoringTabs")) {
        if (ImGui::BeginTabItem("Behavior Tree")) {
            DrawBehaviorAuthoring(pipeline, context.notifications, context.canMutate);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Environment Query")) {
            DrawEqsAuthoring(pipeline, context.notifications, context.canMutate);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Debugger / Simulation")) {
            DrawDebugger(pipeline, context.notifications);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

} // namespace editor
