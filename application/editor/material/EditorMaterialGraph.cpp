#include "EditorMaterialGraph.h"

#include "../EditorSelection.h"
#include "../EditorTransactionStack.h"
#include "../core/EditorExecutionContext.h"
#include "../core/EditorUndoCommand.h"
#include "../documents/EditorDocumentManager.h"
#include "../documents/EditorMaterialGraphDocumentProvider.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace editor {
namespace {

EditorGraphPinDefinition InputPin(
    std::string id,
    std::string label,
    std::string type,
    bool required = false) {
    return {std::move(id), std::move(label), std::move(type),
        EditorGraphPinDirection::Input, required, false};
}

EditorGraphPinDefinition OutputPin(
    std::string id,
    std::string label,
    std::string type) {
    return {std::move(id), std::move(label), std::move(type),
        EditorGraphPinDirection::Output, false, true};
}

std::string Property(const EditorGraphNode& node, std::string_view key, std::string fallback) {
    const auto found = node.properties.find(std::string(key));
    return found == node.properties.end() ? std::move(fallback) : found->second;
}

std::string SafeIdentifier(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (char value : text) {
        const bool valid = (value >= 'a' && value <= 'z') ||
            (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9');
        result.push_back(valid ? value : '_');
    }
    if (result.empty() || (result.front() >= '0' && result.front() <= '9')) result.insert(result.begin(), '_');
    return result;
}

bool ParseScalarLiteral(std::string_view text, std::string& normalized) {
    std::istringstream input{std::string(text)};
    double value = 0.0;
    std::string extra;
    if (!(input >> value) || !std::isfinite(value) || (input >> extra)) return false;
    std::ostringstream output;
    output << std::setprecision(9) << value;
    normalized = output.str();
    if (normalized.find_first_of(".eE") == std::string::npos) normalized += ".0";
    return true;
}

bool ParseVector3Literal(std::string_view text, std::string& normalized) {
    std::string values(text);
    std::replace(values.begin(), values.end(), ',', ' ');
    std::istringstream input(values);
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::string extra;
    if (!(input >> x >> y >> z) || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(z) || (input >> extra)) return false;
    const auto format = [](double value) {
        std::ostringstream output;
        output << std::setprecision(9) << value;
        std::string result = output.str();
        if (result.find_first_of(".eE") == std::string::npos) result += ".0";
        return result;
    };
    normalized = format(x) + ", " + format(y) + ", " + format(z);
    return true;
}

uint64_t HashText(std::string_view value) {
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char byte : value) {
        hash ^= static_cast<uint64_t>(byte);
        hash *= 1099511628211ull;
    }
    return hash;
}

struct ExpressionEmitter {
    const EditorMaterialGraphAsset& asset;
    const EditorGraphSchema& schema;
    std::unordered_map<std::string, std::string> cache;
    std::set<std::string> textureGuids;
    std::vector<EditorMaterialCompileDiagnostic> diagnostics;

    std::string Input(
        const EditorGraphNode& node,
        std::string_view pinId,
        std::string fallback) {
        const EditorGraphLink* link = FindEditorGraphIncomingLink(asset.graph, node.id, pinId);
        if (link == nullptr) return fallback;
        return Emit(link->fromNodeId, link->fromPinId);
    }

    std::string Emit(std::string_view nodeId, std::string_view pinId) {
        const std::string key = std::string(nodeId) + ":" + std::string(pinId);
        const auto cached = cache.find(key);
        if (cached != cache.end()) return cached->second;
        const EditorGraphNode* node = FindEditorGraphNode(asset.graph, nodeId);
        if (node == nullptr) return "0.0";

        std::string expression;
        if (node->typeId == "material.constant.scalar") {
            if (!ParseScalarLiteral(Property(*node, "value", "0.0"), expression)) {
                diagnostics.push_back({EditorGraphIssueSeverity::Error,
                    "material.scalar_literal", node->id,
                    "Scalar node value is not a finite numeric literal."});
                expression = "0.0";
            }
        } else if (node->typeId == "material.constant.vector3") {
            std::string values;
            if (!ParseVector3Literal(Property(*node, "value", "0.5, 0.5, 0.5"), values)) {
                diagnostics.push_back({EditorGraphIssueSeverity::Error,
                    "material.vector3_literal", node->id,
                    "Vector 3 node value must contain three finite numeric literals."});
                values = "0.5, 0.5, 0.5";
            }
            expression = "float3(" + values + ")";
        } else if (node->typeId == "material.input.texcoord") {
            expression = "IN.uv";
        } else if (node->typeId == "material.texture2d") {
            const std::string guid = Property(*node, "assetGuid", {});
            if (guid.empty()) {
                diagnostics.push_back({EditorGraphIssueSeverity::Error,
                    "material.texture_missing", node->id,
                    "Texture Sample requires a durable Texture Asset GUID."});
                expression = pinId == "alpha" ? "1.0" :
                    (pinId == "rgb" ? "float3(1.0, 0.0, 1.0)" : "float4(1.0, 0.0, 1.0, 1.0)");
            } else {
                textureGuids.insert(guid);
                const std::string sample = "MG_Texture_" + SafeIdentifier(node->id) +
                    ".Sample(MG_LinearSampler, " + Input(*node, "uv", "IN.uv") + ")";
                expression = pinId == "alpha" ? sample + ".a" :
                    (pinId == "rgb" ? sample + ".rgb" : sample);
            }
        } else if (node->typeId == "material.math.add") {
            expression = "(" + Input(*node, "a", "float3(0.0, 0.0, 0.0)") + " + " +
                Input(*node, "b", "float3(0.0, 0.0, 0.0)") + ")";
        } else if (node->typeId == "material.math.multiply") {
            expression = "(" + Input(*node, "a", "float3(1.0, 1.0, 1.0)") + " * " +
                Input(*node, "b", "float3(1.0, 1.0, 1.0)") + ")";
        } else if (node->typeId == "material.math.lerp") {
            expression = "lerp(" + Input(*node, "a", "float3(0.0, 0.0, 0.0)") + ", " +
                Input(*node, "b", "float3(1.0, 1.0, 1.0)") + ", " +
                Input(*node, "alpha", "0.5") + ")";
        } else if (node->typeId == "material.normal.decode") {
            expression = "normalize(" + Input(*node, "value", "float3(0.5, 0.5, 1.0)") +
                " * 2.0 - 1.0)";
        } else {
            diagnostics.push_back({EditorGraphIssueSeverity::Error,
                "material.unsupported_node", node->id,
                "Material compiler does not support node type: " + node->typeId});
            expression = "0.0";
        }
        cache.emplace(key, expression);
        return expression;
    }
};

class EditorMaterialGraphUndoCommand final : public IEditorUndoCommand {
public:
    EditorMaterialGraphUndoCommand(
        EditorDocumentId document,
        EditorMaterialGraphAsset before,
        EditorMaterialGraphAsset after)
        : document_(std::move(document)), before_(std::move(before)), after_(std::move(after)) {}

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override {
        auto* service = dynamic_cast<EditorMaterialGraphService*>(
            context.Find(EditorMaterialGraphService::kServiceId));
        if (service == nullptr) {
            return EditorUndoResult::Failure(
                EditorErrorCode::MissingService,
                "Material Graph execution service is unavailable.");
        }
        std::string error;
        const EditorMaterialGraphAsset& asset =
            mode == EditorTransactionApplyMode::Undo ? before_ : after_;
        if (!service->PublishFromCommand(document_, asset, error)) {
            return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, std::move(error));
        }
        return EditorUndoResult::Success();
    }

    std::size_t EstimatedBytes() const noexcept override {
        const auto assetBytes = [](const EditorMaterialGraphAsset& asset) {
            std::size_t bytes = sizeof(asset) + asset.assetGuid.size() + asset.name.size();
            for (const EditorGraphNode& node : asset.graph.nodes) {
                bytes += sizeof(node) + node.id.size() + node.typeId.size() + node.label.size();
                for (const auto& [key, value] : node.properties) bytes += key.size() + value.size();
            }
            for (const EditorGraphLink& link : asset.graph.links) {
                bytes += sizeof(link) + link.id.size() + link.fromNodeId.size() +
                    link.fromPinId.size() + link.toNodeId.size() + link.toPinId.size();
            }
            return bytes;
        };
        return sizeof(*this) + document_.assetGuid.size() + document_.type.size() +
            assetBytes(before_) + assetBytes(after_);
    }

    std::string_view DomainId() const noexcept override { return "material-graph"; }
    std::string_view TypeId() const noexcept override { return "material-graph.snapshot"; }

private:
    EditorDocumentId document_;
    EditorMaterialGraphAsset before_;
    EditorMaterialGraphAsset after_;
};

} // namespace

EditorGraphSchema BuildEditorMaterialGraphSchema() {
    EditorGraphSchema schema;
    schema.RegisterNodeType({"material.output", "Material Output", "Material", {
        InputPin("baseColor", "Base Color", "float3"),
        InputPin("roughness", "Roughness", "float"),
        InputPin("metallic", "Metallic", "float"),
        InputPin("normal", "Normal", "float3"),
        InputPin("emissive", "Emissive", "float3"),
        InputPin("opacity", "Opacity", "float")}});
    schema.RegisterNodeType({"material.constant.scalar", "Scalar", "Constants", {
        OutputPin("value", "Value", "float")}});
    schema.RegisterNodeType({"material.constant.vector3", "Vector 3", "Constants", {
        OutputPin("value", "Value", "float3")}});
    schema.RegisterNodeType({"material.input.texcoord", "Texture Coordinate", "Inputs", {
        OutputPin("uv", "UV", "float2")}});
    schema.RegisterNodeType({"material.texture2d", "Texture Sample", "Textures", {
        InputPin("uv", "UV", "float2"),
        OutputPin("rgba", "RGBA", "float4"),
        OutputPin("rgb", "RGB", "float3"),
        OutputPin("alpha", "Alpha", "float")}});
    schema.RegisterNodeType({"material.math.add", "Add", "Math", {
        InputPin("a", "A", "float3", true), InputPin("b", "B", "float3", true),
        OutputPin("result", "Result", "float3")}});
    schema.RegisterNodeType({"material.math.multiply", "Multiply", "Math", {
        InputPin("a", "A", "float3", true), InputPin("b", "B", "float3", true),
        OutputPin("result", "Result", "float3")}});
    schema.RegisterNodeType({"material.math.lerp", "Lerp", "Math", {
        InputPin("a", "A", "float3", true), InputPin("b", "B", "float3", true),
        InputPin("alpha", "Alpha", "float", true), OutputPin("result", "Result", "float3")}});
    schema.RegisterNodeType({"material.normal.decode", "Decode Normal", "Normals", {
        InputPin("value", "Value", "float3", true), OutputPin("normal", "Normal", "float3")}});
    return schema;
}

EditorMaterialGraphAsset MakeDefaultEditorMaterialGraph(
    std::string assetGuid,
    std::string name) {
    EditorMaterialGraphAsset asset;
    asset.assetGuid = std::move(assetGuid);
    asset.name = name.empty() ? "Material" : std::move(name);

    EditorGraphNode output;
    output.id = MakeEditorGraphElementId("node");
    output.typeId = "material.output";
    output.label = "Material Output";
    output.positionX = 420.0f;

    EditorGraphNode color;
    color.id = MakeEditorGraphElementId("node");
    color.typeId = "material.constant.vector3";
    color.label = "Base Color";
    color.positionX = 40.0f;
    color.positionY = 20.0f;
    color.properties["value"] = "0.5, 0.5, 0.5";

    asset.graph.nodes = {output, color};
    asset.graph.links.push_back({MakeEditorGraphElementId("link"),
        color.id, "value", output.id, "baseColor"});
    asset.graph.revision = 1;
    asset.revision = 1;
    return asset;
}

EditorMaterialCompileArtifact CompileEditorMaterialGraph(
    const EditorMaterialGraphAsset& asset,
    const EditorGraphSchema& schema) {
    EditorMaterialCompileArtifact artifact;
    const EditorGraphValidationReport validation = ValidateEditorGraph(asset.graph, schema);
    for (const EditorGraphIssue& issue : validation.issues) {
        artifact.diagnostics.push_back({issue.severity, issue.code, issue.nodeId, issue.message});
    }
    const auto output = std::find_if(asset.graph.nodes.begin(), asset.graph.nodes.end(), [](const auto& node) {
        return node.typeId == "material.output";
    });
    const std::size_t outputCount = static_cast<std::size_t>(std::count_if(
        asset.graph.nodes.begin(), asset.graph.nodes.end(), [](const auto& node) {
            return node.typeId == "material.output";
        }));
    if (outputCount != 1) {
        artifact.diagnostics.push_back({EditorGraphIssueSeverity::Error,
            "material.output_count", {}, "Material Graph must contain exactly one Material Output node."});
    }
    if (validation.HasErrors() || output == asset.graph.nodes.end() || outputCount != 1) return artifact;

    ExpressionEmitter emitter{asset, schema};
    const std::string baseColor = emitter.Input(*output, "baseColor", "float3(0.5, 0.5, 0.5)");
    const std::string roughness = emitter.Input(*output, "roughness", "0.5");
    const std::string metallic = emitter.Input(*output, "metallic", "0.0");
    const std::string normal = emitter.Input(*output, "normal", "float3(0.0, 0.0, 1.0)");
    const std::string emissive = emitter.Input(*output, "emissive", "float3(0.0, 0.0, 0.0)");
    const std::string opacity = emitter.Input(*output, "opacity", "1.0");
    artifact.diagnostics.insert(
        artifact.diagnostics.end(), emitter.diagnostics.begin(), emitter.diagnostics.end());
    if (std::any_of(artifact.diagnostics.begin(), artifact.diagnostics.end(), [](const auto& diagnostic) {
            return diagnostic.severity == EditorGraphIssueSeverity::Error;
        })) {
        return artifact;
    }

    std::ostringstream source;
    source << "// Generated Material Graph artifact. Do not edit.\n";
    source << "// Asset " << asset.assetGuid << "\n";
    for (const EditorGraphNode& node : asset.graph.nodes) {
        if (node.typeId == "material.texture2d" && !Property(node, "assetGuid", {}).empty()) {
            source << "Texture2D MG_Texture_" << SafeIdentifier(node.id) << ";\n";
        }
    }
    source << "SamplerState MG_LinearSampler;\n"
           << "struct MaterialGraphInput { float2 uv; };\n"
           << "struct MaterialGraphResult { float3 baseColor; float roughness; float metallic; "
              "float3 normal; float3 emissive; float opacity; };\n"
           << "MaterialGraphResult EvaluateMaterialGraph(MaterialGraphInput IN) {\n"
           << "    MaterialGraphResult OUT;\n"
           << "    OUT.baseColor = " << baseColor << ";\n"
           << "    OUT.roughness = saturate(" << roughness << ");\n"
           << "    OUT.metallic = saturate(" << metallic << ");\n"
           << "    OUT.normal = normalize(" << normal << ");\n"
           << "    OUT.emissive = " << emissive << ";\n"
           << "    OUT.opacity = saturate(" << opacity << ");\n"
           << "    return OUT;\n}\n";
    artifact.hlslSource = source.str();
    artifact.sourceFingerprint = HashText(artifact.hlslSource);
    artifact.textureAssetGuids.assign(emitter.textureGuids.begin(), emitter.textureGuids.end());
    artifact.succeeded = true;
    return artifact;
}

void EditorMaterialGraphService::Bind(
    EditorMaterialGraphDocumentProvider* provider,
    EditorTransactionStack* transactions,
    EditorDocumentManager* documents) {
    provider_ = provider;
    transactions_ = transactions;
    documents_ = documents;
}

void EditorMaterialGraphService::SetActiveDocument(EditorDocumentId document) {
    activeDocument_ = std::move(document);
    if (const EditorMaterialGraphAsset* asset = ActiveAsset()) UpdateCompileArtifact(*asset);
}

EditorMaterialGraphAsset* EditorMaterialGraphService::ActiveAsset() {
    return provider_ != nullptr ? provider_->Asset(activeDocument_) : nullptr;
}

const EditorMaterialGraphAsset* EditorMaterialGraphService::ActiveAsset() const {
    return const_cast<EditorMaterialGraphService*>(this)->ActiveAsset();
}

bool EditorMaterialGraphService::ValidateConnection(
    const EditorMaterialGraphAsset& asset,
    std::string_view fromNodeId,
    std::string_view fromPinId,
    std::string_view toNodeId,
    std::string_view toPinId,
    std::string& errorMessage) const {
    const EditorGraphNode* from = FindEditorGraphNode(asset.graph, fromNodeId);
    const EditorGraphNode* to = FindEditorGraphNode(asset.graph, toNodeId);
    if (from == nullptr || to == nullptr) {
        errorMessage = "Material Graph connection references a missing node.";
        return false;
    }
    const EditorGraphPinDefinition* fromPin = schema_.FindPin(from->typeId, fromPinId);
    const EditorGraphPinDefinition* toPin = schema_.FindPin(to->typeId, toPinId);
    if (fromPin == nullptr || toPin == nullptr ||
        fromPin->direction != EditorGraphPinDirection::Output ||
        toPin->direction != EditorGraphPinDirection::Input) {
        errorMessage = "Material Graph connection pin is unavailable or has the wrong direction.";
        return false;
    }
    if (!schema_.CanConnect(fromPin->typeId, toPin->typeId)) {
        errorMessage = "Material Graph pin types are incompatible: " + fromPin->typeId +
            " -> " + toPin->typeId + ".";
        return false;
    }
    if (EditorGraphWouldCreateCycle(asset.graph, fromNodeId, toNodeId)) {
        errorMessage = "Material Graph connection would create a cycle.";
        return false;
    }
    return true;
}

bool EditorMaterialGraphService::AddNode(
    std::string_view nodeTypeId,
    float positionX,
    float positionY,
    std::string* createdNodeId,
    std::string& errorMessage) {
    EditorMaterialGraphAsset* asset = ActiveAsset();
    const EditorGraphNodeTypeDefinition* type = schema_.FindNodeType(nodeTypeId);
    if (asset == nullptr || type == nullptr) {
        errorMessage = "Active Material Graph or requested node type is unavailable.";
        return false;
    }
    if (asset->graph.nodes.size() >= kEditorGraphMaxNodes) {
        errorMessage = "Material Graph node safety limit reached.";
        return false;
    }
    EditorMaterialGraphAsset before = *asset;
    EditorMaterialGraphAsset after = before;
    EditorGraphNode node;
    node.id = MakeEditorGraphElementId("node");
    node.typeId = type->typeId;
    node.label = type->displayName;
    node.positionX = positionX;
    node.positionY = positionY;
    if (node.typeId == "material.constant.scalar") node.properties["value"] = "0.0";
    if (node.typeId == "material.constant.vector3") node.properties["value"] = "0.5, 0.5, 0.5";
    if (node.typeId == "material.texture2d") node.properties["assetGuid"] = "";
    if (node.typeId == "material.output" && std::any_of(after.graph.nodes.begin(), after.graph.nodes.end(),
            [](const auto& current) { return current.typeId == "material.output"; })) {
        errorMessage = "Material Graph already has a Material Output node.";
        return false;
    }
    const std::string id = node.id;
    after.graph.nodes.push_back(std::move(node));
    ++after.graph.revision;
    ++after.revision;
    if (!CommitMutation("Add Material Graph Node", std::move(before), std::move(after), errorMessage)) return false;
    if (createdNodeId != nullptr) *createdNodeId = id;
    return true;
}

bool EditorMaterialGraphService::RemoveNode(std::string_view nodeId, std::string& errorMessage) {
    EditorMaterialGraphAsset* asset = ActiveAsset();
    const EditorGraphNode* node = asset != nullptr ? FindEditorGraphNode(asset->graph, nodeId) : nullptr;
    if (node == nullptr) {
        errorMessage = "Material Graph node was not found.";
        return false;
    }
    if (node->typeId == "material.output") {
        errorMessage = "Material Output cannot be removed.";
        return false;
    }
    EditorMaterialGraphAsset before = *asset;
    EditorMaterialGraphAsset after = before;
    if (!RemoveEditorGraphNode(after.graph, nodeId)) return false;
    ++after.revision;
    return CommitMutation("Remove Material Graph Node", std::move(before), std::move(after), errorMessage);
}

bool EditorMaterialGraphService::Connect(
    std::string_view fromNodeId,
    std::string_view fromPinId,
    std::string_view toNodeId,
    std::string_view toPinId,
    std::string& errorMessage) {
    EditorMaterialGraphAsset* asset = ActiveAsset();
    if (asset == nullptr || !ValidateConnection(
            *asset, fromNodeId, fromPinId, toNodeId, toPinId, errorMessage)) return false;
    if (asset->graph.links.size() >= kEditorGraphMaxLinks) {
        errorMessage = "Material Graph link safety limit reached.";
        return false;
    }
    EditorMaterialGraphAsset before = *asset;
    EditorMaterialGraphAsset after = before;
    after.graph.links.erase(
        std::remove_if(after.graph.links.begin(), after.graph.links.end(), [&](const auto& link) {
            return link.toNodeId == toNodeId && link.toPinId == toPinId;
        }),
        after.graph.links.end());
    after.graph.links.push_back({MakeEditorGraphElementId("link"), std::string(fromNodeId),
        std::string(fromPinId), std::string(toNodeId), std::string(toPinId)});
    ++after.graph.revision;
    ++after.revision;
    return CommitMutation("Connect Material Graph Pins", std::move(before), std::move(after), errorMessage);
}

bool EditorMaterialGraphService::Disconnect(std::string_view linkId, std::string& errorMessage) {
    EditorMaterialGraphAsset* asset = ActiveAsset();
    if (asset == nullptr || FindEditorGraphLink(asset->graph, linkId) == nullptr) {
        errorMessage = "Material Graph link was not found.";
        return false;
    }
    EditorMaterialGraphAsset before = *asset;
    EditorMaterialGraphAsset after = before;
    after.graph.links.erase(std::remove_if(after.graph.links.begin(), after.graph.links.end(),
        [&](const auto& link) { return link.id == linkId; }), after.graph.links.end());
    ++after.graph.revision;
    ++after.revision;
    return CommitMutation("Disconnect Material Graph Pins", std::move(before), std::move(after), errorMessage);
}

bool EditorMaterialGraphService::SetNodeProperty(
    std::string_view nodeId,
    std::string key,
    std::string value,
    std::string& errorMessage) {
    EditorMaterialGraphAsset* asset = ActiveAsset();
    if (asset == nullptr || key.empty()) {
        errorMessage = "Active Material Graph or property key is unavailable.";
        return false;
    }
    EditorMaterialGraphAsset before = *asset;
    EditorMaterialGraphAsset after = before;
    EditorGraphNode* node = FindEditorGraphNode(after.graph, nodeId);
    if (node == nullptr) {
        errorMessage = "Material Graph node was not found.";
        return false;
    }
    if (node->properties[key] == value) return true;
    node->properties[std::move(key)] = std::move(value);
    ++after.graph.revision;
    ++after.revision;
    return CommitMutation("Edit Material Graph Property", std::move(before), std::move(after), errorMessage);
}

bool EditorMaterialGraphService::MoveNode(
    std::string_view nodeId,
    float positionX,
    float positionY,
    std::string& errorMessage) {
    if (!std::isfinite(positionX) || !std::isfinite(positionY)) {
        errorMessage = "Material Graph node position must be finite.";
        return false;
    }
    EditorMaterialGraphAsset* asset = ActiveAsset();
    if (asset == nullptr) {
        errorMessage = "Active Material Graph is unavailable.";
        return false;
    }
    EditorMaterialGraphAsset before = *asset;
    EditorMaterialGraphAsset after = before;
    EditorGraphNode* node = FindEditorGraphNode(after.graph, nodeId);
    if (node == nullptr) {
        errorMessage = "Material Graph node was not found.";
        return false;
    }
    if (node->positionX == positionX && node->positionY == positionY) return true;
    node->positionX = positionX;
    node->positionY = positionY;
    ++after.graph.revision;
    ++after.revision;
    return CommitMutation("Move Material Graph Node", std::move(before), std::move(after), errorMessage);
}

bool EditorMaterialGraphService::CommitMutation(
    std::string_view label,
    EditorMaterialGraphAsset before,
    EditorMaterialGraphAsset after,
    std::string& errorMessage) {
    if (provider_ == nullptr || transactions_ == nullptr || !activeDocument_.IsValid()) {
        errorMessage = "Material Graph document or transaction service is unavailable.";
        return false;
    }
    if (!provider_->Publish(activeDocument_, after)) {
        errorMessage = "Material Graph mutation could not be published.";
        return false;
    }
    EditorObjectHandle target;
    target.domain = EditorDomainId::MaterialGraphNode;
    target.stableId = activeDocument_.assetGuid;
    target.displayName = after.name;
    EditorError error;
    if (!transactions_->PushCommand(
            std::string(label), std::move(target),
            std::make_shared<EditorMaterialGraphUndoCommand>(activeDocument_, before, after), &error)) {
        provider_->Publish(activeDocument_, std::move(before));
        errorMessage = error.message;
        return false;
    }
    UpdateCompileArtifact(after);
    if (documents_ != nullptr) documents_->MarkDirty(activeDocument_, label);
    if (mutationCallback_) mutationCallback_(activeDocument_, label);
    return true;
}

bool EditorMaterialGraphService::PublishFromCommand(
    const EditorDocumentId& document,
    const EditorMaterialGraphAsset& asset,
    std::string& errorMessage) {
    if (provider_ == nullptr || !provider_->Publish(document, asset)) {
        errorMessage = "Material Graph transaction could not publish its snapshot.";
        return false;
    }
    if (document == activeDocument_) UpdateCompileArtifact(asset);
    if (documents_ != nullptr) documents_->MarkDirty(document, "Material Graph Undo/Redo");
    if (mutationCallback_) mutationCallback_(document, "Material Graph Undo/Redo");
    return true;
}

void EditorMaterialGraphService::UpdateCompileArtifact(const EditorMaterialGraphAsset& asset) {
    lastCompileArtifact_ = CompileEditorMaterialGraph(asset, schema_);
    if (lastCompileArtifact_.succeeded) lastSuccessfulArtifact_ = lastCompileArtifact_;
}

bool EditorMaterialGraphService::Recompile(std::string& errorMessage) {
    const EditorMaterialGraphAsset* asset = ActiveAsset();
    if (asset == nullptr) {
        errorMessage = "Active Material Graph is unavailable.";
        return false;
    }
    UpdateCompileArtifact(*asset);
    if (!lastCompileArtifact_.succeeded) {
        errorMessage = lastCompileArtifact_.diagnostics.empty()
            ? "Material Graph compile failed."
            : lastCompileArtifact_.diagnostics.front().message;
        return false;
    }
    return true;
}

const char* ToString(EditorMaterialDomain value) {
    switch (value) {
    case EditorMaterialDomain::Surface: return "Surface";
    case EditorMaterialDomain::PostProcess: return "PostProcess";
    }
    return "Surface";
}

const char* ToString(EditorMaterialBlendMode value) {
    switch (value) {
    case EditorMaterialBlendMode::Opaque: return "Opaque";
    case EditorMaterialBlendMode::Masked: return "Masked";
    case EditorMaterialBlendMode::Translucent: return "Translucent";
    }
    return "Opaque";
}

const char* ToString(EditorMaterialShadingModel value) {
    switch (value) {
    case EditorMaterialShadingModel::Lit: return "Lit";
    case EditorMaterialShadingModel::Unlit: return "Unlit";
    }
    return "Lit";
}

bool EditorMaterialDomainFromString(std::string_view value, EditorMaterialDomain& output) {
    if (value == "Surface") output = EditorMaterialDomain::Surface;
    else if (value == "PostProcess") output = EditorMaterialDomain::PostProcess;
    else return false;
    return true;
}

bool EditorMaterialBlendModeFromString(std::string_view value, EditorMaterialBlendMode& output) {
    if (value == "Opaque") output = EditorMaterialBlendMode::Opaque;
    else if (value == "Masked") output = EditorMaterialBlendMode::Masked;
    else if (value == "Translucent") output = EditorMaterialBlendMode::Translucent;
    else return false;
    return true;
}

bool EditorMaterialShadingModelFromString(std::string_view value, EditorMaterialShadingModel& output) {
    if (value == "Lit") output = EditorMaterialShadingModel::Lit;
    else if (value == "Unlit") output = EditorMaterialShadingModel::Unlit;
    else return false;
    return true;
}

} // namespace editor
