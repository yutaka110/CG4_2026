#include "EditorVfxGraphDocumentProvider.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {

constexpr std::size_t kMaxVfxGraphBytes = 64u * 1024u * 1024u;

bool ValidateStructure(const EditorVfxGraphAsset& asset, std::string* errorMessage) {
    if (asset.assetGuid.empty() || asset.name.empty() ||
        asset.graph.nodes.size() > kEditorGraphMaxNodes ||
        asset.graph.links.size() > kEditorGraphMaxLinks ||
        asset.maxParticles == 0 || asset.maxParticles > 16u * 1024u * 1024u ||
        !std::isfinite(asset.fixedTimeStep) || asset.fixedTimeStep <= 0.0f ||
        asset.fixedTimeStep > 0.1f) {
        if (errorMessage != nullptr) *errorMessage = "VFX Graph identity, settings, or safety limits are invalid.";
        return false;
    }
    std::unordered_set<std::string> nodes;
    for (const EditorGraphNode& node : asset.graph.nodes) {
        if (node.id.empty() || node.typeId.empty() || !nodes.insert(node.id).second ||
            !std::isfinite(node.positionX) || !std::isfinite(node.positionY)) {
            if (errorMessage != nullptr) *errorMessage = "VFX Graph contains an invalid node record.";
            return false;
        }
    }
    std::unordered_set<std::string> links;
    for (const EditorGraphLink& link : asset.graph.links) {
        if (link.id.empty() || !links.insert(link.id).second ||
            nodes.find(link.fromNodeId) == nodes.end() || nodes.find(link.toNodeId) == nodes.end() ||
            link.fromPinId.empty() || link.toPinId.empty()) {
            if (errorMessage != nullptr) *errorMessage = "VFX Graph contains an invalid link record.";
            return false;
        }
    }
    return true;
}

void AddIssue(EditorDocumentValidationReport& report, EditorDocumentIssueSeverity severity,
    std::string code, std::string message) {
    report.issues.push_back({severity, std::move(code), std::move(message)});
}

} // namespace

bool EditorVfxGraphDocumentProvider::SupportsPath(const std::filesystem::path& path) const {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".vfxgraph" || extension == ".vfxsystem";
}

bool EditorVfxGraphDocumentProvider::ReadSource(const std::filesystem::path& path,
    EditorDocumentContent* content, std::string* errorMessage) const {
    if (content == nullptr) return false;
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        std::error_code error;
        if (std::filesystem::exists(path, error)) {
            if (errorMessage != nullptr) *errorMessage = "Could not open VFX Graph: " + path.string();
            return false;
        }
        return Encode(MakeDefaultEditorVfxGraph(
            MakeEditorDocumentGuid(EditorDocumentTypes::VfxGraph, path), path.stem().string()),
            content, errorMessage);
    }
    content->bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (content->bytes.size() > kMaxVfxGraphBytes) {
        if (errorMessage != nullptr) *errorMessage = "VFX Graph exceeds the 64 MiB safety limit.";
        return false;
    }
    std::istringstream header(std::string(content->bytes.begin(), content->bytes.end()));
    std::string marker;
    content->schemaVersion = 0;
    if (!(header >> marker >> content->schemaVersion) || marker != "VFX_GRAPH") {
        if (errorMessage != nullptr) *errorMessage = "VFX Graph header is invalid.";
        return false;
    }
    return true;
}

bool EditorVfxGraphDocumentProvider::Serialize(const EditorDocumentId& id,
    EditorDocumentContent* content, std::string* errorMessage) const {
    const EditorVfxGraphAsset* asset = Asset(id);
    if (asset == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "VFX Graph live model is unavailable.";
        return false;
    }
    return Encode(*asset, content, errorMessage);
}

bool EditorVfxGraphDocumentProvider::Deserialize(const EditorDocumentId& id,
    const EditorDocumentContent& content, std::string* errorMessage) {
    EditorVfxGraphAsset decoded;
    if (!Decode(content, &decoded, errorMessage)) return false;
    decoded.revision = Asset(id) != nullptr ? Asset(id)->revision + 1 : 1;
    assets_[id.Key()] = std::move(decoded);
    return true;
}

EditorDocumentValidationReport EditorVfxGraphDocumentProvider::Validate(
    const EditorDocumentContent& content) const {
    EditorDocumentValidationReport report;
    if (content.bytes.size() > kMaxVfxGraphBytes) {
        AddIssue(report, EditorDocumentIssueSeverity::Error, "vfx_graph.size",
            "VFX Graph exceeds the 64 MiB safety limit.");
        return report;
    }
    EditorVfxGraphAsset asset;
    std::string error;
    if (!Decode(content, &asset, &error)) {
        AddIssue(report, EditorDocumentIssueSeverity::Error, "vfx_graph.parse", std::move(error));
        return report;
    }
    const EditorVfxCompileArtifact artifact = CompileEditorVfxGraph(asset, BuildEditorVfxGraphSchema());
    for (const EditorVfxCompileDiagnostic& diagnostic : artifact.diagnostics) {
        AddIssue(report, EditorDocumentIssueSeverity::Warning, diagnostic.code, diagnostic.message);
    }
    return report;
}

bool EditorVfxGraphDocumentProvider::Migrate(const EditorDocumentContent& source,
    EditorDocumentContent* migrated, EditorDocumentMigrationReport* report,
    std::string* errorMessage) const {
    if (migrated == nullptr || source.schemaVersion == 0 ||
        source.schemaVersion > kEditorVfxGraphSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "VFX Graph has no compatible migration path.";
        return false;
    }
    EditorVfxGraphAsset asset;
    if (!Decode(source, &asset, errorMessage) || !Encode(asset, migrated, errorMessage)) return false;
    if (report != nullptr) {
        report->migrated = source.schemaVersion != kEditorVfxGraphSchemaVersion;
        report->sourceSchemaVersion = source.schemaVersion;
        report->targetSchemaVersion = kEditorVfxGraphSchemaVersion;
        if (report->migrated) {
            report->notes.push_back("VFX Graph v1 gained explicit simulation target, capacity, and fixed time step settings.");
        }
    }
    return true;
}

void EditorVfxGraphDocumentProvider::Release(const EditorDocumentId& id) { assets_.erase(id.Key()); }

EditorVfxGraphAsset* EditorVfxGraphDocumentProvider::Asset(const EditorDocumentId& id) {
    const auto found = assets_.find(id.Key());
    return found == assets_.end() ? nullptr : &found->second;
}

const EditorVfxGraphAsset* EditorVfxGraphDocumentProvider::Asset(const EditorDocumentId& id) const {
    return const_cast<EditorVfxGraphDocumentProvider*>(this)->Asset(id);
}

EditorDocumentId EditorVfxGraphDocumentProvider::DocumentForAssetGuid(std::string_view guid) const {
    for (const auto& [key, asset] : assets_) {
        if (asset.assetGuid != guid) continue;
        const std::string prefix = std::string(EditorDocumentTypes::VfxGraph) + ":";
        if (key.rfind(prefix, 0) == 0) return {key.substr(prefix.size()), std::string(EditorDocumentTypes::VfxGraph)};
    }
    return {};
}

bool EditorVfxGraphDocumentProvider::Publish(const EditorDocumentId& id, EditorVfxGraphAsset asset) {
    std::string error;
    if (!id.IsValid() || id.type != EditorDocumentTypes::VfxGraph ||
        !ValidateStructure(asset, &error)) return false;
    assets_[id.Key()] = std::move(asset);
    return true;
}

bool EditorVfxGraphDocumentProvider::Encode(const EditorVfxGraphAsset& asset,
    EditorDocumentContent* content, std::string* errorMessage) {
    if (content == nullptr || !ValidateStructure(asset, errorMessage)) return false;
    std::ostringstream output;
    output << "VFX_GRAPH " << kEditorVfxGraphSchemaVersion << '\n';
    output << "ASSET " << std::quoted(asset.assetGuid) << ' ' << std::quoted(asset.name) << '\n';
    output << "SETTINGS " << ToString(asset.simulationTarget) << ' ' << asset.maxParticles << ' '
           << std::setprecision(9) << asset.fixedTimeStep << '\n';
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
    content->schemaVersion = kEditorVfxGraphSchemaVersion;
    content->bytes.assign(bytes.begin(), bytes.end());
    return true;
}

bool EditorVfxGraphDocumentProvider::Decode(const EditorDocumentContent& content,
    EditorVfxGraphAsset* asset, std::string* errorMessage) {
    if (asset == nullptr || content.bytes.empty() || content.bytes.size() > kMaxVfxGraphBytes) {
        if (errorMessage != nullptr) *errorMessage = "VFX Graph content is empty or too large.";
        return false;
    }
    std::istringstream input(std::string(content.bytes.begin(), content.bytes.end()));
    std::string line;
    std::getline(input, line);
    std::istringstream header(line);
    std::string marker;
    uint32_t schema = 0;
    if (!(header >> marker >> schema) || marker != "VFX_GRAPH" || schema == 0 ||
        schema > kEditorVfxGraphSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "VFX Graph schema is unsupported.";
        return false;
    }
    EditorVfxGraphAsset decoded;
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
            if (!(row >> std::quoted(decoded.assetGuid) >> std::quoted(decoded.name))) return false;
            hasAsset = true;
        } else if (kind == "SETTINGS") {
            std::string target;
            if (!(row >> target >> decoded.maxParticles >> decoded.fixedTimeStep) ||
                !EditorVfxSimulationTargetFromString(target, decoded.simulationTarget)) return false;
            hasSettings = true;
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
            if (remainingProperties != 0 || decoded.graph.links.size() >= kEditorGraphMaxLinks) return false;
            EditorGraphLink link;
            if (!(row >> std::quoted(link.id) >> std::quoted(link.fromNodeId) >>
                  std::quoted(link.fromPinId) >> std::quoted(link.toNodeId) >>
                  std::quoted(link.toPinId))) return false;
            decoded.graph.links.push_back(std::move(link));
            currentNode = nullptr;
        } else if (kind == "END") {
            if (remainingProperties != 0) return false;
            hasEnd = true;
            break;
        } else {
            if (errorMessage != nullptr) *errorMessage = "Unknown VFX Graph record: " + kind;
            return false;
        }
    }
    decoded.schemaVersion = kEditorVfxGraphSchemaVersion;
    if (!hasAsset || !hasSettings || !hasEnd || !ValidateStructure(decoded, errorMessage)) {
        if (errorMessage != nullptr && errorMessage->empty()) *errorMessage = "VFX Graph is incomplete.";
        return false;
    }
    *asset = std::move(decoded);
    return true;
}

} // namespace editor
