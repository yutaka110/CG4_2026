#include "EditorAiDocumentProviders.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace editor {
namespace {
constexpr std::size_t kMaximumBehaviorBytes = 16u * 1024u * 1024u;
constexpr std::size_t kMaximumEqsBytes = 4u * 1024u * 1024u;

std::string LowerExtension(const std::filesystem::path& path) {
    std::string value = path.extension().string();
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool ReadBytes(const std::filesystem::path& path, std::size_t maximum,
    EditorDocumentContent* content, std::string* errorMessage) {
    if (content == nullptr) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        if (errorMessage != nullptr) *errorMessage = "AI authoring source could not be opened.";
        return false;
    }
    content->bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (content->bytes.empty() || content->bytes.size() > maximum) {
        if (errorMessage != nullptr) *errorMessage = "AI authoring source is empty or exceeds its byte budget.";
        return false;
    }
    return true;
}

std::string Text(const EditorDocumentContent& content) {
    return std::string(content.bytes.begin(), content.bytes.end());
}

void SetText(EditorDocumentContent* content, uint32_t schema, const std::string& text) {
    content->schemaVersion = schema;
    content->bytes.assign(text.begin(), text.end());
}
} // namespace

bool EditorBehaviorTreeDocumentProvider::SupportsPath(const std::filesystem::path& path) const {
    const std::string extension = LowerExtension(path);
    return extension == ".behavior" || extension == ".behaviortree" || extension == ".btree";
}

bool EditorBehaviorTreeDocumentProvider::ReadSource(const std::filesystem::path& path,
    EditorDocumentContent* content, std::string* errorMessage) const {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        EditorBehaviorTreeAsset asset = MakeDefaultEditorBehaviorTree(
            MakeEditorDocumentGuid(EditorDocumentTypes::BehaviorTree, path), path.stem().string());
        std::string encoded;
        if (!EncodeEditorBehaviorTree(asset, encoded, errorMessage)) return false;
        SetText(content, kEditorBehaviorTreeSchemaVersion, encoded);
        return true;
    }
    if (!ReadBytes(path, kMaximumBehaviorBytes, content, errorMessage)) return false;
    EditorBehaviorTreeAsset asset;
    if (!DecodeEditorBehaviorTree(Text(*content), asset, errorMessage)) return false;
    content->schemaVersion = kEditorBehaviorTreeSchemaVersion;
    return true;
}

bool EditorBehaviorTreeDocumentProvider::Serialize(const EditorDocumentId& id,
    EditorDocumentContent* content, std::string* errorMessage) const {
    const EditorBehaviorTreeAsset* asset = Asset(id);
    std::string encoded;
    if (asset == nullptr || content == nullptr ||
        !EncodeEditorBehaviorTree(*asset, encoded, errorMessage)) return false;
    SetText(content, kEditorBehaviorTreeSchemaVersion, encoded);
    return true;
}

bool EditorBehaviorTreeDocumentProvider::Deserialize(const EditorDocumentId& id,
    const EditorDocumentContent& content, std::string* errorMessage) {
    EditorBehaviorTreeAsset asset;
    if (!DecodeEditorBehaviorTree(Text(content), asset, errorMessage)) return false;
    assets_[id.Key()] = std::move(asset);
    return true;
}

EditorDocumentValidationReport EditorBehaviorTreeDocumentProvider::Validate(
    const EditorDocumentContent& content) const {
    EditorDocumentValidationReport report;
    EditorBehaviorTreeAsset asset;
    std::string error;
    if (!DecodeEditorBehaviorTree(Text(content), asset, &error)) {
        report.issues.push_back({EditorDocumentIssueSeverity::Error, "ai.behavior.parse", std::move(error)});
        return report;
    }
    for (const auto& issue : CompileEditorBehaviorTree(asset).diagnostics)
        report.issues.push_back({EditorDocumentIssueSeverity::Error, issue.code, issue.message});
    return report;
}

bool EditorBehaviorTreeDocumentProvider::Migrate(const EditorDocumentContent& source,
    EditorDocumentContent* migrated, EditorDocumentMigrationReport* report,
    std::string* errorMessage) const {
    if (migrated == nullptr || source.schemaVersion != kEditorBehaviorTreeSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "Behavior Tree schema cannot be migrated.";
        return false;
    }
    *migrated = source;
    if (report != nullptr) {
        report->sourceSchemaVersion = source.schemaVersion;
        report->targetSchemaVersion = kEditorBehaviorTreeSchemaVersion;
        report->migrated = false;
    }
    return true;
}

void EditorBehaviorTreeDocumentProvider::Release(const EditorDocumentId& id) { assets_.erase(id.Key()); }
EditorBehaviorTreeAsset* EditorBehaviorTreeDocumentProvider::Asset(const EditorDocumentId& id) {
    const auto found = assets_.find(id.Key());
    return found == assets_.end() ? nullptr : &found->second;
}
const EditorBehaviorTreeAsset* EditorBehaviorTreeDocumentProvider::Asset(const EditorDocumentId& id) const {
    return const_cast<EditorBehaviorTreeDocumentProvider*>(this)->Asset(id);
}
bool EditorBehaviorTreeDocumentProvider::Publish(const EditorDocumentId& id,
    EditorBehaviorTreeAsset asset) {
    if (!id.IsValid() || id.type != EditorDocumentTypes::BehaviorTree ||
        !CompileEditorBehaviorTree(asset).succeeded) return false;
    assets_[id.Key()] = std::move(asset);
    return true;
}

bool EditorEqsDocumentProvider::SupportsPath(const std::filesystem::path& path) const {
    const std::string extension = LowerExtension(path);
    return extension == ".eqs" || extension == ".envquery";
}

bool EditorEqsDocumentProvider::ReadSource(const std::filesystem::path& path,
    EditorDocumentContent* content, std::string* errorMessage) const {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        EditorEqsAsset asset = MakeDefaultEditorEqsAsset(
            MakeEditorDocumentGuid(EditorDocumentTypes::EnvironmentQuery, path), path.stem().string());
        std::string encoded;
        if (!EncodeEditorEqs(asset, encoded, errorMessage)) return false;
        SetText(content, kEditorEqsSchemaVersion, encoded);
        return true;
    }
    if (!ReadBytes(path, kMaximumEqsBytes, content, errorMessage)) return false;
    EditorEqsAsset asset;
    if (!DecodeEditorEqs(Text(*content), asset, errorMessage)) return false;
    content->schemaVersion = kEditorEqsSchemaVersion;
    return true;
}

bool EditorEqsDocumentProvider::Serialize(const EditorDocumentId& id,
    EditorDocumentContent* content, std::string* errorMessage) const {
    const EditorEqsAsset* asset = Asset(id);
    std::string encoded;
    if (asset == nullptr || content == nullptr || !EncodeEditorEqs(*asset, encoded, errorMessage)) return false;
    SetText(content, kEditorEqsSchemaVersion, encoded);
    return true;
}

bool EditorEqsDocumentProvider::Deserialize(const EditorDocumentId& id,
    const EditorDocumentContent& content, std::string* errorMessage) {
    EditorEqsAsset asset;
    if (!DecodeEditorEqs(Text(content), asset, errorMessage)) return false;
    assets_[id.Key()] = std::move(asset);
    return true;
}

EditorDocumentValidationReport EditorEqsDocumentProvider::Validate(
    const EditorDocumentContent& content) const {
    EditorDocumentValidationReport report;
    EditorEqsAsset asset;
    std::string error;
    if (!DecodeEditorEqs(Text(content), asset, &error)) {
        report.issues.push_back({EditorDocumentIssueSeverity::Error, "ai.eqs.parse", std::move(error)});
        return report;
    }
    for (const auto& issue : CompileEditorEqs(asset).diagnostics)
        report.issues.push_back({EditorDocumentIssueSeverity::Error, issue.code, issue.message});
    return report;
}

bool EditorEqsDocumentProvider::Migrate(const EditorDocumentContent& source,
    EditorDocumentContent* migrated, EditorDocumentMigrationReport* report,
    std::string* errorMessage) const {
    if (migrated == nullptr || source.schemaVersion != kEditorEqsSchemaVersion) {
        if (errorMessage != nullptr) *errorMessage = "Environment Query schema cannot be migrated.";
        return false;
    }
    *migrated = source;
    if (report != nullptr) {
        report->sourceSchemaVersion = source.schemaVersion;
        report->targetSchemaVersion = kEditorEqsSchemaVersion;
        report->migrated = false;
    }
    return true;
}

void EditorEqsDocumentProvider::Release(const EditorDocumentId& id) { assets_.erase(id.Key()); }
EditorEqsAsset* EditorEqsDocumentProvider::Asset(const EditorDocumentId& id) {
    const auto found = assets_.find(id.Key());
    return found == assets_.end() ? nullptr : &found->second;
}
const EditorEqsAsset* EditorEqsDocumentProvider::Asset(const EditorDocumentId& id) const {
    return const_cast<EditorEqsDocumentProvider*>(this)->Asset(id);
}
bool EditorEqsDocumentProvider::Publish(const EditorDocumentId& id, EditorEqsAsset asset) {
    if (!id.IsValid() || id.type != EditorDocumentTypes::EnvironmentQuery ||
        !CompileEditorEqs(asset).succeeded) return false;
    assets_[id.Key()] = std::move(asset);
    return true;
}

} // namespace editor
