#include "EditorExternalChangeMonitor.h"

#include <fstream>
#include <iterator>

namespace editor {

EditorExternalChangeMonitor::EditorExternalChangeMonitor(std::filesystem::path projectRoot)
    : projectRoot_(std::move(projectRoot)) {}

EditorExternalChangeResult EditorExternalChangeMonitor::Check(
    const EditorDocumentRecord& record) const {
    EditorExternalChangeResult result{};
    const std::filesystem::path path = Absolute(record.path);
    std::error_code ec;
    result.exists = std::filesystem::is_regular_file(path, ec) && !ec;
    if (!result.exists) {
        result.changed = record.sourceExisted;
        result.state = result.changed
            ? EditorDocumentConflictState::ExternalDeleted
            : EditorDocumentConflictState::None;
        result.message = result.changed ? "Document source was deleted externally." : "No external change.";
        return result;
    }

    const auto writeTime = std::filesystem::last_write_time(path, ec);
    result.writeTime = ec ? 0 : static_cast<int64_t>(writeTime.time_since_epoch().count());
    result.contentHash = HashFile(path);
    if (!record.sourceExisted) {
        result.changed = true;
        result.state = EditorDocumentConflictState::ExternalCreated;
        result.message = "Document destination was created externally.";
        return result;
    }
    result.changed = result.contentHash != record.sourceContentHash;
    result.state = result.changed
        ? EditorDocumentConflictState::ExternalModified
        : EditorDocumentConflictState::None;
    result.message = result.changed ? "Document source was modified externally." : "No external change.";
    return result;
}

EditorDocumentComparison EditorExternalChangeMonitor::Compare(
    const EditorDocumentRecord& record) const {
    EditorDocumentComparison comparison{};
    if (record.provider == nullptr) {
        comparison.message = "Document provider is unavailable.";
        return comparison;
    }
    EditorDocumentContent editorContent{};
    std::string error;
    if (!record.provider->Serialize(record.id, &editorContent, &error)) {
        comparison.message = error.empty() ? "Editor document serialization failed." : error;
        return comparison;
    }
    std::ifstream stream(Absolute(record.path), std::ios::binary);
    if (!stream.is_open()) {
        comparison.message = "External document source is unavailable.";
        return comparison;
    }
    comparison.externalBytes.assign(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
    comparison.editorBytes = std::move(editorContent.bytes);
    comparison.editorContentHash = HashBytes(comparison.editorBytes);
    comparison.externalContentHash = HashBytes(comparison.externalBytes);
    comparison.identical = comparison.editorContentHash == comparison.externalContentHash &&
        comparison.editorBytes.size() == comparison.externalBytes.size();
    comparison.succeeded = true;
    comparison.message = comparison.identical
        ? "Editor and external document content are identical."
        : "Editor and external document content differ.";
    return comparison;
}

std::vector<EditorExternalChangeResult> EditorExternalChangeMonitor::Poll(
    EditorDocumentManager& manager) const {
    std::vector<EditorExternalChangeResult> results;
    for (const EditorDocumentRecord* record : manager.OpenDocuments()) {
        EditorExternalChangeResult result = Check(*record);
        manager.SetConflict(record->id, result.state);
        results.push_back(std::move(result));
    }
    return results;
}

uint64_t EditorExternalChangeMonitor::HashBytes(const std::vector<uint8_t>& bytes) noexcept {
    uint64_t hash = 1469598103934665603ull;
    for (const uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

uint64_t EditorExternalChangeMonitor::HashFile(const std::filesystem::path& path) {
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

std::filesystem::path EditorExternalChangeMonitor::Absolute(
    const std::filesystem::path& path) const {
    return path.is_absolute() ? path : projectRoot_ / path;
}

} // namespace editor
