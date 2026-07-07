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
    std::string displayName;
    std::string sourcePath;
    bool runtimeOnly = false;
    bool referenceable = false;
};

class EditorAssetRegistry {
public:
    void Clear();
    bool Register(EditorAssetRecord record);

    const EditorAssetRecord* Find(EditorAssetKind kind, std::string_view id) const;
    std::vector<const EditorAssetRecord*> List(EditorAssetKind kind) const;

    std::size_t Count() const { return records_.size(); }
    std::size_t Count(EditorAssetKind kind) const;
    uint32_t Revision() const { return revision_; }
    const std::vector<EditorAssetRecord>& Records() const { return records_; }

private:
    void Touch();

    std::vector<EditorAssetRecord> records_;
    uint32_t revision_ = 0;
};

const char* ToString(EditorAssetKind kind);

} // namespace editor
