#include "EditorDocumentManager.h"

#include "../io/EditorFileTransaction.h"
#include "../io/EditorProjectPathPolicy.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <utility>

namespace editor {
namespace {

std::string DisplayNameForPath(const std::filesystem::path& path, std::string_view fallback) {
    const std::string name = path.filename().string();
    return name.empty() ? std::string(fallback) : name;
}

std::string MigrationReportText(
    const EditorDocumentId& id,
    const EditorDocumentMigrationReport& report) {
    std::ostringstream stream;
    stream << "document=" << id.Key() << '\n';
    stream << "sourceSchema=" << report.sourceSchemaVersion << '\n';
    stream << "targetSchema=" << report.targetSchemaVersion << '\n';
    for (const std::string& note : report.notes) stream << "note=" << note << '\n';
    return stream.str();
}

uint64_t HashFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) return 0;
    uint64_t hash = 1469598103934665603ull;
    char buffer[8192];
    while (stream.good()) {
        stream.read(buffer, sizeof(buffer));
        const std::streamsize count = stream.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

} // namespace

EditorDocumentManager::EditorDocumentManager(
    EditorDocumentRegistry& registry,
    std::filesystem::path projectRoot)
    : registry_(registry), projectRoot_(std::move(projectRoot)) {}

EditorDocumentManager::~EditorDocumentManager() {
    for (EditorDocumentRecord& record : documents_) {
        if (record.provider != nullptr) record.provider->Release(record.id);
    }
}

EditorDocumentOpenResult EditorDocumentManager::Open(
    std::string_view typeId,
    const std::filesystem::path& path) {
    EditorDocumentOpenResult result{};
    const EditorProjectPathResolution resolution =
        EditorProjectPathPolicy(projectRoot_).Resolve(path);
    if (!resolution.accepted) {
        result.message = resolution.message.empty()
            ? "Document path is outside the project."
            : resolution.message;
        return result;
    }
    const std::filesystem::path documentPath = resolution.projectRelativePath;
    IEditorDocumentProvider* provider = registry_.Find(typeId);
    if (provider == nullptr) {
        result.message = "No document provider is registered for type: " + std::string(typeId);
        return result;
    }
    if (!provider->SupportsPath(documentPath)) {
        result.message = "Document provider does not support path: " + documentPath.generic_string();
        return result;
    }

    const EditorDocumentId id{MakeEditorDocumentGuid(typeId, documentPath), std::string(typeId)};
    if (EditorDocumentRecord* existing = Find(id)) {
        existing->open = true;
        activeId_ = id;
        Touch();
        result.succeeded = true;
        result.alreadyOpen = true;
        result.id = id;
        result.message = "Document is already registered and was activated.";
        return result;
    }

    EditorDocumentContent content{};
    EditorDocumentMigrationReport migration{};
    EditorDocumentValidationReport validation{};
    std::string error;
    if (!ReadMigrateValidate(
            *provider, id, documentPath, &content, &migration, &validation, &error)) {
        result.validation = std::move(validation);
        result.migration = std::move(migration);
        result.message = std::move(error);
        return result;
    }
    if (!provider->Deserialize(id, content, &error)) {
        result.message = error.empty() ? "Document provider rejected deserialization." : error;
        return result;
    }

    EditorDocumentRecord record{};
    record.id = id;
    record.provider = provider;
    record.path = documentPath;
    record.displayName = DisplayNameForPath(documentPath, provider->DisplayName());
    record.schemaVersion = content.schemaVersion;
    record.editRevision = migration.migrated ? 1 : 0;
    record.savedRevision = 0;
    record.open = true;
    record.dirty = migration.migrated;
    record.migration = migration;
    CaptureSourceState(record);
    documents_.push_back(std::move(record));
    activeId_ = id;
    Touch();

    result.succeeded = true;
    result.id = id;
    result.migration = std::move(migration);
    result.validation = std::move(validation);
    result.message = result.migration.migrated
        ? "Document opened after schema migration; save is required."
        : "Document opened.";
    return result;
}

EditorDocumentOpenResult EditorDocumentManager::Open(const std::filesystem::path& path) {
    IEditorDocumentProvider* provider = registry_.FindForPath(path);
    if (provider == nullptr) {
        EditorDocumentOpenResult result{};
        result.message = "No document provider accepts path: " + path.generic_string();
        return result;
    }
    return Open(provider->TypeId(), path);
}

bool EditorDocumentManager::Reload(const EditorDocumentId& id, std::string* errorMessage) {
    EditorDocumentRecord* record = Find(id);
    if (record == nullptr || record->provider == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Document is not registered.";
        return false;
    }
    EditorDocumentContent content{};
    EditorDocumentMigrationReport migration{};
    EditorDocumentValidationReport validation{};
    if (!ReadMigrateValidate(
            *record->provider, id, record->path, &content, &migration, &validation, errorMessage)) {
        return false;
    }
    if (!record->provider->Deserialize(id, content, errorMessage)) return false;
    record->schemaVersion = content.schemaVersion;
    record->dirty = migration.migrated;
    record->conflict = EditorDocumentConflictState::None;
    record->migration = std::move(migration);
    ++record->editRevision;
    record->savedRevision = record->dirty ? record->savedRevision : record->editRevision;
    CaptureSourceState(*record);
    Touch();
    return true;
}

bool EditorDocumentManager::Close(
    const EditorDocumentId& id,
    bool discardDirty,
    std::string* errorMessage) {
    EditorDocumentRecord* record = Find(id);
    if (record == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Document is not registered.";
        return false;
    }
    if (record->dirty && !discardDirty) {
        if (errorMessage != nullptr) *errorMessage = "Dirty document requires save or explicit discard.";
        return false;
    }
    record->open = false;
    if (discardDirty) {
        record->dirty = false;
        record->editRevision = record->savedRevision;
    }
    if (activeId_ == id) activeId_ = {};
    Touch();
    return true;
}

bool EditorDocumentManager::Reopen(const EditorDocumentId& id, std::string* errorMessage) {
    EditorDocumentRecord* record = Find(id);
    if (record == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Document is not registered.";
        return false;
    }
    record->open = true;
    activeId_ = id;
    Touch();
    return true;
}

bool EditorDocumentManager::RemoveClosed(
    const EditorDocumentId& id,
    std::string* errorMessage) {
    const auto it = std::find_if(
        documents_.begin(), documents_.end(),
        [&](const EditorDocumentRecord& record) { return record.id == id; });
    if (it == documents_.end()) {
        if (errorMessage != nullptr) *errorMessage = "Document is not registered.";
        return false;
    }
    if (it->open || it->dirty) {
        if (errorMessage != nullptr) *errorMessage = "Only clean, closed documents can be removed.";
        return false;
    }
    if (it->provider != nullptr) it->provider->Release(it->id);
    documents_.erase(it);
    Touch();
    return true;
}

bool EditorDocumentManager::Duplicate(
    const EditorDocumentId& source,
    const std::filesystem::path& destination,
    EditorDocumentId* duplicateId,
    std::string* errorMessage) {
    EditorDocumentRecord* sourceRecord = Find(source);
    if (sourceRecord == nullptr || sourceRecord->provider == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Source document is not registered.";
        return false;
    }
    const EditorProjectPathResolution resolution =
        EditorProjectPathPolicy(projectRoot_).Resolve(destination);
    if (!resolution.accepted) {
        if (errorMessage != nullptr) *errorMessage = resolution.message;
        return false;
    }
    const std::filesystem::path destinationPath = resolution.projectRelativePath;
    std::error_code destinationError;
    if (std::filesystem::is_regular_file(resolution.absolutePath, destinationError) &&
        !destinationError) {
        if (errorMessage != nullptr) *errorMessage = "Duplicate destination already exists on disk.";
        return false;
    }
    if (!sourceRecord->provider->SupportsPath(destinationPath)) {
        if (errorMessage != nullptr) *errorMessage = "Destination is not supported by the provider.";
        return false;
    }
    const EditorDocumentId id{
        MakeEditorDocumentGuid(source.type, destinationPath), source.type};
    if (Find(id) != nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Duplicate destination is already registered.";
        return false;
    }
    EditorDocumentContent content{};
    if (!sourceRecord->provider->Serialize(source, &content, errorMessage)) return false;
    const EditorDocumentValidationReport validation = sourceRecord->provider->Validate(content);
    if (validation.HasErrors()) {
        if (errorMessage != nullptr) *errorMessage = "Source document failed validation.";
        return false;
    }
    if (!sourceRecord->provider->Deserialize(id, content, errorMessage)) return false;

    EditorDocumentRecord record = *sourceRecord;
    record.id = id;
    record.path = destinationPath;
    record.displayName = DisplayNameForPath(destinationPath, sourceRecord->provider->DisplayName());
    record.open = true;
    record.dirty = true;
    record.recovered = false;
    record.conflict = EditorDocumentConflictState::None;
    ++record.editRevision;
    record.savedRevision = 0;
    record.autosaveRevision = 0;
    CaptureSourceState(record);
    documents_.push_back(std::move(record));
    activeId_ = id;
    if (duplicateId != nullptr) *duplicateId = id;
    Touch();
    return true;
}

bool EditorDocumentManager::RestoreFromContent(
    const EditorDocumentId& id,
    const std::filesystem::path& sourcePath,
    const EditorDocumentContent& sourceContent,
    std::string* errorMessage) {
    const EditorProjectPathResolution resolution =
        EditorProjectPathPolicy(projectRoot_).Resolve(sourcePath);
    if (!resolution.accepted) {
        if (errorMessage != nullptr) *errorMessage = resolution.message;
        return false;
    }
    const std::filesystem::path restoredSourcePath = resolution.projectRelativePath;
    IEditorDocumentProvider* provider = registry_.Find(id.type);
    if (provider == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Recovery provider is not registered.";
        return false;
    }
    if (sourceContent.schemaVersion == 0 ||
        sourceContent.schemaVersion > provider->CurrentSchemaVersion()) {
        if (errorMessage != nullptr) *errorMessage = "Recovery schema is unsupported.";
        return false;
    }
    EditorDocumentContent content = sourceContent;
    EditorDocumentMigrationReport migration{};
    if (content.schemaVersion < provider->CurrentSchemaVersion()) {
        if (!provider->Migrate(sourceContent, &content, &migration, errorMessage)) return false;
        migration.migrated = true;
        migration.sourceSchemaVersion = sourceContent.schemaVersion;
        migration.targetSchemaVersion = content.schemaVersion;
        if (!WriteMigrationArtifacts(id, sourceContent, &migration, errorMessage)) return false;
    }
    if (provider->Validate(content).HasErrors()) {
        if (errorMessage != nullptr) *errorMessage = "Recovered document failed validation.";
        return false;
    }
    if (!provider->Deserialize(id, content, errorMessage)) return false;

    EditorDocumentRecord* record = Find(id);
    if (record == nullptr) {
        EditorDocumentRecord value{};
        value.id = id;
        value.provider = provider;
        value.path = restoredSourcePath;
        value.displayName = DisplayNameForPath(restoredSourcePath, provider->DisplayName());
        value.schemaVersion = content.schemaVersion;
        value.open = true;
        value.dirty = true;
        value.recovered = true;
        value.editRevision = 1;
        value.migration = std::move(migration);
        CaptureSourceState(value);
        documents_.push_back(std::move(value));
    } else {
        record->path = restoredSourcePath;
        record->schemaVersion = content.schemaVersion;
        record->open = true;
        record->dirty = true;
        record->recovered = true;
        record->conflict = EditorDocumentConflictState::None;
        record->migration = std::move(migration);
        ++record->editRevision;
    }
    activeId_ = id;
    Touch();
    return true;
}

bool EditorDocumentManager::MarkDirty(const EditorDocumentId& id, std::string_view) {
    EditorDocumentRecord* record = Find(id);
    if (record == nullptr) return false;
    record->dirty = true;
    ++record->editRevision;
    Touch();
    return true;
}

bool EditorDocumentManager::MarkSaved(
    const EditorDocumentId& id,
    const std::filesystem::path& path,
    uint32_t schemaVersion) {
    EditorDocumentRecord* record = Find(id);
    if (record == nullptr) return false;
    const EditorProjectPathResolution resolution =
        EditorProjectPathPolicy(projectRoot_).Resolve(path);
    if (!resolution.accepted) return false;
    record->path = resolution.projectRelativePath;
    record->displayName = DisplayNameForPath(record->path, record->provider->DisplayName());
    record->schemaVersion = schemaVersion;
    record->dirty = false;
    record->recovered = false;
    record->conflict = EditorDocumentConflictState::None;
    record->savedRevision = record->editRevision;
    CaptureSourceState(*record);
    Touch();
    return true;
}

bool EditorDocumentManager::MarkAutosaved(const EditorDocumentId& id, uint64_t revision) {
    EditorDocumentRecord* record = Find(id);
    if (record == nullptr) return false;
    record->autosaveRevision = revision;
    Touch();
    return true;
}

bool EditorDocumentManager::MarkRecovered(const EditorDocumentId& id) {
    EditorDocumentRecord* record = Find(id);
    if (record == nullptr) return false;
    record->recovered = true;
    record->dirty = true;
    ++record->editRevision;
    Touch();
    return true;
}

bool EditorDocumentManager::SetConflict(
    const EditorDocumentId& id,
    EditorDocumentConflictState conflict) {
    EditorDocumentRecord* record = Find(id);
    if (record == nullptr) return false;
    if (record->conflict == conflict) return true;
    record->conflict = conflict;
    Touch();
    return true;
}

bool EditorDocumentManager::SetActive(const EditorDocumentId& id) {
    const EditorDocumentRecord* record = Find(id);
    if (record == nullptr || !record->open) return false;
    activeId_ = id;
    Touch();
    return true;
}

EditorDocumentValidationReport EditorDocumentManager::Validate(
    const EditorDocumentId& id) const {
    const EditorDocumentRecord* record = Find(id);
    if (record == nullptr || record->provider == nullptr) {
        EditorDocumentValidationReport report{};
        report.issues.push_back({
            EditorDocumentIssueSeverity::Error, "document.missing", "Document is not registered."});
        return report;
    }
    EditorDocumentContent content{};
    std::string error;
    if (!record->provider->Serialize(id, &content, &error)) {
        EditorDocumentValidationReport report{};
        report.issues.push_back({
            EditorDocumentIssueSeverity::Error,
            "document.serialize",
            error.empty() ? "Document serialization failed." : error});
        return report;
    }
    return record->provider->Validate(content);
}

EditorDocumentRecord* EditorDocumentManager::Find(const EditorDocumentId& id) {
    for (EditorDocumentRecord& record : documents_) if (record.id == id) return &record;
    return nullptr;
}

const EditorDocumentRecord* EditorDocumentManager::Find(const EditorDocumentId& id) const {
    for (const EditorDocumentRecord& record : documents_) if (record.id == id) return &record;
    return nullptr;
}

EditorDocumentRecord* EditorDocumentManager::FindByPath(const std::filesystem::path& path) {
    const EditorProjectPathResolution resolution =
        EditorProjectPathPolicy(projectRoot_).Resolve(path);
    if (!resolution.accepted) return nullptr;
    const std::filesystem::path normalized = resolution.projectRelativePath;
    for (EditorDocumentRecord& record : documents_) {
        if (record.path.lexically_normal() == normalized) return &record;
    }
    return nullptr;
}

const EditorDocumentRecord* EditorDocumentManager::Active() const { return Find(activeId_); }
EditorDocumentRecord* EditorDocumentManager::Active() { return Find(activeId_); }

std::vector<const EditorDocumentRecord*> EditorDocumentManager::OpenDocuments() const {
    std::vector<const EditorDocumentRecord*> result;
    for (const EditorDocumentRecord& record : documents_) if (record.open) result.push_back(&record);
    return result;
}

std::vector<const EditorDocumentRecord*> EditorDocumentManager::DirtyDocuments() const {
    std::vector<const EditorDocumentRecord*> result;
    for (const EditorDocumentRecord& record : documents_) if (record.dirty) result.push_back(&record);
    return result;
}

std::size_t EditorDocumentManager::OpenCount() const {
    return static_cast<std::size_t>(std::count_if(
        documents_.begin(), documents_.end(),
        [](const EditorDocumentRecord& record) { return record.open; }));
}

std::size_t EditorDocumentManager::DirtyCount() const {
    return static_cast<std::size_t>(std::count_if(
        documents_.begin(), documents_.end(),
        [](const EditorDocumentRecord& record) { return record.dirty; }));
}

bool EditorDocumentManager::ReadMigrateValidate(
    IEditorDocumentProvider& provider,
    const EditorDocumentId& id,
    const std::filesystem::path& path,
    EditorDocumentContent* content,
    EditorDocumentMigrationReport* migration,
    EditorDocumentValidationReport* validation,
    std::string* errorMessage) {
    EditorDocumentContent original{};
    const std::filesystem::path sourcePath = path.is_absolute() ? path : projectRoot_ / path;
    if (!provider.ReadSource(sourcePath, &original, errorMessage)) return false;
    if (original.schemaVersion == 0) {
        if (errorMessage != nullptr) *errorMessage = "Document schema version is zero.";
        return false;
    }
    if (original.schemaVersion > provider.CurrentSchemaVersion()) {
        if (errorMessage != nullptr) *errorMessage = "Document schema is newer than the provider.";
        return false;
    }

    *content = original;
    if (original.schemaVersion < provider.CurrentSchemaVersion()) {
        if (!provider.Migrate(original, content, migration, errorMessage)) return false;
        if (content->schemaVersion != provider.CurrentSchemaVersion()) {
            if (errorMessage != nullptr) *errorMessage = "Migration did not produce the current schema.";
            return false;
        }
        migration->migrated = true;
        migration->sourceSchemaVersion = original.schemaVersion;
        migration->targetSchemaVersion = content->schemaVersion;
        if (!WriteMigrationArtifacts(id, original, migration, errorMessage)) return false;
    }

    *validation = provider.Validate(*content);
    if (validation->HasErrors()) {
        if (errorMessage != nullptr) *errorMessage = "Document validation failed.";
        return false;
    }
    return true;
}

bool EditorDocumentManager::WriteMigrationArtifacts(
    const EditorDocumentId& id,
    const EditorDocumentContent& original,
    EditorDocumentMigrationReport* migration,
    std::string* errorMessage) const {
    const std::filesystem::path root =
        std::filesystem::path(".editor") / "migration-backups" / id.assetGuid;
    const std::filesystem::path backup = root /
        ("schema-" + std::to_string(original.schemaVersion) + ".backup");
    const std::filesystem::path reportPath = root / "migration.report";
    migration->backupPath = backup;
    migration->reportPath = reportPath;

    EditorFileTransaction transaction(projectRoot_);
    if (!transaction.StageWrite(backup, original.bytes, {}, errorMessage)) return false;
    if (!transaction.StageTextWrite(
            reportPath, MigrationReportText(id, *migration), {}, errorMessage)) return false;
    return transaction.Execute(nullptr, errorMessage);
}

void EditorDocumentManager::CaptureSourceState(EditorDocumentRecord& record) const {
    std::error_code ec;
    const std::filesystem::path absolute = record.path.is_absolute()
        ? record.path
        : projectRoot_ / record.path;
    record.sourceExisted = std::filesystem::is_regular_file(absolute, ec) && !ec;
    if (!record.sourceExisted) {
        record.sourceContentHash = 0;
        record.sourceWriteTime = 0;
        return;
    }
    record.sourceContentHash = HashFile(absolute);
    const auto writeTime = std::filesystem::last_write_time(absolute, ec);
    record.sourceWriteTime = ec ? 0 : static_cast<int64_t>(writeTime.time_since_epoch().count());
}

const char* ToString(EditorDocumentConflictState state) {
    switch (state) {
    case EditorDocumentConflictState::None: return "None";
    case EditorDocumentConflictState::ExternalModified: return "ExternalModified";
    case EditorDocumentConflictState::ExternalDeleted: return "ExternalDeleted";
    case EditorDocumentConflictState::ExternalCreated: return "ExternalCreated";
    }
    return "Unknown";
}

} // namespace editor
