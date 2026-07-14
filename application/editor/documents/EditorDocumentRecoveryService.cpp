#include "EditorDocumentRecoveryService.h"
#include "../io/EditorProjectPathPolicy.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <utility>

namespace editor {
namespace {

bool ParseUnsigned(const std::string& text, uint64_t* value) {
    try {
        std::size_t consumed = 0;
        const uint64_t parsed = std::stoull(text, &consumed);
        if (consumed != text.size()) return false;
        *value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseSigned(const std::string& text, int64_t* value) {
    try {
        std::size_t consumed = 0;
        const int64_t parsed = std::stoll(text, &consumed);
        if (consumed != text.size()) return false;
        *value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool ReadBytes(
    const std::filesystem::path& path,
    std::vector<uint8_t>* bytes,
    std::string* errorMessage) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        if (errorMessage != nullptr) *errorMessage = "Could not open autosave content.";
        return false;
    }
    stream.seekg(0, std::ios::end);
    const std::streamoff length = stream.tellg();
    if (length < 0) {
        if (errorMessage != nullptr) *errorMessage = "Could not measure autosave content.";
        return false;
    }
    stream.seekg(0, std::ios::beg);
    bytes->resize(static_cast<std::size_t>(length));
    if (length > 0) stream.read(reinterpret_cast<char*>(bytes->data()), length);
    if (!stream.good() && !stream.eof()) {
        if (errorMessage != nullptr) *errorMessage = "Could not read autosave content.";
        return false;
    }
    return true;
}

} // namespace

EditorDocumentRecoveryService::EditorDocumentRecoveryService(
    EditorDocumentRegistry& registry,
    EditorDocumentManager& manager,
    std::filesystem::path projectRoot)
    : registry_(registry), manager_(manager), projectRoot_(std::move(projectRoot)) {}

EditorDocumentRecoveryScanResult EditorDocumentRecoveryService::Scan() const {
    EditorDocumentRecoveryScanResult result{};
    const std::filesystem::path root = projectRoot_ / ".editor" / "autosave";
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) return result;

    std::map<std::string, EditorDocumentRecoveryCandidate> newest;
    std::filesystem::recursive_directory_iterator iterator(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; !ec && iterator != end; iterator.increment(ec)) {
        if (!iterator->is_regular_file(ec) || iterator->path().filename() != "manifest.txt") continue;
        EditorAutosaveRecord record{};
        std::string error;
        if (!ParseManifest(iterator->path(), &record, &error)) {
            result.errors.push_back(iterator->path().generic_string() + ": " + error);
            continue;
        }
        if (registry_.Find(record.id.type) == nullptr) {
            result.errors.push_back(record.id.Key() + ": provider is not registered.");
            continue;
        }
        EditorDocumentRecoveryCandidate candidate{};
        candidate.autosave = record;
        const std::filesystem::path source = Absolute(record.sourcePath);
        candidate.sourceMissing = !std::filesystem::is_regular_file(source, ec) || ec;
        const uint64_t currentHash = candidate.sourceMissing
            ? 0
            : EditorExternalChangeMonitor::HashFile(source);
        candidate.sourceChangedSinceAutosave =
            candidate.sourceMissing || currentHash != record.sourceContentHash;
        candidate.message = candidate.sourceChangedSinceAutosave
            ? "Autosave is recoverable, but the source also changed."
            : "Autosave is newer than the last saved document revision.";
        const std::string key = record.id.Key();
        const auto found = newest.find(key);
        if (found == newest.end() ||
            found->second.autosave.documentRevision < record.documentRevision) {
            newest[key] = std::move(candidate);
        }
    }
    if (ec) result.errors.push_back("Autosave scan failed: " + ec.message());
    for (auto& pair : newest) result.candidates.push_back(std::move(pair.second));
    result.succeeded = result.errors.empty();
    return result;
}

bool EditorDocumentRecoveryService::Recover(
    const EditorDocumentRecoveryCandidate& candidate,
    std::string* errorMessage) {
    IEditorDocumentProvider* provider = registry_.Find(candidate.autosave.id.type);
    if (provider == nullptr) {
        if (errorMessage != nullptr) *errorMessage = "Recovery provider is not registered.";
        return false;
    }
    EditorDocumentContent content{};
    content.schemaVersion = candidate.autosave.schemaVersion;
    if (!ReadBytes(Absolute(candidate.autosave.contentPath), &content.bytes, errorMessage)) return false;
    if (EditorExternalChangeMonitor::HashBytes(content.bytes) !=
        candidate.autosave.autosaveContentHash) {
        if (errorMessage != nullptr) *errorMessage = "Autosave content hash mismatch.";
        return false;
    }
    return manager_.RestoreFromContent(
        candidate.autosave.id,
        candidate.autosave.sourcePath,
        content,
        errorMessage);
}

bool EditorDocumentRecoveryService::ParseManifest(
    const std::filesystem::path& path,
    EditorAutosaveRecord* record,
    std::string* errorMessage) const {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        if (errorMessage != nullptr) *errorMessage = "Could not open autosave manifest.";
        return false;
    }
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(stream, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        values[line.substr(0, separator)] = line.substr(separator + 1);
    }
    if (values["format"] != "editor-autosave-v1" || values["type"].empty() ||
        values["guid"].empty() || values["sourcePath"].empty()) {
        if (errorMessage != nullptr) *errorMessage = "Autosave manifest is incomplete.";
        return false;
    }
    uint64_t schema = 0;
    if (!ParseUnsigned(values["documentRevision"], &record->documentRevision) ||
        !ParseUnsigned(values["schemaVersion"], &schema) || schema > UINT32_MAX ||
        !ParseUnsigned(values["sourceContentHash"], &record->sourceContentHash) ||
        !ParseSigned(values["sourceWriteTime"], &record->sourceWriteTime) ||
        !ParseUnsigned(values["autosaveContentHash"], &record->autosaveContentHash)) {
        if (errorMessage != nullptr) *errorMessage = "Autosave manifest contains invalid numbers.";
        return false;
    }
    record->schemaVersion = static_cast<uint32_t>(schema);
    record->id = EditorDocumentId{values["guid"], values["type"]};
    if (!record->id.IsValid() ||
        record->id.assetGuid.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.") !=
            std::string::npos) {
        if (errorMessage != nullptr) *errorMessage = "Autosave document identity is unsafe.";
        return false;
    }
    const EditorProjectPathResolution sourceResolution =
        EditorProjectPathPolicy(projectRoot_).Resolve(values["sourcePath"]);
    if (!sourceResolution.accepted) {
        if (errorMessage != nullptr) *errorMessage = "Autosave source path is outside the project.";
        return false;
    }
    record->sourcePath = sourceResolution.projectRelativePath;
    record->manifestPath = std::filesystem::relative(path, projectRoot_);
    record->contentPath = std::filesystem::relative(path.parent_path() / "document.autosave", projectRoot_);
    return true;
}

std::filesystem::path EditorDocumentRecoveryService::Absolute(
    const std::filesystem::path& path) const {
    return path.is_absolute() ? path : projectRoot_ / path;
}

} // namespace editor
