#pragma once

#include "EditorExternalChangeMonitor.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace editor {

struct EditorAutosaveRecord {
    EditorDocumentId id;
    std::filesystem::path sourcePath;
    std::filesystem::path contentPath;
    std::filesystem::path manifestPath;
    uint64_t documentRevision = 0;
    uint32_t schemaVersion = 0;
    uint64_t sourceContentHash = 0;
    int64_t sourceWriteTime = 0;
    uint64_t autosaveContentHash = 0;
};

struct EditorAutosaveResult {
    bool succeeded = false;
    std::vector<EditorAutosaveRecord> records;
    std::vector<std::string> errors;
};

class EditorAutosaveService {
public:
    EditorAutosaveService(
        EditorDocumentManager& manager,
        std::filesystem::path projectRoot = std::filesystem::current_path());

    EditorAutosaveResult AutosaveDirtyDocuments();
    bool Autosave(
        const EditorDocumentId& id,
        EditorAutosaveRecord* record,
        std::string* errorMessage = nullptr);

private:
    EditorDocumentManager& manager_;
    std::filesystem::path projectRoot_;
};

} // namespace editor
