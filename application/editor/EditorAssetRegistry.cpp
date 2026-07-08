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

std::string DependencyToken(const EditorAssetRecord& record) {
    return std::string(ToString(record.kind)) + ":" + record.id;
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
                changed = AppendUnique(record.dependencies, DependencyToken(candidate)) || changed;
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
}

} // namespace editor
