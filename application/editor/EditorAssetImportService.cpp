#include "EditorAssetImportService.h"

#include "EditorAssetThumbnailService.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace editor {
namespace {

std::string ToLower(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

std::string NormalizePath(std::filesystem::path path) {
    std::string text = path.generic_string();
    if (text.rfind("./", 0) == 0) {
        text.erase(0, 2);
    }
    return text;
}

std::string Trim(std::string value) {
    const auto first = std::find_if(
        value.begin(),
        value.end(),
        [](unsigned char ch) {
            return !std::isspace(ch);
        });
    const auto last = std::find_if(
        value.rbegin(),
        value.rend(),
        [](unsigned char ch) {
            return !std::isspace(ch);
        }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

std::vector<std::string> SplitList(const std::string& text) {
    std::vector<std::string> values;
    std::string token;
    std::stringstream stream(text);
    while (std::getline(stream, token, ',')) {
        const std::string trimmed = Trim(token);
        if (!trimmed.empty()) {
            values.push_back(trimmed);
        }
    }
    return values;
}

uint64_t SourceTimestamp(const std::filesystem::path& path) {
    std::error_code error;
    const auto timestamp = std::filesystem::last_write_time(path, error);
    if (error) {
        return 0;
    }
    return static_cast<uint64_t>(timestamp.time_since_epoch().count());
}

bool PathStartsWith(
    const std::filesystem::path& path,
    const std::filesystem::path& root) {
    auto pathIt = path.begin();
    auto rootIt = root.begin();
    for (; rootIt != root.end(); ++rootIt, ++pathIt) {
        if (pathIt == path.end() || *pathIt != *rootIt) {
            return false;
        }
    }
    return true;
}

bool IsSafeRelativeFolder(const std::filesystem::path& folder) {
    if (folder.empty() || folder.is_absolute()) {
        return false;
    }
    for (const std::filesystem::path& part : folder) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

bool ResolveResourcePaths(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& resourcesRoot,
    std::filesystem::path& outPhysicalSource,
    std::filesystem::path& outRelativeSource,
    std::string* errorMessage) {
    std::error_code error;
    const std::filesystem::path rootAbsolute =
        std::filesystem::absolute(resourcesRoot, error).lexically_normal();
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to resolve Resources root: " + error.message();
        }
        return false;
    }

    const std::filesystem::path sourceAbsolute =
        std::filesystem::absolute(sourcePath, error).lexically_normal();
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to resolve source path: " + error.message();
        }
        return false;
    }

    if (!PathStartsWith(sourceAbsolute, rootAbsolute)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Asset import source must be under Resources for this pipeline stage.";
        }
        return false;
    }

    if (!std::filesystem::exists(sourceAbsolute, error) ||
        !std::filesystem::is_regular_file(sourceAbsolute, error)) {
        if (errorMessage != nullptr) {
            *errorMessage = "Asset import source file does not exist.";
        }
        return false;
    }

    std::filesystem::path relative = std::filesystem::relative(sourceAbsolute, rootAbsolute, error);
    if (error) {
        relative = sourceAbsolute.filename();
        error.clear();
    }

    outPhysicalSource = sourceAbsolute;
    outRelativeSource = relative.lexically_normal();
    return true;
}

std::filesystem::path MakeUniqueDestinationPath(
    const std::filesystem::path& destinationPath) {
    if (!std::filesystem::exists(destinationPath)) {
        return destinationPath;
    }

    const std::filesystem::path parent = destinationPath.parent_path();
    const std::string stem = destinationPath.stem().string();
    const std::string extension = destinationPath.extension().string();
    for (int index = 1; index < 10000; ++index) {
        std::filesystem::path candidate =
            parent / (stem + "_" + std::to_string(index) + extension);
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

bool ResolveExternalImportDestination(
    const std::filesystem::path& externalSourcePath,
    const EditorAssetExternalImportPolicy& policy,
    std::filesystem::path& outExternalSource,
    std::filesystem::path& outDestination,
    std::string* errorMessage) {
    std::error_code error;
    const std::filesystem::path sourceAbsolute =
        std::filesystem::absolute(externalSourcePath, error).lexically_normal();
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to resolve external import source: " + error.message();
        }
        return false;
    }
    if (!std::filesystem::exists(sourceAbsolute, error) ||
        !std::filesystem::is_regular_file(sourceAbsolute, error)) {
        if (errorMessage != nullptr) {
            *errorMessage = "External import source file does not exist.";
        }
        return false;
    }
    if (policy.maxFileBytes > 0) {
        const std::uintmax_t fileSize = std::filesystem::file_size(sourceAbsolute, error);
        if (!error && fileSize > policy.maxFileBytes) {
            if (errorMessage != nullptr) {
                *errorMessage = "External import source exceeds import size limit.";
            }
            return false;
        }
        error.clear();
    }

    EditorAssetImportOptions classificationOptions{};
    if (EditorAssetKindForImportPath(sourceAbsolute, classificationOptions) == EditorAssetKind::Unknown) {
        if (errorMessage != nullptr) {
            *errorMessage = "Unsupported asset type for external import.";
        }
        return false;
    }

    const std::filesystem::path destinationFolder =
        policy.destinationFolder.empty()
            ? std::filesystem::path{"Imported"}
            : policy.destinationFolder.lexically_normal();
    if (!IsSafeRelativeFolder(destinationFolder)) {
        if (errorMessage != nullptr) {
            *errorMessage = "External import destination must be a relative folder under Resources.";
        }
        return false;
    }

    const std::filesystem::path rootAbsolute =
        std::filesystem::absolute(policy.resourcesRoot, error).lexically_normal();
    if (error) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to resolve Resources root: " + error.message();
        }
        return false;
    }

    std::filesystem::path destination =
        (rootAbsolute / destinationFolder / sourceAbsolute.filename()).lexically_normal();
    if (!PathStartsWith(destination, rootAbsolute)) {
        if (errorMessage != nullptr) {
            *errorMessage = "External import destination escaped Resources.";
        }
        return false;
    }

    if (std::filesystem::exists(destination, error)) {
        error.clear();
        if (policy.collisionPolicy == EditorAssetExternalImportCollisionPolicy::Skip) {
            if (errorMessage != nullptr) {
                *errorMessage = "External import skipped because destination already exists.";
            }
            return false;
        }
        if (policy.collisionPolicy == EditorAssetExternalImportCollisionPolicy::Rename) {
            destination = MakeUniqueDestinationPath(destination);
            if (destination.empty()) {
                if (errorMessage != nullptr) {
                    *errorMessage = "Could not allocate a unique external import destination.";
                }
                return false;
            }
        }
    }

    outExternalSource = sourceAbsolute;
    outDestination = destination;
    return true;
}

bool ReadAssetMetadata(
    const std::filesystem::path& physicalMetaPath,
    EditorAssetRecord& record) {
    std::ifstream file(physicalMetaPath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = Trim(line.substr(0, equals));
        const std::string value = Trim(line.substr(equals + 1));
        if (key == "guid") {
            record.guid = value;
            record.provisionalGuid = false;
        } else if (key == "logicalPath") {
            record.logicalPath = value;
        } else if (key == "tags") {
            record.tags = SplitList(value);
        } else if (key == "dependencies") {
            record.dependencies = SplitList(value);
        }
    }

    record.hasMetadata = true;
    return true;
}

bool WriteAssetMetadata(
    const EditorAssetRecord& record,
    std::string* errorMessage) {
    if (record.metadataPath.empty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Asset metadata path is empty.";
        }
        return false;
    }

    std::error_code error;
    const std::filesystem::path metadataPath(record.metadataPath);
    if (metadataPath.has_parent_path()) {
        std::filesystem::create_directories(metadataPath.parent_path(), error);
        if (error) {
            if (errorMessage != nullptr) {
                *errorMessage = "Failed to create metadata directory: " + error.message();
            }
            return false;
        }
    }

    std::ofstream file(metadataPath, std::ios::trunc);
    if (!file.is_open()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Failed to write metadata file.";
        }
        return false;
    }

    file << "guid=" << record.guid << '\n';
    file << "logicalPath=" << (record.logicalPath.empty() ? record.sourcePath : record.logicalPath) << '\n';
    if (!record.tags.empty()) {
        file << "tags=";
        for (std::size_t i = 0; i < record.tags.size(); ++i) {
            if (i > 0) {
                file << ',';
            }
            file << record.tags[i];
        }
        file << '\n';
    }
    if (!record.dependencies.empty()) {
        file << "dependencies=";
        for (std::size_t i = 0; i < record.dependencies.size(); ++i) {
            if (i > 0) {
                file << ',';
            }
            file << record.dependencies[i];
        }
        file << '\n';
    }
    return true;
}

EditorAssetImportResult Fail(std::string message) {
    EditorAssetImportResult result{};
    result.message = std::move(message);
    return result;
}

EditorAssetRecord BuildRecordForSource(
    const std::filesystem::path& physicalSource,
    const std::filesystem::path& relativeSource,
    const std::filesystem::path& resourcesRoot,
    const EditorAssetImportOptions& options) {
    EditorAssetRecord record{};
    record.kind = EditorAssetKindForImportPath(physicalSource, options);
    record.id = BuildEditorAssetIdForImportPath(record.kind, relativeSource);
    record.displayName = physicalSource.stem().string();
    record.sourcePath = NormalizePath(resourcesRoot / relativeSource);
    record.logicalPath = record.sourcePath;
    record.metadataPath = record.sourcePath + ".meta";
    record.sourceTimestamp = SourceTimestamp(physicalSource);
    record.referenceable = record.kind != EditorAssetKind::Mesh;

    std::error_code error;
    record.missing = !std::filesystem::exists(physicalSource, error);
    error.clear();
    ReadAssetMetadata(physicalSource.string() + ".meta", record);
    return record;
}

void PreserveExistingIdentity(
    const EditorAssetRecord& existing,
    EditorAssetRecord& imported) {
    if (!existing.guid.empty()) {
        imported.guid = existing.guid;
        imported.provisionalGuid = existing.provisionalGuid;
    }
    if (!existing.logicalPath.empty() && !existing.provisionalGuid) {
        imported.logicalPath = existing.logicalPath;
    }
    if (!existing.metadataPath.empty()) {
        imported.metadataPath = existing.metadataPath;
    }
    if (imported.tags.empty()) {
        imported.tags = existing.tags;
    }
}

} // namespace

EditorAssetImportService::EditorAssetImportService(
    EditorAssetRegistry& registry,
    EditorAssetThumbnailService* thumbnails)
    : registry_(registry)
    , thumbnails_(thumbnails) {
}

EditorAssetImportResult EditorAssetImportService::Import(
    const std::filesystem::path& sourcePath,
    const EditorAssetImportOptions& options) {
    std::filesystem::path physicalSource;
    std::filesystem::path relativeSource;
    std::string error;
    if (!ResolveResourcePaths(
            sourcePath,
            options.resourcesRoot,
            physicalSource,
            relativeSource,
            &error)) {
        return Fail(error);
    }

    EditorAssetRecord record =
        BuildRecordForSource(physicalSource, relativeSource, options.resourcesRoot, options);
    if (record.kind == EditorAssetKind::Unknown || record.id.empty()) {
        return Fail("Unsupported asset type for import.");
    }

    if (const EditorAssetRecord* existing = registry_.Find(record.kind, record.id)) {
        if (!options.replaceExisting) {
            return Fail("Asset already exists and replaceExisting is disabled.");
        }
        PreserveExistingIdentity(*existing, record);
    }

    EnsureEditorAssetIdentity(record);
    if (options.createMetadata) {
        record.hasMetadata = true;
        record.provisionalGuid = false;
        if (!WriteAssetMetadata(record, &error)) {
            return Fail(error.empty() ? std::string("Failed to write imported asset metadata.") : error);
        }
    }

    if (!registry_.Register(record)) {
        return Fail("Failed to register imported asset.");
    }

    FinalizeAssetRegistryChange(options);

    EditorAssetImportResult result{};
    result.succeeded = true;
    result.message = "Imported " + std::string(ToString(record.kind)) + ":" + record.id;
    result.record = std::move(record);
    result.importedCount = 1;
    return result;
}

EditorAssetImportResult EditorAssetImportService::ImportExternal(
    const std::filesystem::path& externalSourcePath,
    const EditorAssetExternalImportPolicy& policy) {
    std::filesystem::path externalSource;
    std::filesystem::path destination;
    std::string error;
    if (!ResolveExternalImportDestination(
            externalSourcePath,
            policy,
            externalSource,
            destination,
            &error)) {
        EditorAssetImportResult result = Fail(error);
        result.skippedCount = 1;
        result.warning = policy.collisionPolicy == EditorAssetExternalImportCollisionPolicy::Skip;
        return result;
    }

    std::error_code fsError;
    std::filesystem::create_directories(destination.parent_path(), fsError);
    if (fsError) {
        return Fail("Failed to create external import destination: " + fsError.message());
    }

    const bool destinationExisted = std::filesystem::exists(destination, fsError);
    fsError.clear();
    const std::filesystem::path externalMeta = externalSource.string() + ".meta";
    const std::filesystem::path destinationMeta = destination.string() + ".meta";
    const bool destinationMetaExisted = std::filesystem::exists(destinationMeta, fsError);
    fsError.clear();
    const std::filesystem::copy_options copyOptions =
        policy.collisionPolicy == EditorAssetExternalImportCollisionPolicy::Overwrite
            ? std::filesystem::copy_options::overwrite_existing
            : std::filesystem::copy_options::none;
    std::filesystem::copy_file(externalSource, destination, copyOptions, fsError);
    if (fsError) {
        return Fail("Failed to copy external import source: " + fsError.message());
    }

    bool sidecarCopied = false;
    if (policy.copySidecarMetadata && std::filesystem::exists(externalMeta, fsError)) {
        fsError.clear();
        std::filesystem::copy_file(externalMeta, destinationMeta, copyOptions, fsError);
        sidecarCopied = !fsError;
        fsError.clear();
    }

    EditorAssetImportOptions importOptions{};
    importOptions.resourcesRoot = policy.resourcesRoot;
    importOptions.createMetadata = policy.createMetadata;
    importOptions.scanDependencies = policy.scanDependencies;
    importOptions.replaceExisting = policy.replaceExisting;
    EditorAssetImportResult result = Import(destination, importOptions);
    if (!result.succeeded) {
        if (!destinationExisted) {
            std::filesystem::remove(destination, fsError);
            fsError.clear();
        }
        if (!destinationMetaExisted && (sidecarCopied || policy.createMetadata)) {
            std::filesystem::remove(destinationMeta, fsError);
            fsError.clear();
        }
        result.message = "External import failed: " + result.message;
        return result;
    }

    result.message =
        "Imported external asset to " +
        NormalizePath(destination) +
        " as " +
        std::string(ToString(result.record.kind)) +
        ":" +
        result.record.id;
    return result;
}

EditorAssetImportResult EditorAssetImportService::Reimport(
    EditorAssetKind kind,
    std::string_view id,
    const EditorAssetImportOptions& options) {
    const EditorAssetRecord* existing = registry_.Find(kind, id);
    if (existing == nullptr) {
        return Fail("Cannot reimport asset because it is not registered.");
    }
    if (existing->runtimeOnly) {
        return Fail("Cannot reimport runtime-only asset.");
    }
    if (existing->sourcePath.empty()) {
        return Fail("Cannot reimport asset without a source path.");
    }

    const EditorAssetRecord existingSnapshot = *existing;
    EditorAssetImportResult result = Import(existingSnapshot.sourcePath, options);
    if (!result.succeeded) {
        result.message = "Reimport failed: " + result.message;
        return result;
    }

    result.message = "Reimported " +
        std::string(ToString(existingSnapshot.kind)) +
        ":" +
        existingSnapshot.id +
        " with stable GUID.";
    return result;
}

EditorAssetImportResult EditorAssetImportService::BatchMigrateMetadata(
    const EditorAssetImportOptions& options) {
    const std::vector<EditorAssetRecord> records(registry_.Records().begin(), registry_.Records().end());
    EditorAssetImportResult result{};

    for (EditorAssetRecord record : records) {
        if (record.runtimeOnly || record.missing || record.kind == EditorAssetKind::Unknown) {
            ++result.skippedCount;
            continue;
        }
        if (record.hasMetadata && !record.provisionalGuid) {
            ++result.skippedCount;
            continue;
        }
        if (record.sourcePath.empty()) {
            ++result.skippedCount;
            continue;
        }

        EnsureEditorAssetIdentity(record);
        record.hasMetadata = true;
        record.provisionalGuid = false;

        std::string error;
        if (!WriteAssetMetadata(record, &error)) {
            result.warning = true;
            ++result.skippedCount;
            continue;
        }
        if (registry_.Register(record)) {
            ++result.migratedCount;
            result.record = std::move(record);
        } else {
            result.warning = true;
            ++result.skippedCount;
        }
    }

    FinalizeAssetRegistryChange(options);
    result.succeeded = true;
    result.message =
        "Batch metadata migration migrated " +
        std::to_string(result.migratedCount) +
        " assets, skipped " +
        std::to_string(result.skippedCount) +
        ".";
    return result;
}

void EditorAssetImportService::FinalizeAssetRegistryChange(
    const EditorAssetImportOptions& options) {
    if (options.scanDependencies) {
        registry_.ScanDependencies();
    }
    if (thumbnails_ != nullptr) {
        thumbnails_->Sync(registry_);
    }
}

EditorAssetKind EditorAssetKindForImportPath(
    const std::filesystem::path& path,
    const EditorAssetImportOptions& options) {
    const std::string ext = ToLower(path.extension().string());
    if (options.includeMeshes &&
        (ext == ".mesh" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx")) {
        return EditorAssetKind::Mesh;
    }
    if (options.includeEffects && ext == ".effect") {
        return EditorAssetKind::Effect;
    }
    if (options.includeCourseAssets &&
        (ext == ".course" ||
            ext == ".json" ||
            ext == ".actor" ||
            ext == ".wave" ||
            ext == ".pattern" ||
            ext == ".obstacle" ||
            ext == ".terrainpreset" ||
            ext == ".postpreset")) {
        return EditorAssetKind::Course;
    }
    if (options.includeTextures &&
        (ext == ".png" || ext == ".bmp" || ext == ".dds" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga")) {
        return EditorAssetKind::Texture;
    }
    if (options.includeAudio &&
        (ext == ".wav" || ext == ".mp3" || ext == ".ogg")) {
        return EditorAssetKind::Audio;
    }
    return EditorAssetKind::Unknown;
}

std::string BuildEditorAssetIdForImportPath(
    EditorAssetKind kind,
    const std::filesystem::path& relativePath) {
    std::filesystem::path idPath = relativePath;
    idPath.replace_extension();

    if (kind == EditorAssetKind::Mesh) {
        return idPath.filename().generic_string();
    }

    return NormalizePath(idPath);
}

const char* ToString(EditorAssetExternalImportCollisionPolicy policy) {
    switch (policy) {
    case EditorAssetExternalImportCollisionPolicy::Skip:
        return "Skip";
    case EditorAssetExternalImportCollisionPolicy::Overwrite:
        return "Overwrite";
    case EditorAssetExternalImportCollisionPolicy::Rename:
        return "Rename";
    }
    return "Rename";
}

} // namespace editor
