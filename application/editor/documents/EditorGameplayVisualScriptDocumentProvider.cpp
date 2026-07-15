#include "EditorGameplayVisualScriptDocumentProvider.h"

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

bool ValidateStructure(const EditorGameplayVisualScriptAsset& asset, std::string* error) {
    if (asset.assetGuid.empty() || asset.name.empty() || asset.instructionBudget == 0 ||
        asset.instructionBudget > 1'000'000 || asset.graph.nodes.size() > kEditorGraphMaxNodes ||
        asset.graph.links.size() > kEditorGraphMaxLinks ||
        asset.variables.size() > kEditorGameplayVisualScriptMaxVariables) {
        if (error != nullptr) *error = "Gameplay Visual Script identity or safety limits are invalid.";
        return false;
    }
    std::unordered_set<std::string> nodeIds;
    for (const auto& node : asset.graph.nodes) {
        if (node.id.empty() || node.typeId.empty() || !nodeIds.insert(node.id).second ||
            !std::isfinite(node.positionX) || !std::isfinite(node.positionY)) return false;
    }
    std::unordered_set<std::string> linkIds;
    for (const auto& link : asset.graph.links) {
        if (link.id.empty() || !linkIds.insert(link.id).second ||
            nodeIds.find(link.fromNodeId) == nodeIds.end() ||
            nodeIds.find(link.toNodeId) == nodeIds.end()) return false;
    }
    return true;
}

std::string EncodeValue(const GameplayValue& value) {
    std::ostringstream out;
    out << std::setprecision(9);
    switch (value.type) {
    case GameplayValueType::Bool: out << (value.boolValue ? "true" : "false"); break;
    case GameplayValueType::Float: out << value.floatValue; break;
    case GameplayValueType::Int: out << value.intValue; break;
    case GameplayValueType::String: return value.stringValue;
    }
    return out.str();
}

bool DecodeValue(GameplayValueType type, std::string_view text, GameplayValue& value) {
    std::istringstream input{std::string(text)};
    std::string extra;
    switch (type) {
    case GameplayValueType::Bool:
        if (text == "true" || text == "1") value = GameplayValue::Bool(true);
        else if (text == "false" || text == "0") value = GameplayValue::Bool(false);
        else return false;
        return true;
    case GameplayValueType::Float: {
        float parsed = 0.0f;
        if (!(input >> parsed) || !std::isfinite(parsed) || (input >> extra)) return false;
        value = GameplayValue::Float(parsed); return true;
    }
    case GameplayValueType::Int: {
        int32_t parsed = 0;
        if (!(input >> parsed) || (input >> extra)) return false;
        value = GameplayValue::Int(parsed); return true;
    }
    case GameplayValueType::String:
        value = GameplayValue::String(std::string(text)); return true;
    }
    return false;
}
} // namespace

bool EditorGameplayVisualScriptDocumentProvider::SupportsPath(
    const std::filesystem::path& path) const {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".gameplay" || extension == ".visualscript";
}

bool EditorGameplayVisualScriptDocumentProvider::ReadSource(const std::filesystem::path& path,
    EditorDocumentContent* content, std::string* errorMessage) const {
    if (content == nullptr) return false;
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        std::error_code error;
        if (std::filesystem::exists(path, error)) return false;
        return Encode(MakeDefaultEditorGameplayVisualScript(
            MakeEditorDocumentGuid(EditorDocumentTypes::GameplayVisualScript, path),
            path.stem().string()), content, errorMessage);
    }
    content->bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    if (content->bytes.size() > kMaxBytes) return false;
    std::istringstream header(std::string(content->bytes.begin(), content->bytes.end()));
    std::string marker;
    content->schemaVersion = 0;
    if (!(header >> marker >> content->schemaVersion) || marker != "GAMEPLAY_VISUAL_SCRIPT") {
        if (errorMessage != nullptr) *errorMessage = "Gameplay Visual Script header is invalid.";
        return false;
    }
    return true;
}

bool EditorGameplayVisualScriptDocumentProvider::Serialize(const EditorDocumentId& id,
    EditorDocumentContent* content, std::string* errorMessage) const {
    const auto* asset = Asset(id); return asset != nullptr && Encode(*asset, content, errorMessage);
}
bool EditorGameplayVisualScriptDocumentProvider::Deserialize(const EditorDocumentId& id,
    const EditorDocumentContent& content, std::string* errorMessage) {
    EditorGameplayVisualScriptAsset decoded;
    if (!Decode(content, &decoded, errorMessage)) return false;
    decoded.revision = Asset(id) != nullptr ? Asset(id)->revision + 1 : 1;
    assets_[id.Key()] = std::move(decoded); return true;
}
EditorDocumentValidationReport EditorGameplayVisualScriptDocumentProvider::Validate(
    const EditorDocumentContent& content) const {
    EditorDocumentValidationReport report;
    EditorGameplayVisualScriptAsset asset; std::string error;
    if (!Decode(content, &asset, &error)) {
        report.issues.push_back({EditorDocumentIssueSeverity::Error, "gameplay.parse", std::move(error)});
        return report;
    }
    const auto artifact = CompileEditorGameplayVisualScript(asset, BuildEditorGameplayVisualScriptSchema());
    for (const auto& diagnostic : artifact.diagnostics) report.issues.push_back({
        EditorDocumentIssueSeverity::Warning, diagnostic.code, diagnostic.message});
    return report;
}
bool EditorGameplayVisualScriptDocumentProvider::Migrate(const EditorDocumentContent& source,
    EditorDocumentContent* migrated, EditorDocumentMigrationReport* report,
    std::string* errorMessage) const {
    if (migrated == nullptr || source.schemaVersion == 0 ||
        source.schemaVersion > kEditorGameplayVisualScriptSchemaVersion) return false;
    EditorGameplayVisualScriptAsset asset;
    if (!Decode(source, &asset, errorMessage) || !Encode(asset, migrated, errorMessage)) return false;
    if (report != nullptr) {
        report->migrated = source.schemaVersion != kEditorGameplayVisualScriptSchemaVersion;
        report->sourceSchemaVersion = source.schemaVersion;
        report->targetSchemaVersion = kEditorGameplayVisualScriptSchemaVersion;
        if (report->migrated) report->notes.push_back(
            "Gameplay Visual Script v1 gained typed variables and an explicit execution budget.");
    }
    return true;
}
void EditorGameplayVisualScriptDocumentProvider::Release(const EditorDocumentId& id) { assets_.erase(id.Key()); }
EditorGameplayVisualScriptAsset* EditorGameplayVisualScriptDocumentProvider::Asset(const EditorDocumentId& id) {
    const auto found = assets_.find(id.Key()); return found == assets_.end() ? nullptr : &found->second;
}
const EditorGameplayVisualScriptAsset* EditorGameplayVisualScriptDocumentProvider::Asset(
    const EditorDocumentId& id) const {
    return const_cast<EditorGameplayVisualScriptDocumentProvider*>(this)->Asset(id);
}
bool EditorGameplayVisualScriptDocumentProvider::Publish(
    const EditorDocumentId& id, EditorGameplayVisualScriptAsset asset) {
    std::string error;
    if (!id.IsValid() || id.type != EditorDocumentTypes::GameplayVisualScript ||
        !ValidateStructure(asset, &error)) return false;
    assets_[id.Key()] = std::move(asset); return true;
}

bool EditorGameplayVisualScriptDocumentProvider::Encode(const EditorGameplayVisualScriptAsset& asset,
    EditorDocumentContent* content, std::string* errorMessage) {
    if (content == nullptr || !ValidateStructure(asset, errorMessage)) return false;
    std::ostringstream output;
    output << "GAMEPLAY_VISUAL_SCRIPT " << kEditorGameplayVisualScriptSchemaVersion << '\n';
    output << "ASSET " << std::quoted(asset.assetGuid) << ' ' << std::quoted(asset.name) << '\n';
    output << "BUDGET " << asset.instructionBudget << '\n';
    for (const auto& variable : asset.variables) output << "VARIABLE " << std::quoted(variable.name)
        << ' ' << ToString(variable.defaultValue.type) << ' ' << std::quoted(EncodeValue(variable.defaultValue)) << '\n';
    output << "GRAPH " << asset.graph.revision << '\n';
    for (const auto& node : asset.graph.nodes) {
        output << "NODE " << std::quoted(node.id) << ' ' << std::quoted(node.typeId) << ' '
               << std::quoted(node.label) << ' ' << std::setprecision(9) << node.positionX << ' '
               << node.positionY << ' ' << node.properties.size() << '\n';
        for (const auto& [key, value] : node.properties) output << "PROPERTY "
            << std::quoted(key) << ' ' << std::quoted(value) << '\n';
    }
    for (const auto& link : asset.graph.links) output << "LINK " << std::quoted(link.id) << ' '
        << std::quoted(link.fromNodeId) << ' ' << std::quoted(link.fromPinId) << ' '
        << std::quoted(link.toNodeId) << ' ' << std::quoted(link.toPinId) << '\n';
    output << "END\n";
    const std::string bytes = output.str();
    content->schemaVersion = kEditorGameplayVisualScriptSchemaVersion;
    content->bytes.assign(bytes.begin(), bytes.end()); return true;
}

bool EditorGameplayVisualScriptDocumentProvider::Decode(const EditorDocumentContent& content,
    EditorGameplayVisualScriptAsset* asset, std::string* errorMessage) {
    if (asset == nullptr || content.bytes.empty() || content.bytes.size() > kMaxBytes) return false;
    std::istringstream input(std::string(content.bytes.begin(), content.bytes.end()));
    std::string line; std::getline(input, line); std::istringstream header(line);
    std::string marker; uint32_t schema = 0;
    if (!(header >> marker >> schema) || marker != "GAMEPLAY_VISUAL_SCRIPT" || schema == 0 ||
        schema > kEditorGameplayVisualScriptSchemaVersion) return false;
    EditorGameplayVisualScriptAsset decoded;
    bool hasAsset = false; bool hasEnd = false; EditorGraphNode* currentNode = nullptr;
    std::size_t remainingProperties = 0;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        std::istringstream row(line); std::string kind; row >> kind;
        if (kind == "ASSET") {
            if (!(row >> std::quoted(decoded.assetGuid) >> std::quoted(decoded.name))) return false;
            hasAsset = true;
        } else if (kind == "BUDGET") {
            if (!(row >> decoded.instructionBudget)) return false;
        } else if (kind == "VARIABLE") {
            std::string name, typeText, valueText; GameplayValueType type; GameplayValue value;
            if (!(row >> std::quoted(name) >> typeText >> std::quoted(valueText)) ||
                !GameplayValueTypeFromString(typeText, type) || !DecodeValue(type, valueText, value) ||
                decoded.variables.size() >= kEditorGameplayVisualScriptMaxVariables) return false;
            decoded.variables.push_back({std::move(name), std::move(value)});
        } else if (kind == "GRAPH") {
            if (!(row >> decoded.graph.revision)) decoded.graph.revision = 0;
        } else if (kind == "NODE") {
            if (remainingProperties != 0 || decoded.graph.nodes.size() >= kEditorGraphMaxNodes) return false;
            EditorGraphNode node;
            if (!(row >> std::quoted(node.id) >> std::quoted(node.typeId) >> std::quoted(node.label)
                  >> node.positionX >> node.positionY >> remainingProperties)) return false;
            decoded.graph.nodes.push_back(std::move(node)); currentNode = &decoded.graph.nodes.back();
        } else if (kind == "PROPERTY") {
            std::string key, value;
            if (currentNode == nullptr || remainingProperties == 0 ||
                !(row >> std::quoted(key) >> std::quoted(value))) return false;
            currentNode->properties[std::move(key)] = std::move(value); --remainingProperties;
        } else if (kind == "LINK") {
            EditorGraphLink link;
            if (remainingProperties != 0 || !(row >> std::quoted(link.id) >>
                std::quoted(link.fromNodeId) >> std::quoted(link.fromPinId) >>
                std::quoted(link.toNodeId) >> std::quoted(link.toPinId))) return false;
            decoded.graph.links.push_back(std::move(link)); currentNode = nullptr;
        } else if (kind == "END") {
            if (remainingProperties != 0) return false; hasEnd = true; break;
        } else return false;
    }
    decoded.schemaVersion = kEditorGameplayVisualScriptSchemaVersion;
    if (schema == 1 && decoded.instructionBudget == 0) decoded.instructionBudget = 4096;
    if (!hasAsset || !hasEnd || !ValidateStructure(decoded, errorMessage)) return false;
    *asset = std::move(decoded); return true;
}

} // namespace editor
