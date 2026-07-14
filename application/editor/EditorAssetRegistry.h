#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

enum class EditorAssetKind {
    Unknown,
    Mesh,
    Effect,
    Course,
    Texture,
    Audio,
    Prefab,
    MaterialGraph,
    VfxGraph,
    AnimationStateMachine,
    GameplayVisualScript,
};

struct EditorAssetRecord {
    EditorAssetKind kind = EditorAssetKind::Unknown;
    std::string id;
    std::string guid;
    std::string logicalPath;
    std::string displayName;
    std::string sourcePath;
    std::string metadataPath;
    std::string thumbnailKey;
    std::vector<std::string> tags;
    std::vector<std::string> dependencies;
    std::vector<std::string> guidDependencies;
    std::vector<std::string> pathOnlyReferences;
    uint64_t sourceTimestamp = 0;
    bool runtimeOnly = false;
    bool referenceable = false;
    bool missing = false;
    bool hasMetadata = false;
    bool provisionalGuid = false;
};

struct EditorAssetRedirect {
    EditorAssetKind kind = EditorAssetKind::Unknown;
    std::string guid;
    std::string oldId;
    std::string oldLogicalPath;
    std::string oldSourcePath;
    std::string currentId;
    std::string currentLogicalPath;
    std::string currentSourcePath;
};

enum class EditorAssetReferenceResolutionSource {
    None,
    Id,
    Guid,
    LogicalPath,
    SourcePath,
    Redirect,
};

struct EditorAssetReferenceResolution {
    const EditorAssetRecord* record = nullptr;
    EditorAssetReferenceResolutionSource source = EditorAssetReferenceResolutionSource::None;
    std::string canonicalReference;
    bool resolved = false;
    bool requiresRepair = false;
};

struct EditorAssetDependencyToken {
    EditorAssetKind kind = EditorAssetKind::Unknown;
    std::string id;
};

class EditorAssetRegistry {
public:
    void Clear();
    bool Register(EditorAssetRecord record);
    bool Replace(EditorAssetKind oldKind, std::string_view oldId, EditorAssetRecord record);
    bool Remove(EditorAssetKind kind, std::string_view id);

    const EditorAssetRecord* Find(EditorAssetKind kind, std::string_view id) const;
    const EditorAssetRecord* FindByGuid(std::string_view guid) const;
    std::vector<const EditorAssetRecord*> FindAllByGuid(std::string_view guid) const;
    EditorAssetReferenceResolution ResolveReference(
        EditorAssetKind expectedKind,
        std::string_view reference) const;
    bool RepairPathOnlyReferences(EditorAssetKind ownerKind, std::string_view ownerId);
    std::vector<const EditorAssetRecord*> List(EditorAssetKind kind) const;
    std::vector<const EditorAssetRecord*> FindDependencies(const EditorAssetRecord& record) const;
    std::vector<const EditorAssetRecord*> FindDependents(const EditorAssetRecord& record) const;
    std::vector<std::string> FindMissingDependencyTokens(const EditorAssetRecord& record) const;
    std::vector<std::string> FindMalformedDependencyTokens(const EditorAssetRecord& record) const;
    std::size_t CountDependents(const EditorAssetRecord& record) const;

    std::size_t Count() const { return records_.size(); }
    std::size_t Count(EditorAssetKind kind) const;
    std::size_t CountMissing() const;
    std::size_t CountWithMetadata() const;
    std::size_t CountWithProvisionalGuid() const;
    std::size_t CountWithDependencies() const;
    std::size_t CountDurableAssets() const;
    std::size_t CountMetadataEligibleAssets() const;
    double MetadataCoveragePercent() const;
    std::vector<std::string> DuplicateGuids() const;
    void ScanDependencies();

    void ConfigureRedirectStore(
        std::filesystem::path redirectPath,
        std::filesystem::path projectRoot = std::filesystem::current_path());
    bool LoadRedirects(std::string* errorMessage = nullptr);
    bool RecordRedirect(
        const EditorAssetRecord& from,
        const EditorAssetRecord& to,
        std::string* errorMessage = nullptr);
    bool RemoveRedirect(
        const EditorAssetRecord& from,
        const EditorAssetRecord& restored,
        std::string* errorMessage = nullptr);
    const std::vector<EditorAssetRedirect>& Redirects() const noexcept { return redirects_; }
    uint32_t Revision() const { return revision_; }
    const std::vector<EditorAssetRecord>& Records() const { return records_; }

private:
    void Touch();

    std::vector<EditorAssetRecord> records_;
    std::vector<EditorAssetRedirect> redirects_;
    std::filesystem::path redirectPath_;
    std::filesystem::path projectRoot_ = std::filesystem::current_path();
    uint32_t revision_ = 0;
};

const char* ToString(EditorAssetKind kind);
std::string BuildEditorAssetDependencyToken(EditorAssetKind kind, std::string_view id);
std::string BuildEditorAssetDependencyToken(const EditorAssetRecord& record);
bool ParseEditorAssetDependencyToken(std::string_view token, EditorAssetDependencyToken& outToken);
std::string BuildEditorAssetProvisionalGuid(EditorAssetKind kind, std::string_view stableKey);
std::string GenerateEditorAssetGuid();
bool IsDurableEditorAssetGuid(std::string_view guid) noexcept;
std::string BuildEditorAssetGuidReference(std::string_view guid);
void EnsureEditorAssetIdentity(EditorAssetRecord& record);
const char* ToString(EditorAssetReferenceResolutionSource source);

} // namespace editor
