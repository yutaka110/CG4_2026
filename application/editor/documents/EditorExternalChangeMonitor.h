#pragma once

#include "EditorDocumentManager.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace editor {

struct EditorExternalChangeResult {
    EditorDocumentConflictState state = EditorDocumentConflictState::None;
    bool changed = false;
    uint64_t contentHash = 0;
    int64_t writeTime = 0;
    bool exists = false;
    std::string message;
};

struct EditorDocumentComparison {
    bool succeeded = false;
    bool identical = false;
    uint64_t editorContentHash = 0;
    uint64_t externalContentHash = 0;
    std::vector<uint8_t> editorBytes;
    std::vector<uint8_t> externalBytes;
    std::string message;
};

class EditorExternalChangeMonitor {
public:
    explicit EditorExternalChangeMonitor(
        std::filesystem::path projectRoot = std::filesystem::current_path());

    EditorExternalChangeResult Check(const EditorDocumentRecord& record) const;
    EditorDocumentComparison Compare(const EditorDocumentRecord& record) const;
    std::vector<EditorExternalChangeResult> Poll(EditorDocumentManager& manager) const;

    static uint64_t HashBytes(const std::vector<uint8_t>& bytes) noexcept;
    static uint64_t HashFile(const std::filesystem::path& path);

private:
    std::filesystem::path Absolute(const std::filesystem::path& path) const;

    std::filesystem::path projectRoot_;
};

} // namespace editor
