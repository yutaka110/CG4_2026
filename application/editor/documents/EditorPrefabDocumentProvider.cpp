#include "EditorPrefabDocumentProvider.h"

#include "EditorSceneDocumentProvider.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
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

} // namespace

bool EditorPrefabDocumentProvider::SupportsPath(const std::filesystem::path& path) const {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == ".prefab";
}

bool EditorPrefabDocumentProvider::ReadSource(
    const std::filesystem::path& path,
    EditorDocumentContent* content,
    std::string* errorMessage) const {
    if (content == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Prefab document content output is null.";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        std::error_code error;
        if (std::filesystem::exists(path, error)) {
            if (errorMessage != nullptr) *errorMessage = "Could not open Prefab document: " + path.string();
            return false;
        }
        EditorPrefabAsset asset{};
        asset.assetGuid = MakeEditorDocumentGuid(EditorDocumentTypes::Prefab, path);
        asset.name = path.stem().string().empty() ? "Prefab" : path.stem().string();
        EditorSceneEntity* root = asset.templateScene.CreateEntity("Prefab Root");
        asset.rootEntityGuid = root != nullptr ? root->guid : std::string{};
        return Encode(asset, content, errorMessage);
    }
    content->bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    content->schemaVersion = 0;
    std::istringstream header(std::string(content->bytes.begin(), content->bytes.end()));
    std::string marker;
    if (!(header >> marker >> content->schemaVersion) || marker != "PREFAB") {
        if (errorMessage != nullptr) *errorMessage = "Prefab document header is invalid.";
        return false;
    }
    return true;
}

bool EditorPrefabDocumentProvider::Serialize(
    const EditorDocumentId& id,
    EditorDocumentContent* content,
    std::string* errorMessage) const {
    const EditorPrefabAsset* asset = Asset(id);
    if (asset == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Prefab live model is unavailable.";
        return false;
    }
    return Encode(*asset, content, errorMessage);
}

bool EditorPrefabDocumentProvider::Deserialize(
    const EditorDocumentId& id,
    const EditorDocumentContent& content,
    std::string* errorMessage) {
    EditorPrefabAsset decoded{};
    if (!Decode(content, &decoded, errorMessage)) return false;
    const uint64_t nextRevision = Asset(id) != nullptr ? Asset(id)->revision + 1 : 1;
    decoded.revision = nextRevision;
    assets_[id.Key()] = std::move(decoded);
    return true;
}

EditorDocumentValidationReport EditorPrefabDocumentProvider::Validate(
    const EditorDocumentContent& content) const {
    EditorDocumentValidationReport report{};
    if (content.bytes.size() > 64u * 1024u * 1024u) {
        AddIssue(report, EditorDocumentIssueSeverity::Error, "prefab.size",
            "Prefab exceeds the 64 MiB safety limit.");
        return report;
    }
    EditorPrefabAsset asset{};
    std::string error;
    if (!Decode(content, &asset, &error)) {
        AddIssue(report, EditorDocumentIssueSeverity::Error, "prefab.parse", std::move(error));
        return report;
    }
    const EditorPrefabValidationReport validation = asset.Validate();
    for (const std::string& message : validation.errors) {
        AddIssue(report, EditorDocumentIssueSeverity::Error, "prefab.model", message);
    }
    for (const std::string& message : validation.warnings) {
        AddIssue(report, EditorDocumentIssueSeverity::Warning, "prefab.model", message);
    }
    return report;
}

bool EditorPrefabDocumentProvider::Migrate(
    const EditorDocumentContent& source,
    EditorDocumentContent* migrated,
    EditorDocumentMigrationReport* report,
    std::string* errorMessage) const {
    if (migrated == nullptr || source.schemaVersion == 0 ||
        source.schemaVersion > kEditorPrefabSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "Prefab document has no compatible migration path.";
        return false;
    }
    if (source.schemaVersion == kEditorPrefabSchemaVersion) {
        *migrated = source;
        if (report != nullptr) {
            report->migrated = false;
            report->sourceSchemaVersion = source.schemaVersion;
            report->targetSchemaVersion = source.schemaVersion;
        }
        return true;
    }
    EditorPrefabAsset legacy{};
    if (!Decode(source, &legacy, errorMessage) || !Encode(legacy, migrated, errorMessage)) return false;
    if (report != nullptr) {
        report->migrated = true;
        report->sourceSchemaVersion = source.schemaVersion;
        report->targetSchemaVersion = kEditorPrefabSchemaVersion;
        report->notes.push_back(
            "Prefab schema v1 was upgraded to the explicit nested-Prefab policy schema.");
    }
    return true;
}

void EditorPrefabDocumentProvider::Release(const EditorDocumentId& id) {
    assets_.erase(id.Key());
}

EditorPrefabAsset* EditorPrefabDocumentProvider::Asset(const EditorDocumentId& id) {
    const auto found = assets_.find(id.Key());
    return found == assets_.end() ? nullptr : &found->second;
}

const EditorPrefabAsset* EditorPrefabDocumentProvider::Asset(const EditorDocumentId& id) const {
    return const_cast<EditorPrefabDocumentProvider*>(this)->Asset(id);
}

EditorPrefabAsset* EditorPrefabDocumentProvider::FindByAssetGuid(std::string_view assetGuid) {
    for (auto& [key, asset] : assets_) {
        (void)key;
        if (asset.assetGuid == assetGuid) return &asset;
    }
    return nullptr;
}

const EditorPrefabAsset* EditorPrefabDocumentProvider::FindByAssetGuid(std::string_view assetGuid) const {
    return const_cast<EditorPrefabDocumentProvider*>(this)->FindByAssetGuid(assetGuid);
}

EditorDocumentId EditorPrefabDocumentProvider::DocumentForAssetGuid(std::string_view assetGuid) const {
    for (const auto& [key, asset] : assets_) {
        if (asset.assetGuid != assetGuid) continue;
        const std::string prefix = std::string(EditorDocumentTypes::Prefab) + ":";
        if (key.rfind(prefix, 0) == 0) {
            return {key.substr(prefix.size()), std::string(EditorDocumentTypes::Prefab)};
        }
    }
    return {};
}

bool EditorPrefabDocumentProvider::Publish(const EditorDocumentId& id, EditorPrefabAsset asset) {
    if (!id.IsValid() || id.type != EditorDocumentTypes::Prefab || !asset.Validate().Succeeded()) {
        return false;
    }
    assets_[id.Key()] = std::move(asset);
    return true;
}

bool EditorPrefabDocumentProvider::Encode(
    const EditorPrefabAsset& asset,
    EditorDocumentContent* content,
    std::string* errorMessage) {
    if (content == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Prefab serialization output is null.";
        return false;
    }
    const EditorPrefabValidationReport validation = asset.Validate();
    if (!validation.Succeeded()) {
        if (errorMessage != nullptr) *errorMessage = validation.errors.front();
        return false;
    }
    EditorDocumentContent sceneContent{};
    if (!EditorSceneDocumentProvider::Encode(asset.templateScene, &sceneContent, errorMessage)) {
        return false;
    }
    std::ostringstream output;
    output << "PREFAB " << kEditorPrefabSchemaVersion << '\n';
    output << "ASSET " << std::quoted(asset.assetGuid) << ' '
           << std::quoted(asset.name) << ' ' << std::quoted(asset.rootEntityGuid) << '\n';
    for (const EditorPrefabNestedReference& nested : asset.nestedPrefabs) {
        output << "NESTED " << std::quoted(nested.mountEntityGuid) << ' '
               << std::quoted(nested.prefabAssetGuid) << '\n';
    }
    output << "SCENE_BYTES " << sceneContent.bytes.size() << '\n';
    output.write(
        reinterpret_cast<const char*>(sceneContent.bytes.data()),
        static_cast<std::streamsize>(sceneContent.bytes.size()));
    output << "\nEND\n";
    const std::string bytes = output.str();
    content->schemaVersion = kEditorPrefabSchemaVersion;
    content->bytes.assign(bytes.begin(), bytes.end());
    return true;
}

bool EditorPrefabDocumentProvider::Decode(
    const EditorDocumentContent& content,
    EditorPrefabAsset* asset,
    std::string* errorMessage) {
    if (asset == nullptr || content.bytes.empty()) {
        if (errorMessage != nullptr) *errorMessage = "Prefab content is empty.";
        return false;
    }
    std::istringstream input(std::string(content.bytes.begin(), content.bytes.end()));
    std::string line;
    if (!std::getline(input, line)) return false;
    std::istringstream header(line);
    std::string marker;
    uint32_t schema = 0;
    if (!(header >> marker >> schema) || marker != "PREFAB" || schema == 0 ||
        schema > kEditorPrefabSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "Prefab schema header is unsupported.";
        return false;
    }
    EditorPrefabAsset decoded{};
    decoded.schemaVersion = kEditorPrefabSchemaVersion;
    bool hasAsset = false;
    bool hasScene = false;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        std::istringstream row(line);
        std::string kind;
        row >> kind;
        if (kind == "ASSET") {
            if (!(row >> std::quoted(decoded.assetGuid) >> std::quoted(decoded.name) >>
                  std::quoted(decoded.rootEntityGuid))) {
                if (errorMessage != nullptr) *errorMessage = "Prefab ASSET record is invalid.";
                return false;
            }
            hasAsset = true;
        } else if (kind == "NESTED") {
            EditorPrefabNestedReference nested{};
            if (!(row >> std::quoted(nested.mountEntityGuid) >> std::quoted(nested.prefabAssetGuid))) {
                if (errorMessage != nullptr) *errorMessage = "Prefab NESTED record is invalid.";
                return false;
            }
            decoded.nestedPrefabs.push_back(std::move(nested));
        } else if (kind == "SCENE_BYTES") {
            std::size_t byteCount = 0;
            if (!(row >> byteCount) || byteCount > 64u * 1024u * 1024u) {
                if (errorMessage != nullptr) *errorMessage = "Prefab Scene byte count is invalid.";
                return false;
            }
            EditorDocumentContent sceneContent{};
            sceneContent.bytes.resize(byteCount);
            input.read(
                reinterpret_cast<char*>(sceneContent.bytes.data()),
                static_cast<std::streamsize>(byteCount));
            if (static_cast<std::size_t>(input.gcount()) != byteCount ||
                !EditorSceneDocumentProvider::Decode(sceneContent, &decoded.templateScene, errorMessage)) {
                return false;
            }
            hasScene = true;
            std::getline(input, line);
        } else if (kind == "END") {
            break;
        } else {
            if (errorMessage != nullptr) *errorMessage = "Unknown Prefab record: " + kind;
            return false;
        }
    }
    if (!hasAsset || !hasScene) {
        if (errorMessage != nullptr) *errorMessage = "Prefab is missing ASSET or SCENE_BYTES.";
        return false;
    }
    const EditorPrefabValidationReport validation = decoded.Validate();
    if (!validation.Succeeded()) {
        if (errorMessage != nullptr) *errorMessage = validation.errors.front();
        return false;
    }
    *asset = std::move(decoded);
    return true;
}

} // namespace editor
