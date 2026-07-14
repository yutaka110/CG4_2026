#include "EditorTextDocumentProvider.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <utility>

namespace editor {

EditorTextDocumentProvider::EditorTextDocumentProvider(
    EditorDocumentTypeId typeId,
    std::string displayName,
    std::vector<std::string> extensions,
    uint32_t schemaVersion)
    : typeId_(std::move(typeId)),
      displayName_(std::move(displayName)),
      extensions_(std::move(extensions)),
      schemaVersion_(schemaVersion) {}

bool EditorTextDocumentProvider::SupportsPath(const std::filesystem::path& path) const {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return std::find(extensions_.begin(), extensions_.end(), extension) != extensions_.end();
}

bool EditorTextDocumentProvider::ReadSource(
    const std::filesystem::path& path,
    EditorDocumentContent* content,
    std::string* errorMessage) const {
    if (content == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Text document content output is null.";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        if (errorMessage != nullptr) *errorMessage = "Could not open document: " + path.string();
        return false;
    }
    content->bytes.assign(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
    content->schemaVersion = 1;
    const std::string prefix = "# editor-schema:";
    const std::string text(content->bytes.begin(), content->bytes.end());
    if (text.rfind(prefix, 0) == 0) {
        const std::size_t end = text.find_first_of("\r\n", prefix.size());
        try {
            content->schemaVersion = static_cast<uint32_t>(
                std::stoul(text.substr(prefix.size(), end - prefix.size())));
        } catch (...) {
            if (errorMessage != nullptr) *errorMessage = "Document schema header is invalid.";
            return false;
        }
    }
    return true;
}

bool EditorTextDocumentProvider::Serialize(
    const EditorDocumentId& id,
    EditorDocumentContent* content,
    std::string* errorMessage) const {
    const auto found = contents_.find(id.Key());
    if (found == contents_.end() || content == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Text document live model is unavailable.";
        return false;
    }
    *content = found->second;
    return true;
}

bool EditorTextDocumentProvider::Deserialize(
    const EditorDocumentId& id,
    const EditorDocumentContent& content,
    std::string*) {
    contents_[id.Key()] = content;
    return true;
}

EditorDocumentValidationReport EditorTextDocumentProvider::Validate(
    const EditorDocumentContent& content) const {
    EditorDocumentValidationReport report{};
    if (content.bytes.empty()) {
        report.issues.push_back({
            EditorDocumentIssueSeverity::Error, "document.empty", "Document content is empty."});
    }
    if (content.bytes.size() > 64u * 1024u * 1024u) {
        report.issues.push_back({
            EditorDocumentIssueSeverity::Error, "document.size", "Document exceeds the 64 MiB safety limit."});
    }
    return report;
}

bool EditorTextDocumentProvider::Migrate(
    const EditorDocumentContent& source,
    EditorDocumentContent* migrated,
    EditorDocumentMigrationReport* report,
    std::string* errorMessage) const {
    if (migrated == nullptr || source.schemaVersion == 0 || source.schemaVersion > schemaVersion_) {
        if (errorMessage != nullptr) *errorMessage = "Text document has no compatible migration path.";
        return false;
    }
    *migrated = source;
    migrated->schemaVersion = schemaVersion_;
    const std::string header = "# editor-schema:" + std::to_string(schemaVersion_) + "\n";
    const std::string text(source.bytes.begin(), source.bytes.end());
    const std::size_t firstLineEnd = text.find_first_of("\r\n");
    const std::string body = text.rfind("# editor-schema:", 0) == 0
        ? (firstLineEnd == std::string::npos ? std::string{} : text.substr(firstLineEnd + 1))
        : text;
    const std::string output = header + body;
    migrated->bytes.assign(output.begin(), output.end());
    if (report != nullptr) {
        report->migrated = source.schemaVersion != schemaVersion_;
        report->sourceSchemaVersion = source.schemaVersion;
        report->targetSchemaVersion = schemaVersion_;
        report->notes.push_back("Updated generic text document schema header.");
    }
    return true;
}

void EditorTextDocumentProvider::Release(const EditorDocumentId& id) {
    contents_.erase(id.Key());
}

bool EditorTextDocumentProvider::SetText(const EditorDocumentId& id, std::string text) {
    const auto found = contents_.find(id.Key());
    if (found == contents_.end()) return false;
    found->second.schemaVersion = schemaVersion_;
    found->second.bytes.assign(text.begin(), text.end());
    return true;
}

const EditorDocumentContent* EditorTextDocumentProvider::Content(
    const EditorDocumentId& id) const {
    const auto found = contents_.find(id.Key());
    return found == contents_.end() ? nullptr : &found->second;
}

} // namespace editor
