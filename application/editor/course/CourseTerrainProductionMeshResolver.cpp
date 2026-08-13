#include "CourseTerrainProductionMeshResolver.h"

#include "../mesh/EditorObjProductionImportBridge.h"
#include "../mesh/EditorProductionMeshEditableSourceLoader.h"

#include <algorithm>
#include <cctype>
#include <utility>
#include <vector>

namespace editor {
namespace {

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string NormalizeAlias(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const unsigned char ch : value) {
        if (std::isalnum(ch) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return normalized;
}

std::string PathStem(const std::string& value) {
    return std::filesystem::path(value).stem().string();
}

int MatchScore(const EditorAssetRecord& record, std::string_view requested,
    std::string_view normalized) {
    if (record.kind != EditorAssetKind::Mesh || record.missing) return 0;
    if (record.id == requested) return 1000;
    if (NormalizeAlias(record.id) == normalized) return 900;
    if (!record.sourcePath.empty() &&
        NormalizeAlias(PathStem(record.sourcePath)) == normalized) return 800;
    if (!record.logicalPath.empty() &&
        NormalizeAlias(PathStem(record.logicalPath)) == normalized) return 750;
    if (!record.displayName.empty() &&
        NormalizeAlias(record.displayName) == normalized) return 700;
    return 0;
}

const EditorAssetRecord* ResolveAliasRecord(
    const EditorAssetRegistry& assets,
    std::string_view requested,
    bool& normalizedAlias,
    bool& ambiguous) {
    const EditorAssetReferenceResolution direct = assets.ResolveReference(
        EditorAssetKind::Mesh, requested);
    if (direct.resolved && direct.record != nullptr && !direct.record->missing) {
        return direct.record;
    }
    const std::string normalized = NormalizeAlias(requested);
    if (normalized.empty()) return nullptr;
    const EditorAssetRecord* best = nullptr;
    int bestScore = 0;
    for (const EditorAssetRecord* candidate : assets.List(EditorAssetKind::Mesh)) {
        if (candidate == nullptr) continue;
        const int score = MatchScore(*candidate, requested, normalized);
        if (score > bestScore) {
            best = candidate;
            bestScore = score;
            ambiguous = false;
        } else if (score != 0 && score == bestScore && best != nullptr &&
            best->guid != candidate->guid) {
            ambiguous = true;
        }
    }
    if (ambiguous) return nullptr;
    normalizedAlias = best != nullptr;
    return best;
}

const EditorAssetRecord* FindProductionAsset(
    const EditorAssetRegistry& assets,
    const EditorAssetRecord& source,
    const std::filesystem::path& projectRoot) {
    const std::string sourceExtension = Lowercase(
        std::filesystem::path(source.sourcePath).extension().string());
    if (sourceExtension == ".mesh") return &source;
    const EditorAssetRecord* best = nullptr;
    for (const EditorAssetRecord* candidate : assets.List(EditorAssetKind::Mesh)) {
        if (candidate == nullptr || !EditorObjProductionImportBridge::
                IsProductionForSource(*candidate, source, projectRoot)) {
            continue;
        }
        if (best == nullptr || candidate->id < best->id) best = candidate;
    }
    return best;
}

} // namespace

CourseTerrainProductionMeshResolver::CourseTerrainProductionMeshResolver(
    std::filesystem::path projectRoot)
    : projectRoot_(std::move(projectRoot)) {}

const CourseTerrainProductionMeshResolution&
CourseTerrainProductionMeshResolver::Resolve(
    const EditorAssetRegistry& assets,
    std::string_view courseMeshId,
    bool* cacheHit) const {
    if (cacheHit != nullptr) *cacheHit = false;
    if (cachedRegistry_ != &assets ||
        cachedRegistryRevision_ != assets.Revision()) {
        cache_.clear();
        cachedRegistry_ = &assets;
        cachedRegistryRevision_ = assets.Revision();
    }
    const std::string key(courseMeshId);
    if (const auto found = cache_.find(key); found != cache_.end()) {
        ++stats_.cacheHits;
        if (cacheHit != nullptr) *cacheHit = true;
        return found->second;
    }
    ++stats_.resolutions;
    CourseTerrainProductionMeshResolution resolved =
        ResolveUncached(assets, courseMeshId);
    if (resolved.ambiguous) ++stats_.ambiguous;
    if (!resolved.Resolved()) ++stats_.unresolved;
    else if (resolved.source ==
        CourseTerrainMeshResolutionSource::ProductionAsset) {
        ++stats_.productionAssets;
    } else {
        ++stats_.importedAuthoringSources;
    }
    if (resolved.normalizedAlias) ++stats_.normalizedAliases;
    else if (resolved.Resolved()) ++stats_.directReferences;
    return cache_.emplace(key, std::move(resolved)).first->second;
}

CourseTerrainProductionMeshResolution
CourseTerrainProductionMeshResolver::ResolveUncached(
    const EditorAssetRegistry& assets,
    std::string_view courseMeshId) const {
    CourseTerrainProductionMeshResolution result{};
    result.requestedMeshId = std::string(courseMeshId);
    if (courseMeshId.empty()) {
        result.message = "Course terrain Mesh ID is empty.";
        return result;
    }
    bool normalizedAlias = false;
    bool ambiguous = false;
    const EditorAssetRecord* source = ResolveAliasRecord(
        assets, courseMeshId, normalizedAlias, ambiguous);
    result.normalizedAlias = normalizedAlias;
    result.ambiguous = ambiguous;
    if (source == nullptr) {
        result.message = ambiguous
            ? "Course terrain Mesh ID is ambiguous after alias normalization."
            : "Course terrain Mesh ID was not found in the Asset Registry.";
        return result;
    }
    result.canonicalMeshId = source->id;
    const EditorAssetReferenceResolution reference = assets.ResolveReference(
        EditorAssetKind::Mesh, source->id);
    result.referenceSource = normalizedAlias
        ? EditorAssetReferenceResolutionSource::Id : reference.source;

    const EditorAssetRecord* production = FindProductionAsset(
        assets, *source, projectRoot_);
    if (production != nullptr && IsDurableEditorAssetGuid(production->guid)) {
        EditorProductionMeshEditableSourceLoader loader(projectRoot_);
        const EditorProductionMeshEditableSourceLoadResult loaded =
            loader.Load(assets, production->guid);
        if (loaded.Succeeded()) {
            result.source = CourseTerrainMeshResolutionSource::ProductionAsset;
            result.resolvedAssetId = production->id;
            result.resolvedAssetGuid = production->guid;
            result.sourcePath = loaded.source.sourcePath;
            result.sourceGeometryHash = loaded.source.sourceGeometryHash;
            result.geometry = loaded.source.geometry;
            result.message = "Course Mesh ID resolved to a Production Mesh Asset.";
            return result;
        }
        result.message = "Production Mesh Asset was found but could not be read: " +
            loaded.message;
    }

    uint64_t fileHash = 0;
    std::vector<std::string> diagnostics;
    std::string importError;
    if (EditorObjProductionImportBridge::LoadSourceGeometry(
            *source, projectRoot_, result.geometry, &fileHash,
            nullptr, &diagnostics, &importError)) {
        result.source =
            CourseTerrainMeshResolutionSource::ImportedAuthoringSource;
        result.resolvedAssetId = source->id;
        result.resolvedAssetGuid = source->guid;
        result.sourcePath = source->sourcePath;
        result.sourceFileHash = fileHash;
        result.sourceGeometryHash = result.geometry.ContentHash();
        result.message =
            "Course Mesh ID resolved through the Production Import geometry path.";
        return result;
    }
    if (!importError.empty()) {
        if (!result.message.empty()) result.message += " ";
        result.message += importError;
    }
    if (result.message.empty()) {
        result.message = "Course terrain Mesh has no Production Geometry source.";
    }
    result.geometry = {};
    return result;
}

void CourseTerrainProductionMeshResolver::Invalidate() const {
    cache_.clear();
    cachedRegistry_ = nullptr;
    cachedRegistryRevision_ = 0;
}

const char* ToString(CourseTerrainMeshResolutionSource source) noexcept {
    switch (source) {
    case CourseTerrainMeshResolutionSource::None: return "None";
    case CourseTerrainMeshResolutionSource::ProductionAsset:
        return "Production Asset";
    case CourseTerrainMeshResolutionSource::ImportedAuthoringSource:
        return "Production Import Source";
    }
    return "Unknown";
}

} // namespace editor
