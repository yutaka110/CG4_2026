#include "EditorGameplayVisualScript.h"

#include "../EditorSelection.h"
#include "../EditorTransactionStack.h"
#include "../core/EditorExecutionContext.h"
#include "../core/EditorUndoCommand.h"
#include "../documents/EditorDocumentManager.h"
#include "../documents/EditorGameplayVisualScriptDocumentProvider.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace editor {
namespace {

constexpr uint32_t kInvalid = UINT32_MAX;
constexpr uint32_t kMaxInstructionBudget = 1'000'000;

EditorGraphPinDefinition Pin(std::string id, std::string label, std::string type,
    EditorGraphPinDirection direction, bool required = false, bool multiple = false) {
    return {std::move(id), std::move(label), std::move(type), direction, required, multiple};
}
EditorGraphPinDefinition ExecIn() {
    return Pin("exec", "Exec", "gameplay.exec", EditorGraphPinDirection::Input, true, true);
}
EditorGraphPinDefinition ExecOut(std::string id = "then", std::string label = "Then") {
    return Pin(std::move(id), std::move(label), "gameplay.exec", EditorGraphPinDirection::Output);
}
EditorGraphPinDefinition DataIn(std::string id, std::string label, std::string type) {
    return Pin(std::move(id), std::move(label), std::move(type), EditorGraphPinDirection::Input, true);
}
EditorGraphPinDefinition DataOut(std::string id, std::string label, std::string type) {
    return Pin(std::move(id), std::move(label), std::move(type), EditorGraphPinDirection::Output);
}

std::string Property(const EditorGraphNode& node, std::string_view key, std::string fallback = {}) {
    const auto found = node.properties.find(std::string(key));
    return found == node.properties.end() ? std::move(fallback) : found->second;
}
bool ParseFloat(std::string_view text, float& value) {
    std::istringstream input{std::string(text)}; std::string extra;
    return static_cast<bool>(input >> value) && std::isfinite(value) && !(input >> extra);
}
bool ParseInt(std::string_view text, int32_t& value) {
    std::istringstream input{std::string(text)}; std::string extra;
    return static_cast<bool>(input >> value) && !(input >> extra);
}
bool ParseBool(std::string_view text, bool& value) {
    if (text == "true" || text == "1") value = true;
    else if (text == "false" || text == "0") value = false;
    else return false;
    return true;
}
uint64_t HashText(std::string_view text) {
    uint64_t hash = 1469598103934665603ull;
    for (const unsigned char byte : text) { hash ^= byte; hash *= 1099511628211ull; }
    return hash;
}
void AddIssue(EditorGameplayVisualScriptArtifact& artifact, EditorGraphIssueSeverity severity,
    std::string code, std::string nodeId, std::string message) {
    artifact.diagnostics.push_back({severity, std::move(code), std::move(nodeId), std::move(message)});
}
bool HasErrors(const EditorGameplayVisualScriptArtifact& artifact) {
    return std::any_of(artifact.diagnostics.begin(), artifact.diagnostics.end(),
        [](const auto& issue) { return issue.severity == EditorGraphIssueSeverity::Error; });
}
bool IsEvent(std::string_view type) {
    return type == "gameplay.event.begin-play" || type == "gameplay.event.tick";
}
bool IsStatement(std::string_view type) {
    return type == "gameplay.variable.set-float" || type == "gameplay.variable.set-bool" ||
        type == "gameplay.flow.branch" || type == "gameplay.debug.print" ||
        type == "gameplay.emit-event" || type == "gameplay.flow.return";
}
bool IsExpression(std::string_view type) {
    return type == "gameplay.variable.get-float" || type == "gameplay.variable.get-bool" ||
        type == "gameplay.literal.float" || type == "gameplay.literal.bool" ||
        type == "gameplay.literal.int" || type == "gameplay.literal.string" ||
        type == "gameplay.math.add-float" || type == "gameplay.compare.greater-float" ||
        type == "gameplay.logic.not" || type == "gameplay.event.tick";
}

class GameplayVisualScriptUndoCommand final : public IEditorUndoCommand {
public:
    GameplayVisualScriptUndoCommand(EditorDocumentId document,
        EditorGameplayVisualScriptAsset before, EditorGameplayVisualScriptAsset after)
        : document_(std::move(document)), before_(std::move(before)), after_(std::move(after)) {}
    EditorUndoResult Apply(EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override {
        auto* service = dynamic_cast<EditorGameplayVisualScriptService*>(
            context.Find(EditorGameplayVisualScriptService::kServiceId));
        if (service == nullptr) return EditorUndoResult::Failure(EditorErrorCode::MissingService,
            "Gameplay Visual Script execution service is unavailable.");
        std::string error;
        if (!service->PublishFromCommand(document_,
                mode == EditorTransactionApplyMode::Undo ? before_ : after_, error)) {
            return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, std::move(error));
        }
        return EditorUndoResult::Success();
    }
    std::size_t EstimatedBytes() const noexcept override {
        return sizeof(*this) + (before_.graph.nodes.size() + after_.graph.nodes.size()) * sizeof(EditorGraphNode) +
            (before_.graph.links.size() + after_.graph.links.size()) * sizeof(EditorGraphLink);
    }
    std::string_view DomainId() const noexcept override { return "gameplay-visual-script"; }
    std::string_view TypeId() const noexcept override { return "gameplay-visual-script.snapshot"; }
private:
    EditorDocumentId document_;
    EditorGameplayVisualScriptAsset before_;
    EditorGameplayVisualScriptAsset after_;
};

struct CompilerState {
    const EditorGameplayVisualScriptAsset& asset;
    EditorGameplayVisualScriptArtifact& artifact;
    const std::unordered_map<std::string, uint32_t>& variables;
    std::unordered_map<std::string, uint32_t> expressions;
    std::unordered_set<std::string> visiting;

    uint32_t CompileExpression(std::string_view nodeId) {
        const auto cached = expressions.find(std::string(nodeId));
        if (cached != expressions.end()) return cached->second;
        const EditorGraphNode* node = FindEditorGraphNode(asset.graph, nodeId);
        if (node == nullptr || !IsExpression(node->typeId)) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "gameplay.expression_source",
                std::string(nodeId), "Data input must originate from a supported expression node.");
            return kInvalid;
        }
        if (!visiting.insert(node->id).second) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "gameplay.data_cycle", node->id,
                "Gameplay data expressions must not contain cycles.");
            return kInvalid;
        }
        auto input = [&](std::string_view pin) {
            const EditorGraphLink* link = FindEditorGraphIncomingLink(asset.graph, node->id, pin);
            return link == nullptr ? kInvalid : CompileExpression(link->fromNodeId);
        };
        GameplayExpression expression;
        expression.nodeId = node->id;
        bool valid = true;
        if (node->typeId == "gameplay.literal.float") {
            float value = 0.0f; valid = ParseFloat(Property(*node, "value", "0"), value);
            expression.opcode = GameplayExpressionOpcode::ConstantFloat;
            expression.resultType = GameplayValueType::Float;
            expression.constant = GameplayValue::Float(value);
        } else if (node->typeId == "gameplay.literal.bool") {
            bool value = false; valid = ParseBool(Property(*node, "value", "false"), value);
            expression.opcode = GameplayExpressionOpcode::ConstantBool;
            expression.resultType = GameplayValueType::Bool;
            expression.constant = GameplayValue::Bool(value);
        } else if (node->typeId == "gameplay.literal.int") {
            int32_t value = 0; valid = ParseInt(Property(*node, "value", "0"), value);
            expression.opcode = GameplayExpressionOpcode::ConstantInt;
            expression.resultType = GameplayValueType::Int;
            expression.constant = GameplayValue::Int(value);
        } else if (node->typeId == "gameplay.literal.string") {
            expression.opcode = GameplayExpressionOpcode::ConstantString;
            expression.resultType = GameplayValueType::String;
            expression.constant = GameplayValue::String(Property(*node, "value", {}));
        } else if (node->typeId == "gameplay.variable.get-float" ||
                   node->typeId == "gameplay.variable.get-bool") {
            const auto found = variables.find(Property(*node, "name"));
            const GameplayValueType expected = node->typeId == "gameplay.variable.get-float"
                ? GameplayValueType::Float : GameplayValueType::Bool;
            valid = found != variables.end() &&
                artifact.program.variables[found->second].defaultValue.type == expected;
            expression.opcode = GameplayExpressionOpcode::LoadVariable;
            expression.resultType = expected;
            expression.variableIndex = found == variables.end() ? kInvalid : found->second;
        } else if (node->typeId == "gameplay.math.add-float") {
            expression.opcode = GameplayExpressionOpcode::AddFloat;
            expression.resultType = GameplayValueType::Float;
            expression.left = input("a"); expression.right = input("b");
            valid = expression.left != kInvalid && expression.right != kInvalid;
        } else if (node->typeId == "gameplay.compare.greater-float") {
            expression.opcode = GameplayExpressionOpcode::GreaterFloat;
            expression.resultType = GameplayValueType::Bool;
            expression.left = input("a"); expression.right = input("b");
            valid = expression.left != kInvalid && expression.right != kInvalid;
        } else if (node->typeId == "gameplay.logic.not") {
            expression.opcode = GameplayExpressionOpcode::NotBool;
            expression.resultType = GameplayValueType::Bool;
            expression.left = input("value");
            valid = expression.left != kInvalid;
        } else if (node->typeId == "gameplay.event.tick") {
            expression.opcode = GameplayExpressionOpcode::DeltaTime;
            expression.resultType = GameplayValueType::Float;
        } else valid = false;
        visiting.erase(node->id);
        if (!valid) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "gameplay.expression_invalid", node->id,
                "Gameplay expression has an invalid literal, variable, or operand.");
            return kInvalid;
        }
        const uint32_t index = static_cast<uint32_t>(artifact.program.expressions.size());
        artifact.program.expressions.push_back(std::move(expression));
        expressions[node->id] = index;
        return index;
    }
};

} // namespace

EditorGraphSchema BuildEditorGameplayVisualScriptSchema() {
    EditorGraphSchema schema;
    schema.SetCyclesAllowed(true);
    schema.RegisterNodeType({"gameplay.event.begin-play", "Begin Play", "Events", {ExecOut("exec", "Exec")}});
    schema.RegisterNodeType({"gameplay.event.tick", "Tick", "Events", {
        ExecOut("exec", "Exec"), DataOut("delta", "Delta Seconds", "gameplay.float")}});
    schema.RegisterNodeType({"gameplay.flow.branch", "Branch", "Flow", {
        ExecIn(), DataIn("condition", "Condition", "gameplay.bool"),
        ExecOut("true", "True"), ExecOut("false", "False")}});
    schema.RegisterNodeType({"gameplay.flow.return", "Return", "Flow", {ExecIn()}});
    schema.RegisterNodeType({"gameplay.variable.get-float", "Get Float", "Variables", {
        DataOut("value", "Value", "gameplay.float")}});
    schema.RegisterNodeType({"gameplay.variable.set-float", "Set Float", "Variables", {
        ExecIn(), DataIn("value", "Value", "gameplay.float"), ExecOut()}});
    schema.RegisterNodeType({"gameplay.variable.get-bool", "Get Bool", "Variables", {
        DataOut("value", "Value", "gameplay.bool")}});
    schema.RegisterNodeType({"gameplay.variable.set-bool", "Set Bool", "Variables", {
        ExecIn(), DataIn("value", "Value", "gameplay.bool"), ExecOut()}});
    schema.RegisterNodeType({"gameplay.literal.float", "Float", "Values", {
        DataOut("value", "Value", "gameplay.float")}});
    schema.RegisterNodeType({"gameplay.literal.bool", "Bool", "Values", {
        DataOut("value", "Value", "gameplay.bool")}});
    schema.RegisterNodeType({"gameplay.literal.int", "Int", "Values", {
        DataOut("value", "Value", "gameplay.int")}});
    schema.RegisterNodeType({"gameplay.literal.string", "String", "Values", {
        DataOut("value", "Value", "gameplay.string")}});
    schema.RegisterNodeType({"gameplay.math.add-float", "Add Float", "Math", {
        DataIn("a", "A", "gameplay.float"), DataIn("b", "B", "gameplay.float"),
        DataOut("result", "Result", "gameplay.float")}});
    schema.RegisterNodeType({"gameplay.compare.greater-float", "Float > Float", "Compare", {
        DataIn("a", "A", "gameplay.float"), DataIn("b", "B", "gameplay.float"),
        DataOut("result", "Result", "gameplay.bool")}});
    schema.RegisterNodeType({"gameplay.logic.not", "Not", "Logic", {
        DataIn("value", "Value", "gameplay.bool"), DataOut("result", "Result", "gameplay.bool")}});
    schema.RegisterNodeType({"gameplay.debug.print", "Print String", "Debug", {
        ExecIn(), DataIn("message", "Message", "gameplay.string"), ExecOut()}});
    schema.RegisterNodeType({"gameplay.emit-event", "Emit Event", "Gameplay", {
        ExecIn(), ExecOut()}});
    return schema;
}

EditorGameplayVisualScriptAsset MakeDefaultEditorGameplayVisualScript(
    std::string assetGuid, std::string name) {
    EditorGameplayVisualScriptAsset asset;
    asset.assetGuid = std::move(assetGuid);
    asset.name = name.empty() ? "Gameplay Visual Script" : std::move(name);
    asset.variables.push_back({"Enabled", GameplayValue::Bool(true)});
    EditorGraphNode begin{MakeEditorGraphElementId("node"), "gameplay.event.begin-play", "Begin Play", 40, 100};
    EditorGraphNode text{MakeEditorGraphElementId("node"), "gameplay.literal.string", "String", 260, 260};
    text.properties["value"] = "Gameplay Visual Script started";
    EditorGraphNode print{MakeEditorGraphElementId("node"), "gameplay.debug.print", "Print String", 520, 100};
    EditorGraphNode end{MakeEditorGraphElementId("node"), "gameplay.flow.return", "Return", 800, 100};
    asset.graph.nodes = {begin, text, print, end};
    asset.graph.links = {
        {MakeEditorGraphElementId("link"), begin.id, "exec", print.id, "exec"},
        {MakeEditorGraphElementId("link"), text.id, "value", print.id, "message"},
        {MakeEditorGraphElementId("link"), print.id, "then", end.id, "exec"},
    };
    asset.graph.revision = 1;
    asset.revision = 1;
    return asset;
}

EditorGameplayVisualScriptArtifact CompileEditorGameplayVisualScript(
    const EditorGameplayVisualScriptAsset& asset, const EditorGraphSchema& schema) {
    EditorGameplayVisualScriptArtifact artifact;
    for (const EditorGraphIssue& issue : ValidateEditorGraph(asset.graph, schema).issues) {
        artifact.diagnostics.push_back({issue.severity, issue.code, issue.nodeId, issue.message});
    }
    if (asset.instructionBudget == 0 || asset.instructionBudget > kMaxInstructionBudget) {
        AddIssue(artifact, EditorGraphIssueSeverity::Error, "gameplay.instruction_budget", {},
            "Instruction budget must be between 1 and 1,000,000.");
    }
    if (asset.variables.size() > kEditorGameplayVisualScriptMaxVariables) {
        AddIssue(artifact, EditorGraphIssueSeverity::Error, "gameplay.variable_count", {},
            "Gameplay Visual Script exceeds the 512 variable limit.");
    }
    std::vector<GameplayVariableDefinition> variables = asset.variables;
    std::sort(variables.begin(), variables.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    std::unordered_map<std::string, uint32_t> variableIndices;
    for (GameplayVariableDefinition& variable : variables) {
        const bool finite = variable.defaultValue.type != GameplayValueType::Float ||
            std::isfinite(variable.defaultValue.floatValue);
        if (variable.name.empty() || !finite || variableIndices.find(variable.name) != variableIndices.end()) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "gameplay.variable_invalid", {},
                "Gameplay variables require unique names and valid typed defaults.");
            continue;
        }
        const uint32_t index = static_cast<uint32_t>(artifact.program.variables.size());
        variableIndices[variable.name] = index;
        artifact.program.variables.push_back(variable);
    }
    std::vector<const EditorGraphNode*> events;
    std::vector<const EditorGraphNode*> statements;
    std::vector<const EditorGraphNode*> expressions;
    for (const EditorGraphNode& node : asset.graph.nodes) {
        if (IsEvent(node.typeId)) events.push_back(&node);
        if (IsStatement(node.typeId)) statements.push_back(&node);
        if (IsExpression(node.typeId)) expressions.push_back(&node);
    }
    if (events.empty()) AddIssue(artifact, EditorGraphIssueSeverity::Error,
        "gameplay.event_missing", {}, "Gameplay Visual Script requires at least one Event node.");
    std::set<std::string> eventNames;
    for (const EditorGraphNode* node : events) {
        const std::string eventName = node->typeId == "gameplay.event.begin-play" ? "BeginPlay" : "Tick";
        if (!eventNames.insert(eventName).second) AddIssue(artifact, EditorGraphIssueSeverity::Error,
            "gameplay.event_duplicate", node->id, "Each built-in Gameplay event may appear only once.");
    }
    if (statements.size() > kEditorGameplayVisualScriptMaxInstructions) AddIssue(artifact,
        EditorGraphIssueSeverity::Error, "gameplay.instruction_count", {},
        "Gameplay Visual Script exceeds the 16,384 instruction limit.");
    if (HasErrors(artifact)) return artifact;

    std::sort(expressions.begin(), expressions.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
    CompilerState compiler{asset, artifact, variableIndices};
    for (const EditorGraphNode* node : expressions) compiler.CompileExpression(node->id);

    std::sort(statements.begin(), statements.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
    std::unordered_map<std::string, uint32_t> instructionIndices;
    for (uint32_t index = 0; index < statements.size(); ++index) instructionIndices[statements[index]->id] = index;
    auto target = [&](const EditorGraphNode& node, std::string_view pin) -> uint32_t {
        const EditorGraphLink* found = nullptr;
        for (const EditorGraphLink& link : asset.graph.links) {
            if (link.fromNodeId == node.id && link.fromPinId == pin) {
                if (found != nullptr) {
                    AddIssue(artifact, EditorGraphIssueSeverity::Error, "gameplay.flow_fanout", node.id,
                        "An execution output may connect to only one instruction.");
                    return kInvalid;
                }
                found = &link;
            }
        }
        if (found == nullptr) return kInvalid;
        const auto resolved = instructionIndices.find(found->toNodeId);
        if (resolved == instructionIndices.end()) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "gameplay.flow_target", node.id,
                "Execution flow must target an executable Gameplay node.");
            return kInvalid;
        }
        return resolved->second;
    };
    for (const EditorGraphNode* node : statements) {
        GameplayInstruction instruction;
        instruction.nodeId = node->id;
        if (node->typeId == "gameplay.flow.return") {
            instruction.opcode = GameplayInstructionOpcode::Return;
        } else if (node->typeId == "gameplay.flow.branch") {
            instruction.opcode = GameplayInstructionOpcode::Branch;
            const EditorGraphLink* condition = FindEditorGraphIncomingLink(asset.graph, node->id, "condition");
            instruction.expression = condition == nullptr ? kInvalid : compiler.CompileExpression(condition->fromNodeId);
            instruction.next = target(*node, "true");
            instruction.alternate = target(*node, "false");
        } else if (node->typeId == "gameplay.variable.set-float" ||
                   node->typeId == "gameplay.variable.set-bool") {
            instruction.opcode = GameplayInstructionOpcode::SetVariable;
            const auto variable = variableIndices.find(Property(*node, "name"));
            const GameplayValueType expected = node->typeId == "gameplay.variable.set-float"
                ? GameplayValueType::Float : GameplayValueType::Bool;
            if (variable == variableIndices.end() ||
                artifact.program.variables[variable->second].defaultValue.type != expected) {
                AddIssue(artifact, EditorGraphIssueSeverity::Error, "gameplay.variable_reference", node->id,
                    "Set Variable references a missing variable or the wrong variable type.");
            } else instruction.variableIndex = variable->second;
            const EditorGraphLink* value = FindEditorGraphIncomingLink(asset.graph, node->id, "value");
            instruction.expression = value == nullptr ? kInvalid : compiler.CompileExpression(value->fromNodeId);
            instruction.next = target(*node, "then");
        } else if (node->typeId == "gameplay.debug.print") {
            instruction.opcode = GameplayInstructionOpcode::Print;
            const EditorGraphLink* message = FindEditorGraphIncomingLink(asset.graph, node->id, "message");
            instruction.expression = message == nullptr ? kInvalid : compiler.CompileExpression(message->fromNodeId);
            instruction.next = target(*node, "then");
        } else if (node->typeId == "gameplay.emit-event") {
            instruction.opcode = GameplayInstructionOpcode::EmitEvent;
            instruction.eventName = Property(*node, "eventName");
            if (instruction.eventName.empty()) AddIssue(artifact, EditorGraphIssueSeverity::Error,
                "gameplay.event_name", node->id, "Emit Event requires a non-empty event name.");
            instruction.next = target(*node, "then");
        }
        if ((instruction.opcode == GameplayInstructionOpcode::Branch ||
             instruction.opcode == GameplayInstructionOpcode::SetVariable ||
             instruction.opcode == GameplayInstructionOpcode::Print) && instruction.expression == kInvalid) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "gameplay.instruction_input", node->id,
                "Executable node is missing a valid typed data input.");
        }
        artifact.program.instructions.push_back(std::move(instruction));
    }
    std::sort(events.begin(), events.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
    for (const EditorGraphNode* event : events) {
        artifact.program.events.push_back({event->typeId == "gameplay.event.begin-play" ? "BeginPlay" : "Tick",
            target(*event, "exec")});
    }
    if (HasErrors(artifact)) return artifact;
    artifact.program.maxInstructionsPerExecution = asset.instructionBudget;

    std::ostringstream generated;
    generated << "GAMEPLAY_VISUAL_SCRIPT_PROGRAM 1\nASSET " << std::quoted(asset.assetGuid)
              << "\nBUDGET " << asset.instructionBudget << '\n';
    for (uint32_t i = 0; i < artifact.program.variables.size(); ++i) {
        const auto& v = artifact.program.variables[i];
        generated << "VARIABLE " << i << ' ' << std::quoted(v.name) << ' '
                  << ToString(v.defaultValue.type) << '\n';
    }
    for (uint32_t i = 0; i < artifact.program.expressions.size(); ++i) {
        const auto& expression = artifact.program.expressions[i];
        generated << "EXPRESSION " << i << ' ' << static_cast<int>(expression.opcode) << ' '
                  << expression.left << ' ' << expression.right << ' ' << expression.variableIndex
                  << ' ' << std::quoted(expression.nodeId) << '\n';
    }
    for (uint32_t i = 0; i < artifact.program.instructions.size(); ++i) {
        const auto& instruction = artifact.program.instructions[i];
        generated << "INSTRUCTION " << i << ' ' << static_cast<int>(instruction.opcode) << ' '
                  << instruction.expression << ' ' << instruction.variableIndex << ' '
                  << instruction.next << ' ' << instruction.alternate << ' '
                  << std::quoted(instruction.eventName) << ' ' << std::quoted(instruction.nodeId) << '\n';
    }
    for (const auto& event : artifact.program.events) {
        generated << "EVENT " << std::quoted(event.name) << ' ' << event.instruction << '\n';
    }
    generated << "END\n";
    artifact.generatedProgram = generated.str();
    artifact.sourceFingerprint = HashText(artifact.generatedProgram);
    artifact.program.sourceFingerprint = artifact.sourceFingerprint;
    artifact.succeeded = true;
    return artifact;
}

void EditorGameplayVisualScriptService::Bind(EditorGameplayVisualScriptDocumentProvider* provider,
    EditorTransactionStack* transactions, EditorDocumentManager* documents) {
    provider_ = provider; transactions_ = transactions; documents_ = documents;
}
void EditorGameplayVisualScriptService::SetActiveDocument(EditorDocumentId document) {
    activeDocument_ = std::move(document);
    if (const auto* asset = ActiveAsset()) UpdateCompileArtifact(*asset);
}
EditorGameplayVisualScriptAsset* EditorGameplayVisualScriptService::ActiveAsset() {
    return provider_ != nullptr ? provider_->Asset(activeDocument_) : nullptr;
}
const EditorGameplayVisualScriptAsset* EditorGameplayVisualScriptService::ActiveAsset() const {
    return const_cast<EditorGameplayVisualScriptService*>(this)->ActiveAsset();
}

bool EditorGameplayVisualScriptService::AddNode(std::string_view nodeTypeId, float x, float y,
    std::string* createdNodeId, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    const auto* type = schema_.FindNodeType(nodeTypeId);
    if (asset == nullptr || type == nullptr || !std::isfinite(x) || !std::isfinite(y)) {
        errorMessage = "Active Gameplay Visual Script or node type is unavailable."; return false;
    }
    if (IsEvent(nodeTypeId) && std::any_of(asset->graph.nodes.begin(), asset->graph.nodes.end(),
            [&](const auto& node) { return node.typeId == nodeTypeId; })) {
        errorMessage = "This Gameplay event already exists."; return false;
    }
    auto before = *asset; auto after = before;
    EditorGraphNode node;
    node.id = MakeEditorGraphElementId("node"); node.typeId = type->typeId;
    node.label = type->displayName; node.positionX = x; node.positionY = y;
    if (node.typeId == "gameplay.literal.float" || node.typeId == "gameplay.literal.int") node.properties["value"] = "0";
    else if (node.typeId == "gameplay.literal.bool") node.properties["value"] = "false";
    else if (node.typeId == "gameplay.literal.string") node.properties["value"] = "Text";
    else if (node.typeId == "gameplay.variable.get-float" || node.typeId == "gameplay.variable.set-float" ||
             node.typeId == "gameplay.variable.get-bool" || node.typeId == "gameplay.variable.set-bool") node.properties["name"] = "";
    else if (node.typeId == "gameplay.emit-event") node.properties["eventName"] = "GameplayEvent";
    const std::string id = node.id;
    after.graph.nodes.push_back(std::move(node)); ++after.graph.revision; ++after.revision;
    if (!CommitMutation("Add Gameplay Node", std::move(before), std::move(after), errorMessage)) return false;
    if (createdNodeId != nullptr) *createdNodeId = id;
    return true;
}

bool EditorGameplayVisualScriptService::RemoveNode(std::string_view nodeId, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || FindEditorGraphNode(asset->graph, nodeId) == nullptr) {
        errorMessage = "Gameplay node was not found."; return false;
    }
    auto before = *asset; auto after = before;
    RemoveEditorGraphNode(after.graph, nodeId); ++after.revision;
    return CommitMutation("Remove Gameplay Node", std::move(before), std::move(after), errorMessage);
}

bool EditorGameplayVisualScriptService::Connect(std::string_view fromNodeId, std::string_view fromPinId,
    std::string_view toNodeId, std::string_view toPinId, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    const auto* from = asset ? FindEditorGraphNode(asset->graph, fromNodeId) : nullptr;
    const auto* to = asset ? FindEditorGraphNode(asset->graph, toNodeId) : nullptr;
    const auto* out = from ? schema_.FindPin(from->typeId, fromPinId) : nullptr;
    const auto* in = to ? schema_.FindPin(to->typeId, toPinId) : nullptr;
    if (out == nullptr || in == nullptr || out->direction != EditorGraphPinDirection::Output ||
        in->direction != EditorGraphPinDirection::Input || !schema_.CanConnect(out->typeId, in->typeId)) {
        errorMessage = "Gameplay connection references incompatible nodes or pins."; return false;
    }
    auto before = *asset; auto after = before;
    if (!in->allowMultipleLinks) after.graph.links.erase(std::remove_if(after.graph.links.begin(),
        after.graph.links.end(), [&](const auto& link) { return link.toNodeId == toNodeId && link.toPinId == toPinId; }),
        after.graph.links.end());
    if (out->typeId == "gameplay.exec") after.graph.links.erase(std::remove_if(after.graph.links.begin(),
        after.graph.links.end(), [&](const auto& link) { return link.fromNodeId == fromNodeId && link.fromPinId == fromPinId; }),
        after.graph.links.end());
    after.graph.links.push_back({MakeEditorGraphElementId("link"), std::string(fromNodeId),
        std::string(fromPinId), std::string(toNodeId), std::string(toPinId)});
    ++after.graph.revision; ++after.revision;
    return CommitMutation("Connect Gameplay Nodes", std::move(before), std::move(after), errorMessage);
}

bool EditorGameplayVisualScriptService::Disconnect(std::string_view linkId, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || FindEditorGraphLink(asset->graph, linkId) == nullptr) {
        errorMessage = "Gameplay link was not found."; return false;
    }
    auto before = *asset; auto after = before;
    after.graph.links.erase(std::remove_if(after.graph.links.begin(), after.graph.links.end(),
        [&](const auto& link) { return link.id == linkId; }), after.graph.links.end());
    ++after.graph.revision; ++after.revision;
    return CommitMutation("Disconnect Gameplay Nodes", std::move(before), std::move(after), errorMessage);
}

bool EditorGameplayVisualScriptService::SetNodeProperty(std::string_view nodeId, std::string key,
    std::string value, std::string& errorMessage) {
    auto* asset = ActiveAsset(); if (asset == nullptr || key.empty()) return false;
    auto before = *asset; auto after = before;
    auto* node = FindEditorGraphNode(after.graph, nodeId);
    if (node == nullptr) { errorMessage = "Gameplay node was not found."; return false; }
    if (node->properties[key] == value) return true;
    node->properties[std::move(key)] = std::move(value); ++after.graph.revision; ++after.revision;
    return CommitMutation("Edit Gameplay Node Property", std::move(before), std::move(after), errorMessage);
}

bool EditorGameplayVisualScriptService::MoveNode(std::string_view nodeId, float x, float y,
    std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || !std::isfinite(x) || !std::isfinite(y)) return false;
    auto before = *asset; auto after = before;
    auto* node = FindEditorGraphNode(after.graph, nodeId); if (node == nullptr) return false;
    node->positionX = x; node->positionY = y; ++after.graph.revision; ++after.revision;
    return CommitMutation("Move Gameplay Node", std::move(before), std::move(after), errorMessage);
}

bool EditorGameplayVisualScriptService::AddVariable(std::string name, GameplayValue value,
    std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || name.empty() || asset->variables.size() >= kEditorGameplayVisualScriptMaxVariables ||
        (value.type == GameplayValueType::Float && !std::isfinite(value.floatValue)) ||
        std::any_of(asset->variables.begin(), asset->variables.end(), [&](const auto& v) { return v.name == name; })) {
        errorMessage = "Gameplay variable name, default, or capacity is invalid."; return false;
    }
    auto before = *asset; auto after = before;
    after.variables.push_back({std::move(name), std::move(value)}); ++after.revision;
    return CommitMutation("Add Gameplay Variable", std::move(before), std::move(after), errorMessage);
}

bool EditorGameplayVisualScriptService::RemoveVariable(std::string_view name, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || std::none_of(asset->variables.begin(), asset->variables.end(),
            [&](const auto& v) { return v.name == name; })) {
        errorMessage = "Gameplay variable was not found."; return false;
    }
    auto before = *asset; auto after = before;
    after.variables.erase(std::remove_if(after.variables.begin(), after.variables.end(),
        [&](const auto& v) { return v.name == name; }), after.variables.end());
    for (auto& node : after.graph.nodes) if (Property(node, "name") == name) node.properties["name"] = "";
    ++after.revision;
    return CommitMutation("Remove Gameplay Variable", std::move(before), std::move(after), errorMessage);
}

bool EditorGameplayVisualScriptService::SetInstructionBudget(uint32_t budget, std::string& errorMessage) {
    auto* asset = ActiveAsset();
    if (asset == nullptr || budget == 0 || budget > kMaxInstructionBudget) {
        errorMessage = "Gameplay instruction budget is outside the safe range."; return false;
    }
    if (asset->instructionBudget == budget) return true;
    auto before = *asset; auto after = before; after.instructionBudget = budget; ++after.revision;
    return CommitMutation("Set Gameplay Instruction Budget", std::move(before), std::move(after), errorMessage);
}

bool EditorGameplayVisualScriptService::CommitMutation(std::string_view label,
    EditorGameplayVisualScriptAsset before, EditorGameplayVisualScriptAsset after,
    std::string& errorMessage) {
    if (provider_ == nullptr || transactions_ == nullptr || !activeDocument_.IsValid() ||
        !provider_->Publish(activeDocument_, after)) {
        errorMessage = "Gameplay Visual Script mutation services are unavailable."; return false;
    }
    EditorObjectHandle target;
    target.domain = EditorDomainId::GameplayVisualScriptNode;
    target.stableId = activeDocument_.assetGuid; target.displayName = after.name;
    EditorError error;
    if (!transactions_->PushCommand(std::string(label), std::move(target),
            std::make_shared<GameplayVisualScriptUndoCommand>(activeDocument_, before, after), &error)) {
        provider_->Publish(activeDocument_, std::move(before)); errorMessage = error.message; return false;
    }
    UpdateCompileArtifact(after);
    if (documents_ != nullptr) documents_->MarkDirty(activeDocument_, label);
    if (mutationCallback_) mutationCallback_(activeDocument_, label);
    return true;
}

bool EditorGameplayVisualScriptService::PublishFromCommand(const EditorDocumentId& document,
    const EditorGameplayVisualScriptAsset& asset, std::string& errorMessage) {
    if (provider_ == nullptr || !provider_->Publish(document, asset)) {
        errorMessage = "Gameplay Visual Script transaction could not publish its snapshot."; return false;
    }
    if (document == activeDocument_) UpdateCompileArtifact(asset);
    if (documents_ != nullptr) documents_->MarkDirty(document, "Gameplay Visual Script Undo/Redo");
    if (mutationCallback_) mutationCallback_(document, "Gameplay Visual Script Undo/Redo");
    return true;
}

void EditorGameplayVisualScriptService::UpdateCompileArtifact(
    const EditorGameplayVisualScriptAsset& asset) {
    lastCompileArtifact_ = CompileEditorGameplayVisualScript(asset, schema_);
    if (lastCompileArtifact_.succeeded) {
        lastSuccessfulArtifact_ = lastCompileArtifact_;
        preview_.SetProgram(&lastSuccessfulArtifact_.program);
    }
}
bool EditorGameplayVisualScriptService::Recompile(std::string& errorMessage) {
    const auto* asset = ActiveAsset(); if (asset == nullptr) return false;
    UpdateCompileArtifact(*asset);
    if (!lastCompileArtifact_.succeeded) {
        errorMessage = lastCompileArtifact_.diagnostics.empty() ? "Gameplay Visual Script compile failed."
            : lastCompileArtifact_.diagnostics.front().message;
        return false;
    }
    return true;
}
bool EditorGameplayVisualScriptService::ResetPreview(std::string& errorMessage) {
    if (!Recompile(errorMessage)) return false;
    preview_.Reset(); previewOutput_.clear(); lastPreviewResult_ = {};
    return true;
}
bool EditorGameplayVisualScriptService::ExecutePreview(std::string_view eventName, float deltaTime,
    std::string& errorMessage) {
    if (lastSuccessfulArtifact_.program.events.empty() && !ResetPreview(errorMessage)) return false;
    GameplayVisualScriptContext context;
    context.deltaTime = deltaTime;
    context.print = [&](std::string_view value) { previewOutput_.push_back(std::string(value)); };
    context.emitEvent = [&](std::string_view value) { previewOutput_.push_back("Event: " + std::string(value)); };
    lastPreviewResult_ = preview_.ExecuteEvent(eventName, context);
    if (lastPreviewResult_.status != GameplayExecutionStatus::Completed) {
        errorMessage = lastPreviewResult_.error; return false;
    }
    return true;
}
bool EditorGameplayVisualScriptService::SetPreviewVariable(
    std::string_view name, const GameplayValue& value) {
    return preview_.SetVariable(name, value);
}

} // namespace editor
