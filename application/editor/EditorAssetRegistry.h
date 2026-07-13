#pragma once

#include <cstddef>
#include <cstdint>
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
    uint64_t sourceTimestamp = 0;
    bool runtimeOnly = false;
    bool referenceable = false;
    bool missing = false;
    bool hasMetadata = false;
    bool provisionalGuid = false;
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
    void ScanDependencies();
    uint32_t Revision() const { return revision_; }
    const std::vector<EditorAssetRecord>& Records() const { return records_; }

private:
    void Touch();

    std::vector<EditorAssetRecord> records_;
    uint32_t revision_ = 0;
};

const char* ToString(EditorAssetKind kind);
std::string BuildEditorAssetDependencyToken(EditorAssetKind kind, std::string_view id);
std::string BuildEditorAssetDependencyToken(const EditorAssetRecord& record);
bool ParseEditorAssetDependencyToken(std::string_view token, EditorAssetDependencyToken& outToken);
std::string BuildEditorAssetProvisionalGuid(EditorAssetKind kind, std::string_view stableKey);
void EnsureEditorAssetIdentity(EditorAssetRecord& record);

} // namespace editor
