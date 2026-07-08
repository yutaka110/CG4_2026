#include "EditorAssetRegistry.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace editor {
namespace {

std::string StableKeyForRecord(const EditorAssetRecord& record) {
    if (!record.logicalPath.empty()) {
        return record.logicalPath;
    }
    if (!record.sourcePath.empty()) {
        return record.sourcePath;
    }
    return record.id;
}

bool IsTextScannableAssetKind(EditorAssetKind kind) {
    return kind == EditorAssetKind::Course || kind == EditorAssetKind::Effect;
}

bool AppendUnique(std::vector<std::string>& values, const std::string& value) {
    if (value.empty()) {
        return false;
    }
    if (std::find(values.begin(), values.end(), value) != values.end()) {
        return false;
    }
    values.push_back(value);
    return true;
}

std::string ReadSmallTextFile(const std::filesystem::path& path) {
    constexpr std::uintmax_t kMaxScanBytes = 2u * 1024u * 1024u;
    std::error_code error;
    if (!std::filesystem::exists(path, error) ||
        !std::filesystem::is_regular_file(path, error) ||
        std::filesystem::file_size(path, error) > kMaxScanBytes) {
        return {};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

uint64_t Hash64(std::string_view text, uint64_t seed) {
    uint64_t hash = seed;
    for (unsigned char ch : text) {
        hash ^= static_cast<uint64_t>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string FormatProvisionalGuid(uint64_t a, uint64_t b) {
    std::ostringstream stream;
    stream << "auto-"
           << std::hex << std::setfill('0')
           << std::setw(8) << static_cast<uint32_t>(a >> 32)
           << '-'
           << std::setw(4) << static_cast<uint16_t>(a >> 16)
           << '-'
           << std::setw(4) << static_cast<uint16_t>(a)
           << '-'
           << std::setw(4) << static_cast<uint16_t>(b >> 48)
           << '-'
           << std::setw(12) << (b & 0x0000ffffffffffffull);
    return stream.str();
}

} // namespace

void EditorAssetRegistry::Clear() {
    if (records_.empty()) {
        return;
    }
    records_.clear();
    Touch();
}

bool EditorAssetRegistry::Register(EditorAssetRecord record) {
    if (record.id.empty() || record.kind == EditorAssetKind::Unknown) {
        return false;
    }
    EnsureEditorAssetIdentity(record);

    auto it = std::find_if(
        records_.begin(),
        records_.end(),
        [&](const EditorAssetRecord& existing) {
            return existing.kind == record.kind && existing.id == record.id;
        });
    if (it != records_.end()) {
        *it = std::move(record);
        Touch();
        return true;
    }

    records_.push_back(std::move(record));
    Touch();
    return true;
}

bool EditorAssetRegistry::Replace(
    EditorAssetKind oldKind,
    std::string_view oldId,
    EditorAssetRecord record) {
    if (oldId.empty() ||
        oldKind == EditorAssetKind::Unknown ||
        record.id.empty() ||
        record.kind == EditorAssetKind::Unknown) {
        return false;
    }
    EnsureEditorAssetIdentity(record);

    const auto oldIt = std::find_if(
        records_.begin(),
        records_.end(),
        [&](const EditorAssetRecord& existing) {
            return existing.kind == oldKind && existing.id == oldId;
        });
    if (oldIt == records_.end()) {
        return false;
    }

    const auto conflictIt = std::find_if(
        records_.begin(),
        records_.end(),
        [&](const EditorAssetRecord& existing) {
            return &existing != &*oldIt &&
                existing.kind == record.kind &&
                existing.id == record.id;
        });
    if (conflictIt != records_.end()) {
        return false;
    }

    *oldIt = std::move(record);
    Touch();
    return true;
}

bool EditorAssetRegistry::Remove(EditorAssetKind kind, std::string_view id) {
    if (id.empty() || kind == EditorAssetKind::Unknown) {
        return false;
    }

    const auto it = std::find_if(
        records_.begin(),
        records_.end(),
        [&](const EditorAssetRecord& record) {
            return record.kind == kind && record.id == id;
        });
    if (it == records_.end()) {
        return false;
    }

    records_.erase(it);
    Touch();
    return true;
}

const EditorAssetRecord* EditorAssetRegistry::Find(EditorAssetKind kind, std::string_view id) const {
    const auto it = std::find_if(
        records_.begin(),
        records_.end(),
        [&](const EditorAssetRecord& record) {
            return record.kind == kind && record.id == id;
        });
    return it != records_.end() ? &*it : nullptr;
}

const EditorAssetRecord* EditorAssetRegistry::FindByGuid(std::string_view guid) const {
    if (guid.empty()) {
        return nullptr;
    }
    const auto it = std::find_if(
        records_.begin(),
        records_.end(),
        [&](const EditorAssetRecord& record) {
            return record.guid == guid;
        });
    return it != records_.end() ? &*it : nullptr;
}

std::vector<const EditorAssetRecord*> EditorAssetRegistry::List(EditorAssetKind kind) const {
    std::vector<const EditorAssetRecord*> results;
    for (const EditorAssetRecord& record : records_) {
        if (record.kind == kind) {
            results.push_back(&record);
        }
    }
    return results;
}

std::vector<const EditorAssetRecord*> EditorAssetRegistry::FindDependencies(
    const EditorAssetRecord& record) const {
    std::vector<const EditorAssetRecord*> results;
    for (const std::string& dependency : record.dependencies) {
        EditorAssetDependencyToken token{};
        if (!ParseEditorAssetDependencyToken(dependency, token)) {
            continue;
        }
        if (const EditorAssetRecord* dependencyRecord = Find(token.kind, token.id)) {
            results.push_back(dependencyRecord);
        }
    }
    return results;
}

std::vector<const EditorAssetRecord*> EditorAssetRegistry::FindDependents(
    const EditorAssetRecord& record) const {
    const std::string token = BuildEditorAssetDependencyToken(record);
    std::vector<const EditorAssetRecord*> results;
    for (const EditorAssetRecord& candidate : records_) {
        if (candidate.kind == record.kind && candidate.id == record.id) {
            continue;
        }
        if (std::find(candidate.dependencies.begin(), candidate.dependencies.end(), token) !=
            candidate.dependencies.end()) {
            results.push_back(&candidate);
        }
    }
    return results;
}

std::vector<std::string> EditorAssetRegistry::FindMissingDependencyTokens(
    const EditorAssetRecord& record) const {
    std::vector<std::string> results;
    for (const std::string& dependency : record.dependencies) {
        EditorAssetDependencyToken token{};
        if (!ParseEditorAssetDependencyToken(dependency, token)) {
            continue;
        }
        if (Find(token.kind, token.id) == nullptr) {
            results.push_back(dependency);
        }
    }
    return results;
}

std::vector<std::string> EditorAssetRegistry::FindMalformedDependencyTokens(
    const EditorAssetRecord& record) const {
    std::vector<std::string> results;
    for (const std::string& dependency : record.dependencies) {
        EditorAssetDependencyToken token{};
        if (!ParseEditorAssetDependencyToken(dependency, token)) {
            results.push_back(dependency);
        }
    }
    return results;
}

std::size_t EditorAssetRegistry::CountDependents(const EditorAssetRecord& record) const {
    return FindDependents(record).size();
}

std::size_t EditorAssetRegistry::Count(EditorAssetKind kind) const {
    return static_cast<std::size_t>(std::count_if(
        records_.begin(),
        records_.end(),
        [&](const EditorAssetRecord& record) {
            return record.kind == kind;
        }));
}

std::size_t EditorAssetRegistry::CountMissing() const {
    return static_cast<std::size_t>(std::count_if(
        records_.begin(),
        records_.end(),
        [](const EditorAssetRecord& record) {
            return record.missing;
        }));
}

std::size_t EditorAssetRegistry::CountWithMetadata() const {
    return static_cast<std::size_t>(std::count_if(
        records_.begin(),
        records_.end(),
        [](const EditorAssetRecord& record) {
            return record.hasMetadata;
        }));
}

std::size_t EditorAssetRegistry::CountWithProvisionalGuid() const {
    return static_cast<std::size_t>(std::count_if(
        records_.begin(),
        records_.end(),
        [](const EditorAssetRecord& record) {
            return record.provisionalGuid;
        }));
}

std::size_t EditorAssetRegistry::CountWithDependencies() const {
    return static_cast<std::size_t>(std::count_if(
        records_.begin(),
        records_.end(),
        [](const EditorAssetRecord& record) {
            return !record.dependencies.empty();
        }));
}

void EditorAssetRegistry::ScanDependencies() {
    bool changed = false;
    for (EditorAssetRecord& record : records_) {
        if (!IsTextScannableAssetKind(record.kind) || record.sourcePath.empty()) {
            continue;
        }

        const std::string text = ReadSmallTextFile(record.sourcePath);
        if (text.empty()) {
            continue;
        }

        for (const EditorAssetRecord& candidate : records_) {
            if (&candidate == &record || candidate.id.empty()) {
                continue;
            }
            const bool referencesId = text.find(candidate.id) != std::string::npos;
            const bool referencesPath =
                !candidate.sourcePath.empty() && text.find(candidate.sourcePath) != std::string::npos;
            const bool referencesLogical =
                !candidate.logicalPath.empty() && text.find(candidate.logicalPath) != std::string::npos;
            if (referencesId || referencesPath || referencesLogical) {
                changed = AppendUnique(record.dependencies, BuildEditorAssetDependencyToken(candidate)) || changed;
            }
        }
    }
    if (changed) {
        Touch();
    }
}

void EditorAssetRegistry::Touch() {
    ++revision_;
}

const char* ToString(EditorAssetKind kind) {
    switch (kind) {
    case EditorAssetKind::Unknown:
        return "Unknown";
    case EditorAssetKind::Mesh:
        return "Mesh";
    case EditorAssetKind::Effect:
        return "Effect";
    case EditorAssetKind::Course:
        return "Course";
    case EditorAssetKind::Texture:
        return "Texture";
    case EditorAssetKind::Audio:
        return "Audio";
    }
    return "Unknown";
}

std::string BuildEditorAssetDependencyToken(EditorAssetKind kind, std::string_view id) {
    std::string token = ToString(kind);
    token += ':';
    token.append(id.data(), id.size());
    return token;
}

std::string BuildEditorAssetDependencyToken(const EditorAssetRecord& record) {
    return BuildEditorAssetDependencyToken(record.kind, record.id);
}

bool ParseEditorAssetDependencyToken(
    std::string_view token,
    EditorAssetDependencyToken& outToken) {
    const std::size_t separator = token.find(':');
    if (separator == std::string_view::npos) {
        return false;
    }

    const std::string_view kindText = token.substr(0, separator);
    if (kindText == ToString(EditorAssetKind::Mesh)) {
        outToken.kind = EditorAssetKind::Mesh;
    } else if (kindText == ToString(EditorAssetKind::Effect)) {
        outToken.kind = EditorAssetKind::Effect;
    } else if (kindText == ToString(EditorAssetKind::Course)) {
        outToken.kind = EditorAssetKind::Course;
    } else if (kindText == ToString(EditorAssetKind::Texture)) {
        outToken.kind = EditorAssetKind::Texture;
    } else if (kindText == ToString(EditorAssetKind::Audio)) {
        outToken.kind = EditorAssetKind::Audio;
    } else {
        outToken.kind = EditorAssetKind::Unknown;
    }

    const std::string_view id = token.substr(separator + 1);
    outToken.id.assign(id.data(), id.size());
    return outToken.kind != EditorAssetKind::Unknown && !outToken.id.empty();
}

std::string BuildEditorAssetProvisionalGuid(EditorAssetKind kind, std::string_view stableKey) {
    std::string seedText = ToString(kind);
    seedText += ':';
    seedText.append(stableKey.data(), stableKey.size());
    const uint64_t a = Hash64(seedText, 14695981039346656037ull);
    const uint64_t b = Hash64(seedText, 1099511628211ull);
    return FormatProvisionalGuid(a, b);
}

void EnsureEditorAssetIdentity(EditorAssetRecord& record) {
    if (record.logicalPath.empty()) {
        record.logicalPath = !record.sourcePath.empty() ? record.sourcePath : record.id;
    }
    if (record.metadataPath.empty() && !record.sourcePath.empty()) {
        record.metadataPath = record.sourcePath + ".meta";
    }
    if (record.guid.empty()) {
        record.guid = BuildEditorAssetProvisionalGuid(record.kind, StableKeyForRecord(record));
        record.provisionalGuid = true;
    }
    if (record.thumbnailKey.empty()) {
        record.thumbnailKey = "thumb:";
        record.thumbnailKey += ToString(record.kind);
        record.thumbnailKey += ':';
        if (!record.guid.empty()) {
            record.thumbnailKey += record.guid;
        } else {
            record.thumbnailKey += StableKeyForRecord(record);
        }
    }
}

} // namespace editor
