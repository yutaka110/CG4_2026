#include "EditorAssetRegistry.h"

#include "io/EditorFileTransaction.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <unordered_map>
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
    return kind == EditorAssetKind::Course || kind == EditorAssetKind::Effect ||
        kind == EditorAssetKind::Prefab || kind == EditorAssetKind::MaterialGraph ||
        kind == EditorAssetKind::MaterialInstance ||
        kind == EditorAssetKind::VfxGraph || kind == EditorAssetKind::AnimationStateMachine ||
        kind == EditorAssetKind::GameplayVisualScript || kind == EditorAssetKind::BehaviorTree ||
        kind == EditorAssetKind::EnvironmentQuery || kind == EditorAssetKind::NavigationData;
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

EditorAssetKind AssetKindFromText(std::string_view text) {
    for (EditorAssetKind kind : {EditorAssetKind::Mesh, EditorAssetKind::Effect,
             EditorAssetKind::Course, EditorAssetKind::Prefab, EditorAssetKind::MaterialGraph,
             EditorAssetKind::MaterialInstance,
             EditorAssetKind::VfxGraph,
             EditorAssetKind::AnimationStateMachine,
             EditorAssetKind::GameplayVisualScript,
             EditorAssetKind::BehaviorTree,
             EditorAssetKind::EnvironmentQuery,
             EditorAssetKind::NavigationData,
             EditorAssetKind::Texture, EditorAssetKind::Audio}) {
        if (text == ToString(kind)) return kind;
    }
    return EditorAssetKind::Unknown;
}

std::string SerializeRedirects(const std::vector<EditorAssetRedirect>& redirects) {
    std::ostringstream output;
    output << "ASSET_REDIRECTS 1\n";
    for (const EditorAssetRedirect& redirect : redirects) {
        output << "REDIRECT " << std::quoted(std::string(ToString(redirect.kind))) << ' '
               << std::quoted(redirect.guid) << ' ' << std::quoted(redirect.oldId) << ' '
               << std::quoted(redirect.oldLogicalPath) << ' ' << std::quoted(redirect.oldSourcePath) << ' '
               << std::quoted(redirect.currentId) << ' ' << std::quoted(redirect.currentLogicalPath) << ' '
               << std::quoted(redirect.currentSourcePath) << '\n';
    }
    output << "END\n";
    return output.str();
}

bool ParseRedirects(
    const std::filesystem::path& path,
    std::vector<EditorAssetRedirect>* redirects,
    std::string* errorMessage) {
    if (redirects == nullptr) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        if (errorMessage != nullptr) *errorMessage = "Could not open Asset redirect table.";
        return false;
    }
    std::string marker;
    uint32_t version = 0;
    if (!(input >> marker >> version) || marker != "ASSET_REDIRECTS" || version != 1) {
        if (errorMessage != nullptr) *errorMessage = "Asset redirect table header is invalid.";
        return false;
    }
    std::vector<EditorAssetRedirect> parsed;
    while (input >> marker) {
        if (marker == "END") {
            *redirects = std::move(parsed);
            return true;
        }
        EditorAssetRedirect redirect{};
        std::string kind;
        if (marker != "REDIRECT" || !(input >> std::quoted(kind) >> std::quoted(redirect.guid) >>
                std::quoted(redirect.oldId) >> std::quoted(redirect.oldLogicalPath) >>
                std::quoted(redirect.oldSourcePath) >> std::quoted(redirect.currentId) >>
                std::quoted(redirect.currentLogicalPath) >> std::quoted(redirect.currentSourcePath))) {
            if (errorMessage != nullptr) *errorMessage = "Asset redirect table record is invalid.";
            return false;
        }
        redirect.kind = AssetKindFromText(kind);
        if (redirect.kind == EditorAssetKind::Unknown || !IsDurableEditorAssetGuid(redirect.guid)) {
            if (errorMessage != nullptr) *errorMessage = "Asset redirect identity is invalid.";
            return false;
        }
        parsed.push_back(std::move(redirect));
    }
    if (errorMessage != nullptr) *errorMessage = "Asset redirect table is missing END.";
    return false;
}

bool PersistRedirects(
    const std::vector<EditorAssetRedirect>& redirects,
    const std::filesystem::path& redirectPath,
    const std::filesystem::path& projectRoot,
    std::string* errorMessage) {
    if (redirectPath.empty()) return true;
    EditorFileTransaction transaction(projectRoot);
    if (!transaction.StageTextWrite(
            redirectPath,
            SerializeRedirects(redirects),
            [](const std::filesystem::path& staged, std::string* validationError) {
                std::vector<EditorAssetRedirect> validated;
                return ParseRedirects(staged, &validated, validationError);
            },
            errorMessage)) {
        return false;
    }
    return transaction.Execute(nullptr, errorMessage);
}

bool SameRedirectAlias(const EditorAssetRedirect& redirect, const EditorAssetRecord& record) {
    return redirect.kind == record.kind && redirect.guid == record.guid &&
        redirect.oldId == record.id && redirect.oldLogicalPath == record.logicalPath &&
        redirect.oldSourcePath == record.sourcePath;
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

std::vector<const EditorAssetRecord*> EditorAssetRegistry::FindAllByGuid(
    std::string_view guid) const {
    std::vector<const EditorAssetRecord*> results;
    if (guid.empty()) return results;
    for (const EditorAssetRecord& record : records_) {
        if (record.guid == guid) results.push_back(&record);
    }
    return results;
}

EditorAssetReferenceResolution EditorAssetRegistry::ResolveReference(
    EditorAssetKind expectedKind,
    std::string_view reference) const {
    EditorAssetReferenceResolution result{};
    if (reference.empty()) return result;
    const auto kindMatches = [&](const EditorAssetRecord& record) {
        return expectedKind == EditorAssetKind::Unknown || record.kind == expectedKind;
    };
    const auto publish = [&](const EditorAssetRecord& record,
                             EditorAssetReferenceResolutionSource source,
                             bool repair) {
        result.record = &record;
        result.source = source;
        result.canonicalReference = BuildEditorAssetGuidReference(record.guid);
        result.resolved = true;
        result.requiresRepair = repair;
    };
    if (expectedKind != EditorAssetKind::Unknown) {
        if (const EditorAssetRecord* record = Find(expectedKind, reference)) {
            publish(*record, EditorAssetReferenceResolutionSource::Id, true);
            return result;
        }
    } else {
        for (const EditorAssetRecord& record : records_) {
            if (record.id == reference) {
                publish(record, EditorAssetReferenceResolutionSource::Id, true);
                return result;
            }
        }
    }
    std::string_view guid = reference;
    constexpr std::string_view kGuidPrefix = "asset-guid:";
    if (guid.rfind(kGuidPrefix, 0) == 0) guid.remove_prefix(kGuidPrefix.size());
    for (const EditorAssetRecord* record : FindAllByGuid(guid)) {
        if (record != nullptr && kindMatches(*record)) {
            publish(*record, EditorAssetReferenceResolutionSource::Guid, false);
            return result;
        }
    }
    for (const EditorAssetRecord& record : records_) {
        if (!kindMatches(record)) continue;
        if (!record.logicalPath.empty() && record.logicalPath == reference) {
            publish(record, EditorAssetReferenceResolutionSource::LogicalPath, true);
            return result;
        }
        if (!record.sourcePath.empty() && record.sourcePath == reference) {
            publish(record, EditorAssetReferenceResolutionSource::SourcePath, true);
            return result;
        }
    }
    for (const EditorAssetRedirect& redirect : redirects_) {
        if (expectedKind != EditorAssetKind::Unknown && redirect.kind != expectedKind) continue;
        if (reference != redirect.oldId && reference != redirect.oldLogicalPath &&
            reference != redirect.oldSourcePath && reference != redirect.guid) {
            continue;
        }
        for (const EditorAssetRecord* record : FindAllByGuid(redirect.guid)) {
            if (record != nullptr && kindMatches(*record)) {
                publish(*record, EditorAssetReferenceResolutionSource::Redirect, true);
                return result;
            }
        }
    }
    return result;
}

bool EditorAssetRegistry::RepairPathOnlyReferences(
    EditorAssetKind ownerKind,
    std::string_view ownerId) {
    EditorAssetRecord* owner = nullptr;
    for (EditorAssetRecord& record : records_) {
        if (record.kind == ownerKind && record.id == ownerId) {
            owner = &record;
            break;
        }
    }
    if (owner == nullptr || owner->pathOnlyReferences.empty()) return false;
    std::vector<std::string> unresolved;
    bool changed = false;
    for (const std::string& reference : owner->pathOnlyReferences) {
        const EditorAssetReferenceResolution resolution = ResolveReference(
            EditorAssetKind::Unknown, reference);
        if (!resolution.resolved || resolution.record == nullptr ||
            !IsDurableEditorAssetGuid(resolution.record->guid)) {
            unresolved.push_back(reference);
            continue;
        }
        changed = AppendUnique(owner->guidDependencies, resolution.record->guid) || changed;
    }
    if (!changed) return false;
    owner->pathOnlyReferences = std::move(unresolved);
    Touch();
    return true;
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

std::size_t EditorAssetRegistry::CountMetadataEligibleAssets() const {
    return static_cast<std::size_t>(std::count_if(records_.begin(), records_.end(),
        [](const EditorAssetRecord& record) {
            return !record.runtimeOnly && !record.missing && record.kind != EditorAssetKind::Unknown;
        }));
}

std::size_t EditorAssetRegistry::CountDurableAssets() const {
    return static_cast<std::size_t>(std::count_if(records_.begin(), records_.end(),
        [](const EditorAssetRecord& record) {
            return !record.runtimeOnly && !record.missing && record.kind != EditorAssetKind::Unknown &&
                record.hasMetadata && !record.provisionalGuid &&
                IsDurableEditorAssetGuid(record.guid);
        }));
}

double EditorAssetRegistry::MetadataCoveragePercent() const {
    const std::size_t eligible = CountMetadataEligibleAssets();
    return eligible == 0 ? 100.0 :
        static_cast<double>(CountDurableAssets()) * 100.0 / static_cast<double>(eligible);
}

std::vector<std::string> EditorAssetRegistry::DuplicateGuids() const {
    std::unordered_map<std::string, std::size_t> counts;
    for (const EditorAssetRecord& record : records_) {
        if (!record.guid.empty()) ++counts[record.guid];
    }
    std::vector<std::string> duplicates;
    for (const auto& [guid, count] : counts) {
        if (count > 1) duplicates.push_back(guid);
    }
    std::sort(duplicates.begin(), duplicates.end());
    return duplicates;
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
            const bool referencesGuid = IsDurableEditorAssetGuid(candidate.guid) &&
                (text.find(candidate.guid) != std::string::npos ||
                    text.find(BuildEditorAssetGuidReference(candidate.guid)) != std::string::npos);
            if (referencesId || referencesPath || referencesLogical) {
                changed = AppendUnique(record.dependencies, BuildEditorAssetDependencyToken(candidate)) || changed;
            }
            if (referencesGuid) {
                changed = AppendUnique(record.guidDependencies, candidate.guid) || changed;
            }
            const bool hasDurableMapping = std::find(
                record.guidDependencies.begin(), record.guidDependencies.end(), candidate.guid) !=
                record.guidDependencies.end();
            if (!referencesGuid && !hasDurableMapping && (referencesPath || referencesLogical)) {
                const std::string legacy = referencesLogical
                    ? candidate.logicalPath
                    : candidate.sourcePath;
                changed = AppendUnique(record.pathOnlyReferences, legacy) || changed;
            }
        }
    }
    if (changed) {
        Touch();
    }
}

void EditorAssetRegistry::ConfigureRedirectStore(
    std::filesystem::path redirectPath,
    std::filesystem::path projectRoot) {
    redirectPath_ = std::move(redirectPath);
    projectRoot_ = std::move(projectRoot);
}

bool EditorAssetRegistry::LoadRedirects(std::string* errorMessage) {
    if (redirectPath_.empty()) return true;
    const std::filesystem::path absolute = redirectPath_.is_absolute()
        ? redirectPath_
        : projectRoot_ / redirectPath_;
    std::error_code error;
    if (!std::filesystem::exists(absolute, error)) {
        redirects_.clear();
        return true;
    }
    std::vector<EditorAssetRedirect> loaded;
    if (!ParseRedirects(absolute, &loaded, errorMessage)) return false;
    redirects_ = std::move(loaded);
    Touch();
    return true;
}

bool EditorAssetRegistry::RecordRedirect(
    const EditorAssetRecord& from,
    const EditorAssetRecord& to,
    std::string* errorMessage) {
    if (from.kind != to.kind || from.guid != to.guid || !IsDurableEditorAssetGuid(to.guid)) {
        if (errorMessage != nullptr) *errorMessage = "Asset redirect requires one durable GUID.";
        return false;
    }
    std::vector<EditorAssetRedirect> next = redirects_;
    for (EditorAssetRedirect& redirect : next) {
        if (redirect.guid == to.guid && redirect.kind == to.kind) {
            redirect.currentId = to.id;
            redirect.currentLogicalPath = to.logicalPath;
            redirect.currentSourcePath = to.sourcePath;
        }
    }
    const auto duplicate = std::find_if(next.begin(), next.end(),
        [&](const EditorAssetRedirect& redirect) { return SameRedirectAlias(redirect, from); });
    if (duplicate == next.end()) {
        next.push_back({from.kind, from.guid, from.id, from.logicalPath, from.sourcePath,
            to.id, to.logicalPath, to.sourcePath});
    }
    if (!PersistRedirects(next, redirectPath_, projectRoot_, errorMessage)) return false;
    redirects_ = std::move(next);
    Touch();
    return true;
}

bool EditorAssetRegistry::RemoveRedirect(
    const EditorAssetRecord& from,
    const EditorAssetRecord& restored,
    std::string* errorMessage) {
    std::vector<EditorAssetRedirect> next = redirects_;
    next.erase(std::remove_if(next.begin(), next.end(),
        [&](const EditorAssetRedirect& redirect) { return SameRedirectAlias(redirect, from); }), next.end());
    for (EditorAssetRedirect& redirect : next) {
        if (redirect.guid == restored.guid && redirect.kind == restored.kind) {
            redirect.currentId = restored.id;
            redirect.currentLogicalPath = restored.logicalPath;
            redirect.currentSourcePath = restored.sourcePath;
        }
    }
    if (!PersistRedirects(next, redirectPath_, projectRoot_, errorMessage)) return false;
    redirects_ = std::move(next);
    Touch();
    return true;
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
    case EditorAssetKind::Prefab:
        return "Prefab";
    case EditorAssetKind::MaterialGraph:
        return "MaterialGraph";
    case EditorAssetKind::MaterialInstance:
        return "MaterialInstance";
    case EditorAssetKind::VfxGraph:
        return "VfxGraph";
    case EditorAssetKind::AnimationStateMachine:
        return "AnimationStateMachine";
    case EditorAssetKind::GameplayVisualScript:
        return "GameplayVisualScript";
    case EditorAssetKind::BehaviorTree:
        return "BehaviorTree";
    case EditorAssetKind::EnvironmentQuery:
        return "EnvironmentQuery";
    case EditorAssetKind::NavigationData:
        return "NavigationData";
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
    } else if (kindText == ToString(EditorAssetKind::Prefab)) {
        outToken.kind = EditorAssetKind::Prefab;
    } else if (kindText == ToString(EditorAssetKind::MaterialGraph)) {
        outToken.kind = EditorAssetKind::MaterialGraph;
    } else if (kindText == ToString(EditorAssetKind::MaterialInstance)) {
        outToken.kind = EditorAssetKind::MaterialInstance;
    } else if (kindText == ToString(EditorAssetKind::VfxGraph)) {
        outToken.kind = EditorAssetKind::VfxGraph;
    } else if (kindText == ToString(EditorAssetKind::AnimationStateMachine)) {
        outToken.kind = EditorAssetKind::AnimationStateMachine;
    } else if (kindText == ToString(EditorAssetKind::GameplayVisualScript)) {
        outToken.kind = EditorAssetKind::GameplayVisualScript;
    } else if (kindText == ToString(EditorAssetKind::BehaviorTree)) {
        outToken.kind = EditorAssetKind::BehaviorTree;
    } else if (kindText == ToString(EditorAssetKind::EnvironmentQuery)) {
        outToken.kind = EditorAssetKind::EnvironmentQuery;
    } else if (kindText == ToString(EditorAssetKind::NavigationData)) {
        outToken.kind = EditorAssetKind::NavigationData;
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

std::string GenerateEditorAssetGuid() {
    static std::atomic<uint64_t> sequence{1};
    const uint64_t counter = sequence.fetch_add(1, std::memory_order_relaxed);
    const uint64_t ticks = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::string seed = std::to_string(ticks) + ":" + std::to_string(counter);
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16)
           << Hash64(seed, 1469598103934665603ull) << std::setw(16)
           << Hash64(seed, 1099511628211ull);
    return stream.str();
}

bool IsDurableEditorAssetGuid(std::string_view guid) noexcept {
    return !guid.empty() && guid.rfind("auto-", 0) != 0;
}

std::string BuildEditorAssetGuidReference(std::string_view guid) {
    return guid.empty() ? std::string{} : "asset-guid:" + std::string(guid);
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

const char* ToString(EditorAssetReferenceResolutionSource source) {
    switch (source) {
    case EditorAssetReferenceResolutionSource::None: return "None";
    case EditorAssetReferenceResolutionSource::Id: return "Id";
    case EditorAssetReferenceResolutionSource::Guid: return "Guid";
    case EditorAssetReferenceResolutionSource::LogicalPath: return "LogicalPath";
    case EditorAssetReferenceResolutionSource::SourcePath: return "SourcePath";
    case EditorAssetReferenceResolutionSource::Redirect: return "Redirect";
    }
    return "None";
}

} // namespace editor
