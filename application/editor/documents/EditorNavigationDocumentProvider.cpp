#include "EditorNavigationDocumentProvider.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>

namespace editor {
namespace {
constexpr std::size_t kMaximumNavigationDataBytes = 8u * 1024u * 1024u;

std::string LowerExtension(const std::filesystem::path& path) {
    std::string value = path.extension().string();
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::string Text(const EditorDocumentContent& content) {
    return std::string(content.bytes.begin(), content.bytes.end());
}

void SetText(EditorDocumentContent* content, std::string_view text) {
    content->schemaVersion = kEditorNavigationAuthoringSchemaVersion;
    content->bytes.assign(text.begin(), text.end());
}
} // namespace

bool EditorNavigationDocumentProvider::SupportsPath(const std::filesystem::path& path) const {
    const std::string extension = LowerExtension(path);
    return extension == ".navdata" || extension == ".navigation";
}

bool EditorNavigationDocumentProvider::ReadSource(const std::filesystem::path& path,
    EditorDocumentContent* content, std::string* errorMessage) const {
    if (content == nullptr) return false;
    std::error_code filesystemError;
    if (!std::filesystem::exists(path, filesystemError)) {
        const EditorNavigationAuthoringAsset asset = MakeDefaultEditorNavigationAuthoringAsset(
            MakeEditorDocumentGuid(EditorDocumentTypes::NavigationData, path), path.stem().string());
        std::string encoded;
        if (!EncodeEditorNavigationAuthoring(asset, encoded, errorMessage)) return false;
        SetText(content, encoded);
        return true;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        if (errorMessage != nullptr) *errorMessage = "Navigation Data source could not be opened.";
        return false;
    }
    content->bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (content->bytes.empty() || content->bytes.size() > kMaximumNavigationDataBytes) {
        if (errorMessage != nullptr) *errorMessage =
            "Navigation Data source is empty or exceeds its byte budget.";
        return false;
    }
    EditorNavigationAuthoringAsset decoded;
    if (!DecodeEditorNavigationAuthoring(Text(*content), decoded, errorMessage)) return false;
    content->schemaVersion = kEditorNavigationAuthoringSchemaVersion;
    return true;
}

bool EditorNavigationDocumentProvider::Serialize(const EditorDocumentId& id,
    EditorDocumentContent* content, std::string* errorMessage) const {
    const EditorNavigationAuthoringAsset* asset = Asset(id);
    std::string encoded;
    if (asset == nullptr || content == nullptr ||
        !EncodeEditorNavigationAuthoring(*asset, encoded, errorMessage)) return false;
    SetText(content, encoded);
    return true;
}

bool EditorNavigationDocumentProvider::Deserialize(const EditorDocumentId& id,
    const EditorDocumentContent& content, std::string* errorMessage) {
    EditorNavigationAuthoringAsset asset;
    if (!DecodeEditorNavigationAuthoring(Text(content), asset, errorMessage)) return false;
    assets_[id.Key()] = std::move(asset);
    return true;
}

EditorDocumentValidationReport EditorNavigationDocumentProvider::Validate(
    const EditorDocumentContent& content) const {
    EditorDocumentValidationReport report;
    EditorNavigationAuthoringAsset asset;
    std::string error;
    if (!DecodeEditorNavigationAuthoring(Text(content), asset, &error)) {
        report.issues.push_back({EditorDocumentIssueSeverity::Error,
            "navigation.data.parse", std::move(error)});
        return report;
    }
    const EditorNavigationAuthoringCompileResult compiled = CompileEditorNavigationAuthoring(asset);
    for (const auto& issue : compiled.diagnostics)
        report.issues.push_back({EditorDocumentIssueSeverity::Error, issue.code, issue.message});
    return report;
}

bool EditorNavigationDocumentProvider::Migrate(const EditorDocumentContent& source,
    EditorDocumentContent* migrated, EditorDocumentMigrationReport* report,
    std::string* errorMessage) const {
    if (migrated == nullptr || source.schemaVersion != kEditorNavigationAuthoringSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "Navigation Data schema cannot be migrated.";
        return false;
    }
    *migrated = source;
    if (report != nullptr) {
        report->sourceSchemaVersion = source.schemaVersion;
        report->targetSchemaVersion = kEditorNavigationAuthoringSchemaVersion;
        report->migrated = false;
    }
    return true;
}

void EditorNavigationDocumentProvider::Release(const EditorDocumentId& id) {
    assets_.erase(id.Key());
}

EditorNavigationAuthoringAsset* EditorNavigationDocumentProvider::Asset(
    const EditorDocumentId& id) {
    const auto found = assets_.find(id.Key());
    return found == assets_.end() ? nullptr : &found->second;
}

const EditorNavigationAuthoringAsset* EditorNavigationDocumentProvider::Asset(
    const EditorDocumentId& id) const {
    return const_cast<EditorNavigationDocumentProvider*>(this)->Asset(id);
}

bool EditorNavigationDocumentProvider::Publish(
    const EditorDocumentId& id, EditorNavigationAuthoringAsset asset) {
    if (!id.IsValid() || id.type != EditorDocumentTypes::NavigationData ||
        !CompileEditorNavigationAuthoring(asset).succeeded) return false;
    assets_[id.Key()] = std::move(asset);
    return true;
}

} // namespace editor
