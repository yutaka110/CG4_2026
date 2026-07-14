#include "EditorAnimationStateMachineDocumentProvider.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {

constexpr std::size_t kMaxBytes = 64u * 1024u * 1024u;

bool ValidateStructure(const EditorAnimationStateMachineAsset& asset, std::string* error) {
    if (asset.assetGuid.empty() || asset.name.empty() ||
        asset.graph.nodes.size() > kEditorGraphMaxNodes ||
        asset.graph.links.size() > kEditorGraphMaxLinks ||
        asset.parameters.size() > kEditorAnimationStateMachineMaxParameters) {
        if (error != nullptr) *error = "Animation State Machine identity or safety limits are invalid.";
        return false;
    }
    std::unordered_set<std::string> nodeIds;
    for (const EditorGraphNode& node : asset.graph.nodes) {
        if (node.id.empty() || node.typeId.empty() || !nodeIds.insert(node.id).second ||
            !std::isfinite(node.positionX) || !std::isfinite(node.positionY)) return false;
    }
    std::unordered_set<std::string> linkIds;
    for (const EditorGraphLink& link : asset.graph.links) {
        if (link.id.empty() || !linkIds.insert(link.id).second ||
            nodeIds.find(link.fromNodeId) == nodeIds.end() ||
            nodeIds.find(link.toNodeId) == nodeIds.end()) return false;
    }
    return true;
}

} // namespace

bool EditorAnimationStateMachineDocumentProvider::SupportsPath(
    const std::filesystem::path& path) const {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".animsm" || extension == ".animstate";
}

bool EditorAnimationStateMachineDocumentProvider::ReadSource(const std::filesystem::path& path,
    EditorDocumentContent* content, std::string* errorMessage) const {
    if (content == nullptr) return false;
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        std::error_code error;
        if (std::filesystem::exists(path, error)) return false;
        return Encode(MakeDefaultEditorAnimationStateMachine(
            MakeEditorDocumentGuid(EditorDocumentTypes::AnimationStateMachine, path),
            path.stem().string()), content, errorMessage);
    }
    content->bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (content->bytes.size() > kMaxBytes) return false;
    std::istringstream header(std::string(content->bytes.begin(), content->bytes.end()));
    std::string marker;
    content->schemaVersion = 0;
    if (!(header >> marker >> content->schemaVersion) || marker != "ANIMATION_STATE_MACHINE") {
        if (errorMessage != nullptr) *errorMessage = "Animation State Machine header is invalid.";
        return false;
    }
    return true;
}

bool EditorAnimationStateMachineDocumentProvider::Serialize(const EditorDocumentId& id,
    EditorDocumentContent* content, std::string* errorMessage) const {
    const auto* asset = Asset(id);
    return asset != nullptr && Encode(*asset, content, errorMessage);
}

bool EditorAnimationStateMachineDocumentProvider::Deserialize(const EditorDocumentId& id,
    const EditorDocumentContent& content, std::string* errorMessage) {
    EditorAnimationStateMachineAsset decoded;
    if (!Decode(content, &decoded, errorMessage)) return false;
    decoded.revision = Asset(id) != nullptr ? Asset(id)->revision + 1 : 1;
    assets_[id.Key()] = std::move(decoded);
    return true;
}

EditorDocumentValidationReport EditorAnimationStateMachineDocumentProvider::Validate(
    const EditorDocumentContent& content) const {
    EditorDocumentValidationReport report;
    EditorAnimationStateMachineAsset asset;
    std::string error;
    if (!Decode(content, &asset, &error)) {
        report.issues.push_back({EditorDocumentIssueSeverity::Error,
            "animation_state_machine.parse", std::move(error)});
        return report;
    }
    const auto artifact = CompileEditorAnimationStateMachine(
        asset, BuildEditorAnimationStateMachineSchema());
    for (const auto& diagnostic : artifact.diagnostics) {
        report.issues.push_back({EditorDocumentIssueSeverity::Warning,
            diagnostic.code, diagnostic.message});
    }
    return report;
}

bool EditorAnimationStateMachineDocumentProvider::Migrate(const EditorDocumentContent& source,
    EditorDocumentContent* migrated, EditorDocumentMigrationReport* report,
    std::string* errorMessage) const {
    if (migrated == nullptr || source.schemaVersion == 0 ||
        source.schemaVersion > kEditorAnimationStateMachineSchemaVersion) return false;
    EditorAnimationStateMachineAsset asset;
    if (!Decode(source, &asset, errorMessage) || !Encode(asset, migrated, errorMessage)) return false;
    if (report != nullptr) {
        report->migrated = source.schemaVersion != kEditorAnimationStateMachineSchemaVersion;
        report->sourceSchemaVersion = source.schemaVersion;
        report->targetSchemaVersion = kEditorAnimationStateMachineSchemaVersion;
        if (report->migrated) report->notes.push_back(
            "Animation State Machine v1 gained typed parameters and explicit transition settings.");
    }
    return true;
}

void EditorAnimationStateMachineDocumentProvider::Release(const EditorDocumentId& id) {
    assets_.erase(id.Key());
}

EditorAnimationStateMachineAsset* EditorAnimationStateMachineDocumentProvider::Asset(
    const EditorDocumentId& id) {
    const auto found = assets_.find(id.Key());
    return found == assets_.end() ? nullptr : &found->second;
}

const EditorAnimationStateMachineAsset* EditorAnimationStateMachineDocumentProvider::Asset(
    const EditorDocumentId& id) const {
    return const_cast<EditorAnimationStateMachineDocumentProvider*>(this)->Asset(id);
}

bool EditorAnimationStateMachineDocumentProvider::Publish(const EditorDocumentId& id,
    EditorAnimationStateMachineAsset asset) {
    std::string error;
    if (!id.IsValid() || id.type != EditorDocumentTypes::AnimationStateMachine ||
        !ValidateStructure(asset, &error)) return false;
    assets_[id.Key()] = std::move(asset);
    return true;
}

bool EditorAnimationStateMachineDocumentProvider::Encode(
    const EditorAnimationStateMachineAsset& asset, EditorDocumentContent* content,
    std::string* errorMessage) {
    if (content == nullptr || !ValidateStructure(asset, errorMessage)) return false;
    std::ostringstream output;
    output << "ANIMATION_STATE_MACHINE " << kEditorAnimationStateMachineSchemaVersion << '\n';
    output << "ASSET " << std::quoted(asset.assetGuid) << ' ' << std::quoted(asset.name) << '\n';
    for (const AnimationStateMachineParameter& parameter : asset.parameters) {
        output << "PARAMETER " << std::quoted(parameter.name) << ' ' << ToString(parameter.type)
               << ' ' << std::setprecision(9) << parameter.defaultValue << '\n';
    }
    output << "GRAPH " << asset.graph.revision << '\n';
    for (const EditorGraphNode& node : asset.graph.nodes) {
        output << "NODE " << std::quoted(node.id) << ' ' << std::quoted(node.typeId) << ' '
               << std::quoted(node.label) << ' ' << std::setprecision(9) << node.positionX << ' '
               << node.positionY << ' ' << node.properties.size() << '\n';
        for (const auto& [key, value] : node.properties) {
            output << "PROPERTY " << std::quoted(key) << ' ' << std::quoted(value) << '\n';
        }
    }
    for (const EditorGraphLink& link : asset.graph.links) {
        output << "LINK " << std::quoted(link.id) << ' ' << std::quoted(link.fromNodeId) << ' '
               << std::quoted(link.fromPinId) << ' ' << std::quoted(link.toNodeId) << ' '
               << std::quoted(link.toPinId) << '\n';
    }
    output << "END\n";
    const std::string bytes = output.str();
    content->schemaVersion = kEditorAnimationStateMachineSchemaVersion;
    content->bytes.assign(bytes.begin(), bytes.end());
    return true;
}

bool EditorAnimationStateMachineDocumentProvider::Decode(const EditorDocumentContent& content,
    EditorAnimationStateMachineAsset* asset, std::string* errorMessage) {
    if (asset == nullptr || content.bytes.empty() || content.bytes.size() > kMaxBytes) return false;
    std::istringstream input(std::string(content.bytes.begin(), content.bytes.end()));
    std::string line;
    std::getline(input, line);
    std::istringstream header(line);
    std::string marker;
    uint32_t schema = 0;
    if (!(header >> marker >> schema) || marker != "ANIMATION_STATE_MACHINE" || schema == 0 ||
        schema > kEditorAnimationStateMachineSchemaVersion) return false;
    EditorAnimationStateMachineAsset decoded;
    bool hasAsset = false;
    bool hasEnd = false;
    EditorGraphNode* currentNode = nullptr;
    std::size_t remainingProperties = 0;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string kind;
        row >> kind;
        if (kind == "ASSET") {
            if (!(row >> std::quoted(decoded.assetGuid) >> std::quoted(decoded.name))) return false;
            hasAsset = true;
        } else if (kind == "PARAMETER") {
            AnimationStateMachineParameter parameter;
            std::string type;
            if (!(row >> std::quoted(parameter.name) >> type >> parameter.defaultValue) ||
                !AnimationParameterTypeFromString(type, parameter.type) ||
                decoded.parameters.size() >= kEditorAnimationStateMachineMaxParameters) return false;
            decoded.parameters.push_back(std::move(parameter));
        } else if (kind == "GRAPH") {
            if (!(row >> decoded.graph.revision)) decoded.graph.revision = 0;
        } else if (kind == "NODE") {
            if (remainingProperties != 0 || decoded.graph.nodes.size() >= kEditorGraphMaxNodes) return false;
            EditorGraphNode node;
            if (!(row >> std::quoted(node.id) >> std::quoted(node.typeId) >> std::quoted(node.label)
                  >> node.positionX >> node.positionY >> remainingProperties)) return false;
            decoded.graph.nodes.push_back(std::move(node));
            currentNode = &decoded.graph.nodes.back();
        } else if (kind == "PROPERTY") {
            std::string key;
            std::string value;
            if (currentNode == nullptr || remainingProperties == 0 ||
                !(row >> std::quoted(key) >> std::quoted(value))) return false;
            currentNode->properties[std::move(key)] = std::move(value);
            --remainingProperties;
        } else if (kind == "LINK") {
            EditorGraphLink link;
            if (remainingProperties != 0 || !(row >> std::quoted(link.id) >>
                std::quoted(link.fromNodeId) >> std::quoted(link.fromPinId) >>
                std::quoted(link.toNodeId) >> std::quoted(link.toPinId))) return false;
            decoded.graph.links.push_back(std::move(link));
            currentNode = nullptr;
        } else if (kind == "END") {
            if (remainingProperties != 0) return false;
            hasEnd = true;
            break;
        } else return false;
    }
    decoded.schemaVersion = kEditorAnimationStateMachineSchemaVersion;
    if (!hasAsset || !hasEnd || !ValidateStructure(decoded, errorMessage)) return false;
    *asset = std::move(decoded);
    return true;
}

} // namespace editor
