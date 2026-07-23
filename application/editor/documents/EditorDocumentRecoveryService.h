#pragma once

#include "EditorAutosaveService.h"

#include <filesystem>
#include <string>
#include <vector>

namespace editor {

struct EditorDocumentRecoveryCandidate {
    EditorAutosaveRecord autosave;
    bool sourceChangedSinceAutosave = false;
    bool sourceMissing = false;
    std::string message;
};

struct EditorDocumentRecoveryQuarantineRecord {
    std::filesystem::path originalGenerationPath;
    std::filesystem::path quarantineGenerationPath;
    std::string reason;
};

struct EditorDocumentRecoveryScanResult {
    bool succeeded = true;
    std::vector<EditorDocumentRecoveryCandidate> candidates;
    std::vector<EditorDocumentRecoveryQuarantineRecord> quarantined;
    std::vector<std::string> errors;
};

class EditorDocumentRecoveryService {
public:
    EditorDocumentRecoveryService(
        EditorDocumentRegistry& registry,
        EditorDocumentManager& manager,
        std::filesystem::path projectRoot = std::filesystem::current_path());

    EditorDocumentRecoveryScanResult Scan() const;
    bool Recover(
        const EditorDocumentRecoveryCandidate& candidate,
        std::string* errorMessage = nullptr);

private:
    bool ParseManifest(
        const std::filesystem::path& path,
        EditorAutosaveRecord* record,
        std::string* errorMessage) const;
    bool QuarantineGeneration(
        const std::filesystem::path& manifestPath,
        const std::string& reason,
        EditorDocumentRecoveryQuarantineRecord* record,
        std::string* errorMessage) const;
    std::filesystem::path Absolute(const std::filesystem::path& path) const;

    EditorDocumentRegistry& registry_;
    EditorDocumentManager& manager_;
    std::filesystem::path projectRoot_;
};

} // namespace editor
