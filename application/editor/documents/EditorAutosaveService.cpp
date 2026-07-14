#include "EditorAutosaveService.h"

#include "../io/EditorFileTransaction.h"

#include <sstream>
#include <utility>

namespace editor {
namespace {

std::string ManifestText(const EditorAutosaveRecord& record) {
    std::ostringstream stream;
    stream << "format=editor-autosave-v1\n";
    stream << "type=" << record.id.type << '\n';
    stream << "guid=" << record.id.assetGuid << '\n';
    stream << "sourcePath=" << record.sourcePath.generic_string() << '\n';
    stream << "documentRevision=" << record.documentRevision << '\n';
    stream << "schemaVersion=" << record.schemaVersion << '\n';
    stream << "sourceContentHash=" << record.sourceContentHash << '\n';
    stream << "sourceWriteTime=" << record.sourceWriteTime << '\n';
    stream << "autosaveContentHash=" << record.autosaveContentHash << '\n';
    return stream.str();
}

} // namespace

EditorAutosaveService::EditorAutosaveService(
    EditorDocumentManager& manager,
    std::filesystem::path projectRoot)
    : manager_(manager), projectRoot_(std::move(projectRoot)) {}

EditorAutosaveResult EditorAutosaveService::AutosaveDirtyDocuments() {
    EditorAutosaveResult result{};
    for (const EditorDocumentRecord* document : manager_.DirtyDocuments()) {
        if (!document->open || document->autosaveRevision >= document->editRevision) continue;
        EditorAutosaveRecord record{};
        std::string error;
        if (Autosave(document->id, &record, &error)) {
            result.records.push_back(std::move(record));
        } else {
            result.errors.push_back(document->id.Key() + ": " + error);
        }
    }
    result.succeeded = result.errors.empty();
    return result;
}

bool EditorAutosaveService::Autosave(
    const EditorDocumentId& id,
    EditorAutosaveRecord* record,
    std::string* errorMessage) {
    EditorDocumentRecord* document = manager_.Find(id);
    if (document == nullptr || document->provider == nullptr || !document->open) {
        if (errorMessage != nullptr) *errorMessage = "Open document is unavailable for autosave.";
        return false;
    }
    EditorDocumentContent content{};
    if (!document->provider->Serialize(id, &content, errorMessage)) return false;
    content.schemaVersion = document->provider->CurrentSchemaVersion();
    if (document->provider->Validate(content).HasErrors()) {
        if (errorMessage != nullptr) *errorMessage = "Autosave validation failed.";
        return false;
    }

    EditorAutosaveRecord value{};
    value.id = id;
    value.sourcePath = document->path;
    value.documentRevision = document->editRevision;
    value.schemaVersion = content.schemaVersion;
    value.sourceContentHash = document->sourceContentHash;
    value.sourceWriteTime = document->sourceWriteTime;
    value.autosaveContentHash = EditorExternalChangeMonitor::HashBytes(content.bytes);
    const std::filesystem::path root = std::filesystem::path(".editor") / "autosave" /
        id.assetGuid / std::to_string(value.documentRevision);
    value.contentPath = root / "document.autosave";
    value.manifestPath = root / "manifest.txt";

    EditorFileTransaction transaction(projectRoot_);
    if (!transaction.StageWrite(value.contentPath, content.bytes, {}, errorMessage)) return false;
    if (!transaction.StageTextWrite(
            value.manifestPath, ManifestText(value), {}, errorMessage)) return false;
    if (!transaction.Execute(nullptr, errorMessage)) return false;
    manager_.MarkAutosaved(id, value.documentRevision);
    if (record != nullptr) *record = std::move(value);
    return true;
}

} // namespace editor
