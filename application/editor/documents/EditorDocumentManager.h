#pragma once

#include "EditorDocumentRegistry.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorDocumentConflictState {
    None,
    ExternalModified,
    ExternalDeleted,
    ExternalCreated,
};

struct EditorDocumentRecord {
    EditorDocumentId id;
    IEditorDocumentProvider* provider = nullptr;
    std::filesystem::path path;
    std::string displayName;
    uint32_t schemaVersion = 0;
    uint64_t editRevision = 0;
    uint64_t savedRevision = 0;
    uint64_t autosaveRevision = 0;
    uint64_t sourceContentHash = 0;
    int64_t sourceWriteTime = 0;
    bool sourceExisted = false;
    bool open = false;
    bool dirty = false;
    bool recovered = false;
    EditorDocumentConflictState conflict = EditorDocumentConflictState::None;
    EditorDocumentMigrationReport migration;
};

struct EditorDocumentOpenResult {
    bool succeeded = false;
    bool alreadyOpen = false;
    EditorDocumentId id;
    EditorDocumentMigrationReport migration;
    EditorDocumentValidationReport validation;
    std::string message;
};

class EditorDocumentManager {
public:
    explicit EditorDocumentManager(
        EditorDocumentRegistry& registry,
        std::filesystem::path projectRoot = std::filesystem::current_path());
    ~EditorDocumentManager();

    EditorDocumentOpenResult Open(
        std::string_view typeId,
        const std::filesystem::path& path);
    EditorDocumentOpenResult Open(const std::filesystem::path& path);
    bool Reload(const EditorDocumentId& id, std::string* errorMessage = nullptr);
    bool Close(const EditorDocumentId& id, bool discardDirty, std::string* errorMessage = nullptr);
    bool Reopen(const EditorDocumentId& id, std::string* errorMessage = nullptr);
    bool RemoveClosed(const EditorDocumentId& id, std::string* errorMessage = nullptr);
    bool Duplicate(
        const EditorDocumentId& source,
        const std::filesystem::path& destination,
        EditorDocumentId* duplicateId,
        std::string* errorMessage = nullptr);
    bool RestoreFromContent(
        const EditorDocumentId& id,
        const std::filesystem::path& sourcePath,
        const EditorDocumentContent& content,
        std::string* errorMessage = nullptr);

    bool MarkDirty(const EditorDocumentId& id, std::string_view reason = {});
    bool MarkSaved(
        const EditorDocumentId& id,
        const std::filesystem::path& path,
        uint32_t schemaVersion);
    bool MarkAutosaved(const EditorDocumentId& id, uint64_t revision);
    bool MarkRecovered(const EditorDocumentId& id);
    bool SetConflict(const EditorDocumentId& id, EditorDocumentConflictState conflict);
    bool SetActive(const EditorDocumentId& id);

    EditorDocumentValidationReport Validate(const EditorDocumentId& id) const;
    EditorDocumentRecord* Find(const EditorDocumentId& id);
    const EditorDocumentRecord* Find(const EditorDocumentId& id) const;
    EditorDocumentRecord* FindByPath(const std::filesystem::path& path);
    const EditorDocumentRecord* Active() const;
    EditorDocumentRecord* Active();

    const std::vector<EditorDocumentRecord>& Documents() const noexcept { return documents_; }
    std::vector<const EditorDocumentRecord*> OpenDocuments() const;
    std::vector<const EditorDocumentRecord*> DirtyDocuments() const;
    std::size_t OpenCount() const;
    std::size_t DirtyCount() const;
    uint64_t Revision() const noexcept { return revision_; }
    const std::filesystem::path& ProjectRoot() const noexcept { return projectRoot_; }

private:
    bool ReadMigrateValidate(
        IEditorDocumentProvider& provider,
        const EditorDocumentId& id,
        const std::filesystem::path& path,
        EditorDocumentContent* content,
        EditorDocumentMigrationReport* migration,
        EditorDocumentValidationReport* validation,
        std::string* errorMessage);
    bool WriteMigrationArtifacts(
        const EditorDocumentId& id,
        const EditorDocumentContent& original,
        EditorDocumentMigrationReport* migration,
        std::string* errorMessage) const;
    void CaptureSourceState(EditorDocumentRecord& record) const;
    void Touch() noexcept { ++revision_; }

    EditorDocumentRegistry& registry_;
    std::filesystem::path projectRoot_;
    std::vector<EditorDocumentRecord> documents_;
    EditorDocumentId activeId_;
    uint64_t revision_ = 0;
};

const char* ToString(EditorDocumentConflictState state);

} // namespace editor
