#include "EditorMaterialGraphDocumentProvider.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

void AddIssue(
    EditorDocumentValidationReport& report,
    EditorDocumentIssueSeverity severity,
    std::string code,
    std::string message) {
    report.issues.push_back({severity, std::move(code), std::move(message)});
}

bool ValidateAssetStructure(const EditorMaterialGraphAsset& asset, std::string* errorMessage) {
    if (asset.assetGuid.empty() || asset.name.empty()) {
        if (errorMessage != nullptr) *errorMessage = "Material Graph asset identity is empty.";
        return false;
    }
    if (asset.graph.nodes.size() > kEditorGraphMaxNodes ||
        asset.graph.links.size() > kEditorGraphMaxLinks) {
        if (errorMessage != nullptr) *errorMessage = "Material Graph exceeds its safety limits.";
        return false;
    }
    std::unordered_set<std::string> nodeIds;
    for (const EditorGraphNode& node : asset.graph.nodes) {
        if (node.id.empty() || node.typeId.empty() || !nodeIds.insert(node.id).second ||
            !std::isfinite(node.positionX) || !std::isfinite(node.positionY)) {
            if (errorMessage != nullptr) *errorMessage = "Material Graph contains an invalid node record.";
            return false;
        }
    }
    std::unordered_set<std::string> linkIds;
    for (const EditorGraphLink& link : asset.graph.links) {
        if (link.id.empty() || !linkIds.insert(link.id).second ||
            nodeIds.find(link.fromNodeId) == nodeIds.end() ||
            nodeIds.find(link.toNodeId) == nodeIds.end() ||
            link.fromPinId.empty() || link.toPinId.empty()) {
            if (errorMessage != nullptr) *errorMessage = "Material Graph contains an invalid link record.";
            return false;
        }
    }
    return true;
}

} // namespace

bool EditorMaterialGraphDocumentProvider::SupportsPath(const std::filesystem::path& path) const {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".material" || extension == ".materialgraph";
}

bool EditorMaterialGraphDocumentProvider::ReadSource(
    const std::filesystem::path& path,
    EditorDocumentContent* content,
    std::string* errorMessage) const {
    if (content == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Material Graph content output is null.";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        std::error_code error;
        if (std::filesystem::exists(path, error)) {
            if (errorMessage != nullptr) *errorMessage = "Could not open Material Graph: " + path.string();
            return false;
        }
        return Encode(MakeDefaultEditorMaterialGraph(
            MakeEditorDocumentGuid(EditorDocumentTypes::MaterialGraph, path), path.stem().string()),
            content, errorMessage);
    }
    content->bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (content->bytes.size() > 64u * 1024u * 1024u) {
        if (errorMessage != nullptr) *errorMessage = "Material Graph exceeds the 64 MiB safety limit.";
        return false;
    }
    std::istringstream header(std::string(content->bytes.begin(), content->bytes.end()));
    std::string marker;
    content->schemaVersion = 0;
    if (!(header >> marker >> content->schemaVersion) || marker != "MATERIAL_GRAPH") {
        if (errorMessage != nullptr) *errorMessage = "Material Graph header is invalid.";
        return false;
    }
    return true;
}

bool EditorMaterialGraphDocumentProvider::Serialize(
    const EditorDocumentId& id,
    EditorDocumentContent* content,
    std::string* errorMessage) const {
    const EditorMaterialGraphAsset* asset = Asset(id);
    if (asset == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Material Graph live model is unavailable.";
        return false;
    }
    return Encode(*asset, content, errorMessage);
}

bool EditorMaterialGraphDocumentProvider::Deserialize(
    const EditorDocumentId& id,
    const EditorDocumentContent& content,
    std::string* errorMessage) {
    EditorMaterialGraphAsset decoded;
    if (!Decode(content, &decoded, errorMessage)) return false;
    decoded.revision = Asset(id) != nullptr ? Asset(id)->revision + 1 : 1;
    assets_[id.Key()] = std::move(decoded);
    return true;
}

EditorDocumentValidationReport EditorMaterialGraphDocumentProvider::Validate(
    const EditorDocumentContent& content) const {
    EditorDocumentValidationReport report;
    if (content.bytes.size() > 64u * 1024u * 1024u) {
        AddIssue(report, EditorDocumentIssueSeverity::Error, "material_graph.size",
            "Material Graph exceeds the 64 MiB safety limit.");
        return report;
    }
    EditorMaterialGraphAsset asset;
    std::string error;
    if (!Decode(content, &asset, &error)) {
        AddIssue(report, EditorDocumentIssueSeverity::Error, "material_graph.parse", std::move(error));
        return report;
    }
    const EditorMaterialCompileArtifact artifact =
        CompileEditorMaterialGraph(asset, BuildEditorMaterialGraphSchema());
    for (const EditorMaterialCompileDiagnostic& diagnostic : artifact.diagnostics) {
        // Incomplete graphs remain saveable; compile diagnostics are authoring warnings.
        AddIssue(report, EditorDocumentIssueSeverity::Warning,
            diagnostic.code, diagnostic.message);
    }
    return report;
}

bool EditorMaterialGraphDocumentProvider::Migrate(
    const EditorDocumentContent& source,
    EditorDocumentContent* migrated,
    EditorDocumentMigrationReport* report,
    std::string* errorMessage) const {
    if (migrated == nullptr || source.schemaVersion == 0 ||
        source.schemaVersion > kEditorMaterialGraphSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "Material Graph has no compatible migration path.";
        return false;
    }
    EditorMaterialGraphAsset asset;
    if (!Decode(source, &asset, errorMessage) || !Encode(asset, migrated, errorMessage)) return false;
    if (report != nullptr) {
        report->migrated = source.schemaVersion != kEditorMaterialGraphSchemaVersion;
        report->sourceSchemaVersion = source.schemaVersion;
        report->targetSchemaVersion = kEditorMaterialGraphSchemaVersion;
        if (report->migrated) {
            report->notes.push_back(
                "Material Graph v1 gained explicit domain, blend-mode, and shading-model settings.");
        }
    }
    return true;
}

void EditorMaterialGraphDocumentProvider::Release(const EditorDocumentId& id) {
    assets_.erase(id.Key());
}

EditorMaterialGraphAsset* EditorMaterialGraphDocumentProvider::Asset(const EditorDocumentId& id) {
    const auto found = assets_.find(id.Key());
    return found == assets_.end() ? nullptr : &found->second;
}

const EditorMaterialGraphAsset* EditorMaterialGraphDocumentProvider::Asset(
    const EditorDocumentId& id) const {
    return const_cast<EditorMaterialGraphDocumentProvider*>(this)->Asset(id);
}

EditorDocumentId EditorMaterialGraphDocumentProvider::DocumentForAssetGuid(
    std::string_view assetGuid) const {
    for (const auto& [key, asset] : assets_) {
        if (asset.assetGuid != assetGuid) continue;
        const std::string prefix = std::string(EditorDocumentTypes::MaterialGraph) + ":";
        if (key.rfind(prefix, 0) == 0) {
            return {key.substr(prefix.size()), std::string(EditorDocumentTypes::MaterialGraph)};
        }
    }
    return {};
}

bool EditorMaterialGraphDocumentProvider::Publish(
    const EditorDocumentId& id,
    EditorMaterialGraphAsset asset) {
    std::string error;
    if (!id.IsValid() || id.type != EditorDocumentTypes::MaterialGraph ||
        !ValidateAssetStructure(asset, &error)) return false;
    assets_[id.Key()] = std::move(asset);
    return true;
}

bool EditorMaterialGraphDocumentProvider::Encode(
    const EditorMaterialGraphAsset& asset,
    EditorDocumentContent* content,
    std::string* errorMessage) {
    if (content == nullptr || !ValidateAssetStructure(asset, errorMessage)) return false;
    std::ostringstream output;
    output << "MATERIAL_GRAPH " << kEditorMaterialGraphSchemaVersion << '\n';
    output << "ASSET " << std::quoted(asset.assetGuid) << ' ' << std::quoted(asset.name) << '\n';
    output << "SETTINGS " << ToString(asset.domain) << ' ' << ToString(asset.blendMode) << ' '
           << ToString(asset.shadingModel) << '\n';
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
    content->schemaVersion = kEditorMaterialGraphSchemaVersion;
    content->bytes.assign(bytes.begin(), bytes.end());
    return true;
}

bool EditorMaterialGraphDocumentProvider::Decode(
    const EditorDocumentContent& content,
    EditorMaterialGraphAsset* asset,
    std::string* errorMessage) {
    if (asset == nullptr || content.bytes.empty() || content.bytes.size() > 64u * 1024u * 1024u) {
        if (errorMessage != nullptr) *errorMessage = "Material Graph content is empty or too large.";
        return false;
    }
    std::istringstream input(std::string(content.bytes.begin(), content.bytes.end()));
    std::string line;
    if (!std::getline(input, line)) return false;
    std::istringstream header(line);
    std::string marker;
    uint32_t schema = 0;
    if (!(header >> marker >> schema) || marker != "MATERIAL_GRAPH" || schema == 0 ||
        schema > kEditorMaterialGraphSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "Material Graph schema is unsupported.";
        return false;
    }
    EditorMaterialGraphAsset decoded;
    bool hasAsset = false;
    bool hasSettings = schema == 1;
    bool hasEnd = false;
    EditorGraphNode* currentNode = nullptr;
    std::size_t remainingProperties = 0;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string kind;
        row >> kind;
        if (kind == "ASSET") {
            if (!(row >> std::quoted(decoded.assetGuid) >> std::quoted(decoded.name))) {
                if (errorMessage != nullptr) *errorMessage = "Material Graph ASSET record is invalid.";
                return false;
            }
            hasAsset = true;
        } else if (kind == "SETTINGS") {
            std::string domain;
            std::string blend;
            std::string shading;
            if (!(row >> domain >> blend >> shading) ||
                !EditorMaterialDomainFromString(domain, decoded.domain) ||
                !EditorMaterialBlendModeFromString(blend, decoded.blendMode) ||
                !EditorMaterialShadingModelFromString(shading, decoded.shadingModel)) {
                if (errorMessage != nullptr) *errorMessage = "Material Graph SETTINGS record is invalid.";
                return false;
            }
            hasSettings = true;
        } else if (kind == "GRAPH") {
            if (!(row >> decoded.graph.revision)) decoded.graph.revision = 0;
        } else if (kind == "NODE") {
            if (remainingProperties != 0 || decoded.graph.nodes.size() >= kEditorGraphMaxNodes) {
                if (errorMessage != nullptr) {
                    *errorMessage = remainingProperties != 0
                        ? "Material Graph node properties are incomplete."
                        : "Material Graph node safety limit exceeded.";
                }
                return false;
            }
            EditorGraphNode node;
            if (!(row >> std::quoted(node.id) >> std::quoted(node.typeId) >> std::quoted(node.label) >>
                  node.positionX >> node.positionY >> remainingProperties)) {
                if (errorMessage != nullptr) *errorMessage = "Material Graph NODE record is invalid.";
                return false;
            }
            decoded.graph.nodes.push_back(std::move(node));
            currentNode = &decoded.graph.nodes.back();
        } else if (kind == "PROPERTY") {
            std::string key;
            std::string value;
            if (currentNode == nullptr || remainingProperties == 0 ||
                !(row >> std::quoted(key) >> std::quoted(value))) {
                if (errorMessage != nullptr) *errorMessage = "Material Graph PROPERTY record is invalid.";
                return false;
            }
            currentNode->properties[std::move(key)] = std::move(value);
            --remainingProperties;
        } else if (kind == "LINK") {
            if (remainingProperties != 0 || decoded.graph.links.size() >= kEditorGraphMaxLinks) {
                if (errorMessage != nullptr) *errorMessage = "Material Graph property/link record is incomplete.";
                return false;
            }
            EditorGraphLink link;
            if (!(row >> std::quoted(link.id) >> std::quoted(link.fromNodeId) >>
                  std::quoted(link.fromPinId) >> std::quoted(link.toNodeId) >>
                  std::quoted(link.toPinId))) {
                if (errorMessage != nullptr) *errorMessage = "Material Graph LINK record is invalid.";
                return false;
            }
            decoded.graph.links.push_back(std::move(link));
            currentNode = nullptr;
        } else if (kind == "END") {
            if (remainingProperties != 0) {
                if (errorMessage != nullptr) *errorMessage = "Material Graph node properties are incomplete.";
                return false;
            }
            hasEnd = true;
            break;
        } else {
            if (errorMessage != nullptr) *errorMessage = "Unknown Material Graph record: " + kind;
            return false;
        }
    }
    decoded.schemaVersion = kEditorMaterialGraphSchemaVersion;
    if (!hasAsset || !hasSettings || !hasEnd || !ValidateAssetStructure(decoded, errorMessage)) {
        if (errorMessage != nullptr && errorMessage->empty()) {
            *errorMessage = "Material Graph is missing required ASSET, SETTINGS, or END records.";
        }
        return false;
    }
    *asset = std::move(decoded);
    return true;
}

} // namespace editor
