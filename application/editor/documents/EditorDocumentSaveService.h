#pragma once

#include "EditorExternalChangeMonitor.h"

#include <filesystem>
#include <string>
#include <vector>

namespace editor {

enum class EditorDocumentSaveFailure {
    None,
    MissingDocument,
    ClosedDocument,
    ExternalConflict,
    Serialization,
    Validation,
    FileTransaction,
};

struct EditorDocumentSaveItemResult {
    EditorDocumentId id;
    std::filesystem::path path;
    bool saved = false;
    std::string message;
};

struct EditorDocumentSaveResult {
    bool succeeded = false;
    EditorDocumentSaveFailure failure = EditorDocumentSaveFailure::None;
    std::string transactionId;
    std::vector<EditorDocumentSaveItemResult> items;
    std::string message;
};

class EditorDocumentSaveService {
public:
    EditorDocumentSaveService(
        EditorDocumentManager& manager,
        EditorExternalChangeMonitor& externalChanges,
        std::filesystem::path projectRoot = std::filesystem::current_path());

    EditorDocumentSaveResult Save(const EditorDocumentId& id);
    EditorDocumentSaveResult SaveAs(
        const EditorDocumentId& id,
        const std::filesystem::path& destination);
    EditorDocumentSaveResult SaveAll();
    EditorDocumentSaveResult SaveAll(const std::vector<EditorDocumentId>& ids);

private:
    struct PreparedDocument {
        EditorDocumentRecord* record = nullptr;
        std::filesystem::path destination;
        EditorDocumentContent content;
    };

    EditorDocumentSaveResult SavePrepared(
        const std::vector<std::pair<EditorDocumentId, std::filesystem::path>>& requests,
        bool checkExternalChanges);

    EditorDocumentManager& manager_;
    EditorExternalChangeMonitor& externalChanges_;
    std::filesystem::path projectRoot_;
};

const char* ToString(EditorDocumentSaveFailure failure);

} // namespace editor
