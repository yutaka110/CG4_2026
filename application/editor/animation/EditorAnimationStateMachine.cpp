#include "EditorAnimationStateMachine.h"

#include "../EditorSelection.h"
#include "../EditorTransactionStack.h"
#include "../core/EditorExecutionContext.h"
#include "../core/EditorUndoCommand.h"
#include "../documents/EditorAnimationStateMachineDocumentProvider.h"
#include "../documents/EditorDocumentManager.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <unordered_map>

namespace editor {
namespace {

EditorGraphPinDefinition InputPin(std::string id, std::string label,
    bool required = false, bool multiple = false) {
    return {std::move(id), std::move(label), "animation.state-flow",
        EditorGraphPinDirection::Input, required, multiple};
}

EditorGraphPinDefinition OutputPin(std::string id, std::string label) {
    return {std::move(id), std::move(label), "animation.state-flow",
        EditorGraphPinDirection::Output, false, true};
}

std::string Property(const EditorGraphNode& node, std::string_view key, std::string fallback) {
    const auto found = node.properties.find(std::string(key));
    return found == node.properties.end() ? std::move(fallback) : found->second;
}

bool ParseFloat(std::string_view text, float& value) {
    std::istringstream input{std::string(text)};
    std::string extra;
    return static_cast<bool>(input >> value) && std::isfinite(value) && !(input >> extra);
}

bool ParseInt(std::string_view text, int32_t& value) {
    std::istringstream input{std::string(text)};
    std::string extra;
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
    for (const unsigned char byte : text) {
        hash ^= static_cast<uint64_t>(byte);
        hash *= 1099511628211ull;
    }
    return hash;
}

void AddIssue(EditorAnimationStateMachineArtifact& artifact, EditorGraphIssueSeverity severity,
    std::string code, std::string nodeId, std::string message) {
    artifact.diagnostics.push_back(
        {severity, std::move(code), std::move(nodeId), std::move(message)});
}

bool HasErrors(const EditorAnimationStateMachineArtifact& artifact) {
    return std::any_of(artifact.diagnostics.begin(), artifact.diagnostics.end(),
        [](const auto& issue) { return issue.severity == EditorGraphIssueSeverity::Error; });
}

const EditorGraphLink* OutgoingLink(const EditorGraph& graph,
    std::string_view nodeId, std::string_view pinId) {
    for (const EditorGraphLink& link : graph.links) {
        if (link.fromNodeId == nodeId && link.fromPinId == pinId) return &link;
    }
    return nullptr;
}

class AnimationStateMachineUndoCommand final : public IEditorUndoCommand {
public:
    AnimationStateMachineUndoCommand(EditorDocumentId document,
        EditorAnimationStateMachineAsset before, EditorAnimationStateMachineAsset after)
        : document_(std::move(document)), before_(std::move(before)), after_(std::move(after)) {}

    EditorUndoResult Apply(EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override {
        auto* service = dynamic_cast<EditorAnimationStateMachineService*>(
            context.Find(EditorAnimationStateMachineService::kServiceId));
        if (service == nullptr) {
            return EditorUndoResult::Failure(EditorErrorCode::MissingService,
                "Animation State Machine execution service is unavailable.");
        }
        std::string error;
        const EditorAnimationStateMachineAsset& asset =
            mode == EditorTransactionApplyMode::Undo ? before_ : after_;
        if (!service->PublishFromCommand(document_, asset, error)) {
            return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, std::move(error));
        }
        return EditorUndoResult::Success();
    }

    std::size_t EstimatedBytes() const noexcept override {
        return sizeof(*this) + before_.graph.nodes.size() * sizeof(EditorGraphNode) +
            after_.graph.nodes.size() * sizeof(EditorGraphNode) +
            before_.graph.links.size() * sizeof(EditorGraphLink) +
            after_.graph.links.size() * sizeof(EditorGraphLink);
    }
    std::string_view DomainId() const noexcept override { return "animation-state-machine"; }
    std::string_view TypeId() const noexcept override { return "animation-state-machine.snapshot"; }

private:
    EditorDocumentId document_;
    EditorAnimationStateMachineAsset before_;
    EditorAnimationStateMachineAsset after_;
};

} // namespace

EditorGraphSchema BuildEditorAnimationStateMachineSchema() {
    EditorGraphSchema schema;
    schema.SetCyclesAllowed(true);
    schema.RegisterNodeType({"animation.entry", "Entry", "State Machine", {
        OutputPin("entry", "Entry")}});
    schema.RegisterNodeType({"animation.state", "Animation State", "State Machine", {
        InputPin("enter", "Enter", true, true), OutputPin("state", "State")}});
    schema.RegisterNodeType({"animation.transition", "Transition", "State Machine", {
        InputPin("source", "Source", true), OutputPin("target", "Target")}});
    return schema;
}

EditorAnimationStateMachineAsset MakeDefaultEditorAnimationStateMachine(
    std::string assetGuid, std::string name) {
    EditorAnimationStateMachineAsset asset;
    asset.assetGuid = std::move(assetGuid);
    asset.name = name.empty() ? "Animation State Machine" : std::move(name);
    asset.parameters.push_back({"Speed", AnimationParameterType::Float, 0.0f});

    EditorGraphNode entry;
    entry.id = MakeEditorGraphElementId("node");
    entry.typeId = "animation.entry";
    entry.label = "Entry";
    entry.positionX = 40.0f;
    entry.positionY = 100.0f;
    EditorGraphNode idle;
    idle.id = MakeEditorGraphElementId("node");
    idle.typeId = "animation.state";
    idle.label = "Idle";
    idle.positionX = 300.0f;
    idle.positionY = 100.0f;
    idle.properties["name"] = "Idle";
    idle.properties["sourceAssetGuid"] = "";
    idle.properties["clipName"] = "Idle";
    idle.properties["speed"] = "1";
    idle.properties["loop"] = "true";
    asset.graph.nodes = {entry, idle};
    asset.graph.links.push_back({MakeEditorGraphElementId("link"), entry.id,
        "entry", idle.id, "enter"});
    asset.graph.revision = 1;
    asset.revision = 1;
    return asset;
}

EditorAnimationStateMachineArtifact CompileEditorAnimationStateMachine(
    const EditorAnimationStateMachineAsset& asset, const EditorGraphSchema& schema) {
    EditorAnimationStateMachineArtifact artifact;
    const EditorGraphValidationReport validation = ValidateEditorGraph(asset.graph, schema);
    for (const EditorGraphIssue& issue : validation.issues) {
        artifact.diagnostics.push_back({issue.severity, issue.code, issue.nodeId, issue.message});
    }
    std::vector<const EditorGraphNode*> entries;
    std::vector<const EditorGraphNode*> states;
    std::vector<const EditorGraphNode*> transitions;
    for (const EditorGraphNode& node : asset.graph.nodes) {
        if (node.typeId == "animation.entry") entries.push_back(&node);
        else if (node.typeId == "animation.state") states.push_back(&node);
        else if (node.typeId == "animation.transition") transitions.push_back(&node);
    }
    if (entries.size() != 1) AddIssue(artifact, EditorGraphIssueSeverity::Error,
        "animation.entry_count", {}, "State Machine must contain exactly one Entry node.");
    if (states.empty() || states.size() > kEditorAnimationStateMachineMaxStates) {
        AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.state_count", {},
            "State Machine must contain between 1 and 1024 states.");
    }
    if (transitions.size() > kEditorAnimationStateMachineMaxTransitions) {
        AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.transition_count", {},
            "State Machine exceeds the 8192 transition limit.");
    }
    if (asset.parameters.size() > kEditorAnimationStateMachineMaxParameters) {
        AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.parameter_count", {},
            "State Machine exceeds the 256 parameter limit.");
    }
    std::set<std::string> parameterNames;
    for (const AnimationStateMachineParameter& parameter : asset.parameters) {
        if (parameter.name.empty() || !parameterNames.insert(parameter.name).second ||
            !std::isfinite(parameter.defaultValue)) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.parameter_invalid", {},
                "Animation parameters require unique names and finite defaults.");
        }
    }
    if (validation.HasErrors() || HasErrors(artifact) || entries.size() != 1) return artifact;

    std::sort(states.begin(), states.end(), [](const auto* lhs, const auto* rhs) {
        return lhs->id < rhs->id;
    });
    std::unordered_map<std::string, uint32_t> stateIndices;
    std::set<std::string> stateNames;
    std::set<std::string> sourceGuids;
    for (uint32_t index = 0; index < states.size(); ++index) {
        const EditorGraphNode& node = *states[index];
        AnimationStateMachineState state;
        state.id = node.id;
        state.name = Property(node, "name", node.label);
        state.sourceAssetGuid = Property(node, "sourceAssetGuid", {});
        state.clipName = Property(node, "clipName", {});
        if (state.name.empty() || !stateNames.insert(state.name).second) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.state_name", node.id,
                "Animation state names must be non-empty and unique.");
        }
        if (state.sourceAssetGuid.empty() || state.clipName.empty()) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.clip_reference", node.id,
                "Animation State requires a durable source Asset GUID and clip name.");
        }
        if (!ParseFloat(Property(node, "speed", "1"), state.speed) || state.speed < 0.0f) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.state_speed", node.id,
                "Animation state speed must be a finite non-negative value.");
        }
        if (!ParseBool(Property(node, "loop", "true"), state.loop)) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.state_loop", node.id,
                "Animation state loop property must be true or false.");
        }
        stateIndices[node.id] = index;
        if (!state.sourceAssetGuid.empty()) sourceGuids.insert(state.sourceAssetGuid);
        artifact.program.states.push_back(std::move(state));
    }
    const EditorGraphLink* entryLink = OutgoingLink(asset.graph, entries.front()->id, "entry");
    if (entryLink == nullptr || stateIndices.find(entryLink->toNodeId) == stateIndices.end()) {
        AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.entry_target", entries.front()->id,
            "Entry must connect directly to an Animation State.");
    } else artifact.program.entryState = stateIndices[entryLink->toNodeId];

    std::sort(transitions.begin(), transitions.end(), [](const auto* lhs, const auto* rhs) {
        return lhs->id < rhs->id;
    });
    for (const EditorGraphNode* node : transitions) {
        const EditorGraphLink* source = FindEditorGraphIncomingLink(asset.graph, node->id, "source");
        const EditorGraphLink* target = OutgoingLink(asset.graph, node->id, "target");
        if (source == nullptr || target == nullptr || stateIndices.find(source->fromNodeId) == stateIndices.end() ||
            stateIndices.find(target->toNodeId) == stateIndices.end()) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.transition_endpoint", node->id,
                "Transition must connect one source State to one target State.");
            continue;
        }
        AnimationStateMachineTransition transition;
        transition.id = node->id;
        transition.sourceState = stateIndices[source->fromNodeId];
        transition.targetState = stateIndices[target->toNodeId];
        transition.parameter = Property(*node, "parameter", {});
        if (!AnimationConditionOperatorFromString(
                Property(*node, "condition", "Always"), transition.condition)) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.transition_condition", node->id,
                "Transition condition operator is unsupported.");
        }
        if (!ParseFloat(Property(*node, "threshold", "0"), transition.threshold) ||
            !ParseFloat(Property(*node, "exitTime", "0"), transition.exitTimeNormalized) ||
            !ParseFloat(Property(*node, "blendDuration", "0.2"), transition.blendDuration) ||
            !ParseInt(Property(*node, "priority", "0"), transition.priority) ||
            transition.exitTimeNormalized < 0.0f || transition.exitTimeNormalized > 1.0f ||
            transition.blendDuration < 0.0f || transition.blendDuration > 60.0f) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.transition_settings", node->id,
                "Transition threshold, exit time, blend duration, or priority is invalid.");
        }
        if (transition.condition != AnimationConditionOperator::Always &&
            parameterNames.find(transition.parameter) == parameterNames.end()) {
            AddIssue(artifact, EditorGraphIssueSeverity::Error, "animation.transition_parameter", node->id,
                "Transition references an undeclared Animation parameter.");
        }
        artifact.program.transitions.push_back(std::move(transition));
    }
    if (HasErrors(artifact)) return artifact;

    artifact.program.parameters = asset.parameters;
    std::ostringstream generated;
    generated << "ANIMATION_STATE_MACHINE_PROGRAM 1\nASSET " << std::quoted(asset.assetGuid) << '\n';
    generated << "ENTRY " << artifact.program.entryState << '\n';
    for (const AnimationStateMachineParameter& parameter : artifact.program.parameters) {
        generated << "PARAMETER " << std::quoted(parameter.name) << ' ' << ToString(parameter.type)
                  << ' ' << std::setprecision(9) << parameter.defaultValue << '\n';
    }
    for (uint32_t index = 0; index < artifact.program.states.size(); ++index) {
        const AnimationStateMachineState& state = artifact.program.states[index];
        generated << "STATE " << index << ' ' << std::quoted(state.id) << ' '
                  << std::quoted(state.name) << ' ' << std::quoted(state.sourceAssetGuid) << ' '
                  << std::quoted(state.clipName) << ' ' << state.speed << ' ' << state.loop << '\n';
    }
    for (const AnimationStateMachineTransition& transition : artifact.program.transitions) {
        generated << "TRANSITION " << std::quoted(transition.id) << ' ' << transition.sourceState
                  << ' ' << transition.targetState << ' ' << ToString(transition.condition) << ' '
                  << std::quoted(transition.parameter) << ' ' << transition.threshold << ' '
                  << transition.exitTimeNormalized << ' ' << transition.blendDuration << ' '
                  << transition.priority << '\n';
    }
    generated << "END\n";
    artifact.generatedProgram = generated.str();
    artifact.sourceFingerprint = HashText(artifact.generatedProgram);
    artifact.program.fingerprint = artifact.sourceFingerprint;
    artifact.animationSourceAssetGuids.assign(sourceGuids.begin(), sourceGuids.end());
    artifact.succeeded = true;
    return artifact;
}

void EditorAnimationStateMachineService::Bind(
    EditorAnimationStateMachineDocumentProvider* provider,
    EditorTransactionStack* transactions, EditorDocumentManager* documents) {
    provider_ = provider;
    transactions_ = transactions;
    documents_ = documents;
}

void EditorAnimationStateMachineService::SetActiveDocument(EditorDocumentId document) {
    activeDocument_ = std::move(document);
    if (const auto* asset = ActiveAsset()) UpdateCompileArtifact(*asset);
}

EditorAnimationStateMachineAsset* EditorAnimationStateMachineService::ActiveAsset() {
    return provider_ != nullptr ? provider_->Asset(activeDocument_) : nullptr;
}

const EditorAnimationStateMachineAsset* EditorAnimationStateMachineService::ActiveAsset() const {
    return const_cast<EditorAnimationStateMachineService*>(this)->ActiveAsset();
}

bool EditorAnimationStateMachineService::AddNode(std::string_view nodeTypeId,
    float x, float y, std::string* createdNodeId, std::string& errorMessage) {
    EditorAnimationStateMachineAsset* asset = ActiveAsset();
    const EditorGraphNodeTypeDefinition* type = schema_.FindNodeType(nodeTypeId);
    if (asset == nullptr || type == nullptr || !std::isfinite(x) || !std::isfinite(y)) {
        errorMessage = "Active Animation State Machine or node type is unavailable.";
        return false;
    }
    if (nodeTypeId == "animation.entry" && std::any_of(asset->graph.nodes.begin(),
            asset->graph.nodes.end(), [](const auto& node) { return node.typeId == "animation.entry"; })) {
        errorMessage = "Animation State Machine already has an Entry node.";
        return false;
    }
    EditorAnimationStateMachineAsset before = *asset;
    EditorAnimationStateMachineAsset after = before;
    EditorGraphNode node;
    node.id = MakeEditorGraphElementId("node");
    node.typeId = type->typeId;
    node.label = type->displayName;
    node.positionX = x;
    node.positionY = y;
    if (node.typeId == "animation.state") {
        node.properties = {{"clipName", "Clip"}, {"loop", "true"}, {"name", "State"},
            {"sourceAssetGuid", ""}, {"speed", "1"}};
    } else if (node.typeId == "animation.transition") {
        node.properties = {{"blendDuration", "0.2"}, {"condition", "Always"},
            {"exitTime", "0"}, {"parameter", ""}, {"priority", "0"}, {"threshold", "0"}};
    }
    const std::string id = node.id;
    after.graph.nodes.push_back(std::move(node));
    ++after.graph.revision;
    ++after.revision;
    if (!CommitMutation("Add Animation State Machine Node", std::move(before), std::move(after), errorMessage)) return false;
    if (createdNodeId != nullptr) *createdNodeId = id;
    return true;
}

bool EditorAnimationStateMachineService::RemoveNode(
    std::string_view nodeId, std::string& errorMessage) {
    EditorAnimationStateMachineAsset* asset = ActiveAsset();
    const EditorGraphNode* node = asset != nullptr ? FindEditorGraphNode(asset->graph, nodeId) : nullptr;
    if (node == nullptr || node->typeId == "animation.entry") {
        errorMessage = node == nullptr ? "Animation node was not found." : "Entry cannot be removed.";
        return false;
    }
    EditorAnimationStateMachineAsset before = *asset;
    EditorAnimationStateMachineAsset after = before;
    RemoveEditorGraphNode(after.graph, nodeId);
    ++after.revision;
    return CommitMutation("Remove Animation State Machine Node", std::move(before), std::move(after), errorMessage);
}

bool EditorAnimationStateMachineService::Connect(std::string_view fromNodeId,
    std::string_view fromPinId, std::string_view toNodeId, std::string_view toPinId,
    std::string& errorMessage) {
    EditorAnimationStateMachineAsset* asset = ActiveAsset();
    const EditorGraphNode* from = asset != nullptr ? FindEditorGraphNode(asset->graph, fromNodeId) : nullptr;
    const EditorGraphNode* to = asset != nullptr ? FindEditorGraphNode(asset->graph, toNodeId) : nullptr;
    const EditorGraphPinDefinition* fromPin = from != nullptr ? schema_.FindPin(from->typeId, fromPinId) : nullptr;
    const EditorGraphPinDefinition* toPin = to != nullptr ? schema_.FindPin(to->typeId, toPinId) : nullptr;
    if (fromPin == nullptr || toPin == nullptr || fromPin->direction != EditorGraphPinDirection::Output ||
        toPin->direction != EditorGraphPinDirection::Input || !schema_.CanConnect(fromPin->typeId, toPin->typeId)) {
        errorMessage = "Animation State Machine connection references an incompatible node or pin.";
        return false;
    }
    EditorAnimationStateMachineAsset before = *asset;
    EditorAnimationStateMachineAsset after = before;
    if (!toPin->allowMultipleLinks) {
        after.graph.links.erase(std::remove_if(after.graph.links.begin(), after.graph.links.end(),
            [&](const auto& link) { return link.toNodeId == toNodeId && link.toPinId == toPinId; }),
            after.graph.links.end());
    }
    after.graph.links.push_back({MakeEditorGraphElementId("link"), std::string(fromNodeId),
        std::string(fromPinId), std::string(toNodeId), std::string(toPinId)});
    ++after.graph.revision;
    ++after.revision;
    return CommitMutation("Connect Animation State Machine", std::move(before), std::move(after), errorMessage);
}

bool EditorAnimationStateMachineService::Disconnect(
    std::string_view linkId, std::string& errorMessage) {
    EditorAnimationStateMachineAsset* asset = ActiveAsset();
    if (asset == nullptr || FindEditorGraphLink(asset->graph, linkId) == nullptr) {
        errorMessage = "Animation State Machine link was not found.";
        return false;
    }
    EditorAnimationStateMachineAsset before = *asset;
    EditorAnimationStateMachineAsset after = before;
    after.graph.links.erase(std::remove_if(after.graph.links.begin(), after.graph.links.end(),
        [&](const auto& link) { return link.id == linkId; }), after.graph.links.end());
    ++after.graph.revision;
    ++after.revision;
    return CommitMutation("Disconnect Animation State Machine", std::move(before), std::move(after), errorMessage);
}

bool EditorAnimationStateMachineService::SetNodeProperty(std::string_view nodeId,
    std::string key, std::string value, std::string& errorMessage) {
    EditorAnimationStateMachineAsset* asset = ActiveAsset();
    if (asset == nullptr || key.empty()) return false;
    EditorAnimationStateMachineAsset before = *asset;
    EditorAnimationStateMachineAsset after = before;
    EditorGraphNode* node = FindEditorGraphNode(after.graph, nodeId);
    if (node == nullptr) {
        errorMessage = "Animation State Machine node was not found.";
        return false;
    }
    if (node->properties[key] == value) return true;
    node->properties[std::move(key)] = std::move(value);
    ++after.graph.revision;
    ++after.revision;
    return CommitMutation("Edit Animation State Machine Property", std::move(before), std::move(after), errorMessage);
}

bool EditorAnimationStateMachineService::MoveNode(std::string_view nodeId,
    float x, float y, std::string& errorMessage) {
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    EditorAnimationStateMachineAsset* asset = ActiveAsset();
    if (asset == nullptr) return false;
    EditorAnimationStateMachineAsset before = *asset;
    EditorAnimationStateMachineAsset after = before;
    EditorGraphNode* node = FindEditorGraphNode(after.graph, nodeId);
    if (node == nullptr) return false;
    node->positionX = x;
    node->positionY = y;
    ++after.graph.revision;
    ++after.revision;
    return CommitMutation("Move Animation State Machine Node", std::move(before), std::move(after), errorMessage);
}

bool EditorAnimationStateMachineService::AddParameter(std::string name,
    AnimationParameterType type, float defaultValue, std::string& errorMessage) {
    EditorAnimationStateMachineAsset* asset = ActiveAsset();
    if (asset == nullptr || name.empty() || !std::isfinite(defaultValue) ||
        asset->parameters.size() >= kEditorAnimationStateMachineMaxParameters ||
        std::any_of(asset->parameters.begin(), asset->parameters.end(),
            [&](const auto& parameter) { return parameter.name == name; })) {
        errorMessage = "Animation parameter name, default, or capacity is invalid.";
        return false;
    }
    EditorAnimationStateMachineAsset before = *asset;
    EditorAnimationStateMachineAsset after = before;
    after.parameters.push_back({std::move(name), type, defaultValue});
    ++after.revision;
    return CommitMutation("Add Animation Parameter", std::move(before), std::move(after), errorMessage);
}

bool EditorAnimationStateMachineService::RemoveParameter(
    std::string_view name, std::string& errorMessage) {
    EditorAnimationStateMachineAsset* asset = ActiveAsset();
    const auto found = asset != nullptr ? std::find_if(asset->parameters.begin(), asset->parameters.end(),
        [&](const auto& parameter) { return parameter.name == name; }) : std::vector<AnimationStateMachineParameter>::iterator{};
    if (asset == nullptr || found == asset->parameters.end()) {
        errorMessage = "Animation parameter was not found.";
        return false;
    }
    EditorAnimationStateMachineAsset before = *asset;
    EditorAnimationStateMachineAsset after = before;
    after.parameters.erase(std::remove_if(after.parameters.begin(), after.parameters.end(),
        [&](const auto& parameter) { return parameter.name == name; }), after.parameters.end());
    for (EditorGraphNode& node : after.graph.nodes) {
        if (node.typeId == "animation.transition" && Property(node, "parameter", {}) == name) {
            node.properties["parameter"] = "";
            node.properties["condition"] = "Always";
        }
    }
    ++after.revision;
    return CommitMutation("Remove Animation Parameter", std::move(before), std::move(after), errorMessage);
}

bool EditorAnimationStateMachineService::CommitMutation(std::string_view label,
    EditorAnimationStateMachineAsset before, EditorAnimationStateMachineAsset after,
    std::string& errorMessage) {
    if (provider_ == nullptr || transactions_ == nullptr || !activeDocument_.IsValid() ||
        !provider_->Publish(activeDocument_, after)) {
        errorMessage = "Animation State Machine mutation services are unavailable.";
        return false;
    }
    EditorObjectHandle target;
    target.domain = EditorDomainId::AnimationStateMachineNode;
    target.stableId = activeDocument_.assetGuid;
    target.displayName = after.name;
    EditorError error;
    if (!transactions_->PushCommand(std::string(label), std::move(target),
            std::make_shared<AnimationStateMachineUndoCommand>(activeDocument_, before, after), &error)) {
        provider_->Publish(activeDocument_, std::move(before));
        errorMessage = error.message;
        return false;
    }
    UpdateCompileArtifact(after);
    if (documents_ != nullptr) documents_->MarkDirty(activeDocument_, label);
    if (mutationCallback_) mutationCallback_(activeDocument_, label);
    return true;
}

bool EditorAnimationStateMachineService::PublishFromCommand(const EditorDocumentId& document,
    const EditorAnimationStateMachineAsset& asset, std::string& errorMessage) {
    if (provider_ == nullptr || !provider_->Publish(document, asset)) {
        errorMessage = "Animation State Machine transaction could not publish its snapshot.";
        return false;
    }
    if (document == activeDocument_) UpdateCompileArtifact(asset);
    if (documents_ != nullptr) documents_->MarkDirty(document, "Animation State Machine Undo/Redo");
    if (mutationCallback_) mutationCallback_(document, "Animation State Machine Undo/Redo");
    return true;
}

void EditorAnimationStateMachineService::UpdateCompileArtifact(
    const EditorAnimationStateMachineAsset& asset) {
    lastCompileArtifact_ = CompileEditorAnimationStateMachine(asset, schema_);
    if (lastCompileArtifact_.succeeded) {
        lastSuccessfulArtifact_ = lastCompileArtifact_;
        preview_.SetProgram(&lastSuccessfulArtifact_.program);
    }
}

bool EditorAnimationStateMachineService::Recompile(std::string& errorMessage) {
    const auto* asset = ActiveAsset();
    if (asset == nullptr) return false;
    UpdateCompileArtifact(*asset);
    if (!lastCompileArtifact_.succeeded) {
        errorMessage = lastCompileArtifact_.diagnostics.empty()
            ? "Animation State Machine compile failed." : lastCompileArtifact_.diagnostics.front().message;
        return false;
    }
    return true;
}

bool EditorAnimationStateMachineService::ResetPreview(std::string& errorMessage) {
    if (!Recompile(errorMessage)) return false;
    preview_.Reset();
    return true;
}

bool EditorAnimationStateMachineService::StepPreview(float deltaTime, std::string& errorMessage) {
    if (!preview_.Sample().valid && !ResetPreview(errorMessage)) return false;
    preview_.Update(deltaTime, [](std::string_view, std::string_view) { return 1.0f; });
    return true;
}

bool EditorAnimationStateMachineService::SetPreviewFloat(std::string_view name, float value) {
    return preview_.SetFloat(name, value);
}
bool EditorAnimationStateMachineService::SetPreviewInt(std::string_view name, int32_t value) {
    return preview_.SetInt(name, value);
}
bool EditorAnimationStateMachineService::SetPreviewBool(std::string_view name, bool value) {
    return preview_.SetBool(name, value);
}
bool EditorAnimationStateMachineService::FirePreviewTrigger(std::string_view name) {
    return preview_.FireTrigger(name);
}

} // namespace editor
