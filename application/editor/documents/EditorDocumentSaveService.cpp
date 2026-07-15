#include "EditorDocumentSaveService.h"

#include "../io/EditorFileTransaction.h"
#include "../io/EditorProjectPathPolicy.h"

#include <utility>

namespace editor {

EditorDocumentSaveService::EditorDocumentSaveService(
    EditorDocumentManager& manager,
    EditorExternalChangeMonitor& externalChanges,
    std::filesystem::path projectRoot)
    : manager_(manager), externalChanges_(externalChanges), projectRoot_(std::move(projectRoot)) {}

EditorDocumentSaveResult EditorDocumentSaveService::Save(const EditorDocumentId& id) {
    const EditorDocumentRecord* record = manager_.Find(id);
    return SavePrepared({{id, record != nullptr ? record->path : std::filesystem::path{}}}, true);
}

EditorDocumentSaveResult EditorDocumentSaveService::SaveAs(
    const EditorDocumentId& id,
    const std::filesystem::path& destination) {
    const EditorDocumentRecord* record = manager_.Find(id);
    const EditorProjectPathResolution resolution =
        EditorProjectPathPolicy(projectRoot_).Resolve(destination);
    if (record != nullptr && resolution.accepted &&
        record->path.lexically_normal() == resolution.projectRelativePath.lexically_normal()) {
        return Save(id);
    }
    return SavePrepared({{id, destination}}, false);
}

EditorDocumentSaveResult EditorDocumentSaveService::SaveAll() {
    std::vector<EditorDocumentId> ids;
    for (const EditorDocumentRecord* record : manager_.DirtyDocuments()) {
        if (record->open) ids.push_back(record->id);
    }
    return SaveAll(ids);
}

EditorDocumentSaveResult EditorDocumentSaveService::SaveAll(
    const std::vector<EditorDocumentId>& ids) {
    std::vector<std::pair<EditorDocumentId, std::filesystem::path>> requests;
    requests.reserve(ids.size());
    for (const EditorDocumentId& id : ids) {
        const EditorDocumentRecord* record = manager_.Find(id);
        requests.push_back({id, record != nullptr ? record->path : std::filesystem::path{}});
    }
    return SavePrepared(requests, true);
}

EditorDocumentSaveResult EditorDocumentSaveService::SavePrepared(
    const std::vector<std::pair<EditorDocumentId, std::filesystem::path>>& requests,
    bool checkExternalChanges) {
    EditorDocumentSaveResult result{};
    if (requests.empty()) {
        result.succeeded = true;
        result.message = "No dirty documents require saving.";
        return result;
    }

    std::vector<PreparedDocument> prepared;
    prepared.reserve(requests.size());
    for (const auto& request : requests) {
        EditorDocumentRecord* record = manager_.Find(request.first);
        EditorDocumentSaveItemResult item{};
        item.id = request.first;
        item.path = request.second;
        if (record == nullptr || record->provider == nullptr) {
            result.failure = EditorDocumentSaveFailure::MissingDocument;
            item.message = "Document is not registered.";
            result.items.push_back(std::move(item));
            result.message = "Save aborted before writing any document.";
            return result;
        }
        if (!record->open) {
            result.failure = EditorDocumentSaveFailure::ClosedDocument;
            item.message = "Closed document cannot be saved.";
            result.items.push_back(std::move(item));
            result.message = "Save aborted before writing any document.";
            return result;
        }
        if (request.second.empty() || !record->provider->SupportsPath(request.second)) {
            result.failure = EditorDocumentSaveFailure::Serialization;
            item.message = "Save destination is empty or unsupported.";
            result.items.push_back(std::move(item));
            result.message = "Save aborted before writing any document.";
            return result;
        }
        if (!checkExternalChanges) {
            const std::filesystem::path destination = request.second.is_absolute()
                ? request.second
                : projectRoot_ / request.second;
            std::error_code destinationError;
            if (std::filesystem::is_regular_file(destination, destinationError) &&
                !destinationError && request.second.lexically_normal() != record->path.lexically_normal()) {
                result.failure = EditorDocumentSaveFailure::ExternalConflict;
                item.message = "Save As destination already exists; explicit replacement is required.";
                result.items.push_back(std::move(item));
                result.message = "Save As blocked to prevent overwriting an existing file.";
                return result;
            }
        }
        if (checkExternalChanges) {
            const EditorExternalChangeResult external = externalChanges_.Check(*record);
            if (external.changed) {
                manager_.SetConflict(record->id, external.state);
                result.failure = EditorDocumentSaveFailure::ExternalConflict;
                item.message = external.message;
                result.items.push_back(std::move(item));
                result.message = "Save blocked to prevent overwriting an external change.";
                return result;
            }
        }

        PreparedDocument value{};
        value.record = record;
        value.destination = request.second.lexically_normal();
        std::string error;
        if (!record->provider->Serialize(record->id, &value.content, &error)) {
            result.failure = EditorDocumentSaveFailure::Serialization;
            item.message = error.empty() ? "Document serialization failed." : error;
            result.items.push_back(std::move(item));
            result.message = "Save aborted before writing any document.";
            return result;
        }
        value.content.schemaVersion = record->provider->CurrentSchemaVersion();
        const EditorDocumentValidationReport validation = record->provider->Validate(value.content);
        if (validation.HasErrors()) {
            result.failure = EditorDocumentSaveFailure::Validation;
            item.message = "Document validation failed before save.";
            result.items.push_back(std::move(item));
            result.message = "Save aborted before writing any document.";
            return result;
        }
        prepared.push_back(std::move(value));
        result.items.push_back(std::move(item));
    }

    EditorFileTransaction transaction(projectRoot_);
    result.transactionId = transaction.TransactionId();
    for (std::size_t index = 0; index < prepared.size(); ++index) {
        std::string error;
        if (!transaction.StageWrite(
                prepared[index].destination,
                prepared[index].content.bytes,
                {},
                &error)) {
            result.failure = EditorDocumentSaveFailure::FileTransaction;
            result.items[index].message = error;
            result.message = "Save All staging failed; no destination was changed.";
            return result;
        }
    }
    std::string error;
    if (!transaction.Execute(nullptr, &error)) {
        result.failure = EditorDocumentSaveFailure::FileTransaction;
        result.message = error.empty() ? "Atomic Save All failed and was rolled back." : error;
        return result;
    }

    for (std::size_t index = 0; index < prepared.size(); ++index) {
        PreparedDocument& value = prepared[index];
        manager_.MarkSaved(
            value.record->id, value.destination, value.content.schemaVersion);
        result.items[index].saved = true;
        result.items[index].path = value.destination;
        result.items[index].message = "Saved.";
    }
    result.succeeded = true;
    result.message = prepared.size() == 1
        ? "Document saved atomically."
        : "All documents saved in one file transaction.";
    return result;
}

const char* ToString(EditorDocumentSaveFailure failure) {
    switch (failure) {
    case EditorDocumentSaveFailure::None: return "None";
    case EditorDocumentSaveFailure::MissingDocument: return "MissingDocument";
    case EditorDocumentSaveFailure::ClosedDocument: return "ClosedDocument";
    case EditorDocumentSaveFailure::ExternalConflict: return "ExternalConflict";
    case EditorDocumentSaveFailure::Serialization: return "Serialization";
    case EditorDocumentSaveFailure::Validation: return "Validation";
    case EditorDocumentSaveFailure::FileTransaction: return "FileTransaction";
    }
    return "Unknown";
}

} // namespace editor
