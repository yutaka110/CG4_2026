#include "EditorMeshBakePipeline.h"

#include "../core/EditorExecutionContext.h"
#include "../io/EditorFileTransaction.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace editor {
namespace {

void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) *errorMessage = std::move(message);
}

std::optional<std::string> PropertyValue(
    const EditorSceneComponent& component,
    std::string_view name) {
    const auto found = std::find_if(component.properties.begin(), component.properties.end(),
        [&](const EditorSceneProperty& property) { return property.name == name; });
    return found == component.properties.end()
        ? std::optional<std::string>{}
        : std::optional<std::string>{found->value};
}

void ApplyProperty(
    EditorSceneComponent& component,
    std::string_view name,
    const std::optional<std::string>& value) {
    const auto found = std::find_if(component.properties.begin(), component.properties.end(),
        [&](const EditorSceneProperty& property) { return property.name == name; });
    if (!value.has_value()) {
        if (found != component.properties.end()) component.properties.erase(found);
    } else if (found == component.properties.end()) {
        component.properties.push_back({std::string(name), *value});
    } else {
        found->value = *value;
    }
}

std::string SanitizeAssetName(std::string_view requested) {
    std::string output;
    output.reserve((std::min)(requested.size(), std::size_t{64}));
    bool previousUnderscore = false;
    for (unsigned char ch : requested) {
        char value = static_cast<char>(std::tolower(ch));
        if (std::isalnum(ch) == 0 && value != '-' && value != '_') value = '_';
        if (value == '_' && previousUnderscore) continue;
        output.push_back(value);
        previousUnderscore = value == '_';
        if (output.size() == 64) break;
    }
    while (!output.empty() && output.back() == '_') output.pop_back();
    return output;
}

bool ReadOptionalFile(
    const std::filesystem::path& path,
    std::optional<std::vector<uint8_t>>& output,
    std::string* errorMessage,
    bool required) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        output.reset();
        if (required) SetError(errorMessage, "Existing Production Mesh artifact is missing: " + path.generic_string());
        return !required;
    }
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size > EditorCookedMeshArtifact::kMaxArtifactBytes) {
        SetError(errorMessage, "Production Mesh artifact is unreadable or too large: " + path.generic_string());
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        SetError(errorMessage, "Production Mesh artifact could not be opened: " + path.generic_string());
        return false;
    }
    std::vector<uint8_t> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file && !bytes.empty()) {
        SetError(errorMessage, "Production Mesh artifact read was incomplete: " + path.generic_string());
        return false;
    }
    output = std::move(bytes);
    return true;
}

bool ReadAllBytes(const std::filesystem::path& path, std::vector<uint8_t>& output) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0 || static_cast<uint64_t>(size) > EditorCookedMeshArtifact::kMaxArtifactBytes) return false;
    file.seekg(0, std::ios::beg);
    output.resize(static_cast<std::size_t>(size));
    if (!output.empty()) file.read(reinterpret_cast<char*>(output.data()), size);
    return file.good() || file.eof();
}

EditorAtomicFileWriter::Validator SourceValidator() {
    return [](const std::filesystem::path& path, std::string* errorMessage) {
        std::vector<uint8_t> bytes;
        if (!ReadAllBytes(path, bytes)) return false;
        EditorProductionMeshAssetDocument document{};
        return EditorProductionMeshAssetDocument::Deserialize(
            std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
            document, errorMessage);
    };
}

EditorAtomicFileWriter::Validator CookedValidator() {
    return [](const std::filesystem::path& path, std::string* errorMessage) {
        std::vector<uint8_t> bytes;
        if (!ReadAllBytes(path, bytes)) return false;
        EditorCookedMeshArtifact artifact{};
        return EditorCookedMeshArtifact::Deserialize(bytes, artifact, errorMessage);
    };
}

EditorAtomicFileWriter::Validator CollisionValidator() {
    return [](const std::filesystem::path& path, std::string* errorMessage) {
        std::vector<uint8_t> bytes;
        if (!ReadAllBytes(path, bytes)) return false;
        EditorCookedCollisionArtifact artifact{};
        return EditorCookedCollisionArtifact::Deserialize(bytes, artifact, errorMessage);
    };
}

EditorAtomicFileWriter::Validator MetadataValidator(std::string guid) {
    return [guid = std::move(guid)](const std::filesystem::path& path, std::string* errorMessage) {
        std::vector<uint8_t> bytes;
        if (!ReadAllBytes(path, bytes)) return false;
        const std::string text(bytes.begin(), bytes.end());
        const bool valid = text.find("guid=" + guid + "\n") != std::string::npos &&
            text.find("logicalPath=") != std::string::npos;
        if (!valid) SetError(errorMessage, "Production Mesh metadata identity is invalid.");
        return valid;
    };
}

std::size_t OptionalBytes(const std::optional<std::vector<uint8_t>>& bytes) {
    return bytes.has_value() ? bytes->capacity() : 0;
}

std::size_t RecordBytes(const std::optional<EditorAssetRecord>& record) {
    if (!record.has_value()) return 0;
    return sizeof(EditorAssetRecord) + record->id.capacity() + record->guid.capacity() +
        record->logicalPath.capacity() + record->displayName.capacity() +
        record->sourcePath.capacity() + record->metadataPath.capacity();
}

std::size_t SnapshotBytes(const EditorMeshBakeSnapshot& snapshot) {
    std::size_t bytes = sizeof(snapshot) + RecordBytes(snapshot.record) +
        OptionalBytes(snapshot.sourceBytes) + OptionalBytes(snapshot.cookedBytes) +
        OptionalBytes(snapshot.collisionBytes) + OptionalBytes(snapshot.metadataBytes);
    for (const EditorSceneObjectReference& reference : snapshot.componentReferences) {
        bytes += sizeof(reference) + reference.property.capacity() +
            reference.entityGuid.capacity() + reference.assetGuid.capacity();
    }
    for (const auto* value : {&snapshot.bakedGuid, &snapshot.sourceHash, &snapshot.buildHash}) {
        if (value->has_value()) bytes += (*value)->capacity();
    }
    return bytes;
}

bool ApplyRegistrySnapshot(
    EditorAssetRegistry& registry,
    const EditorMeshBakeChange& change,
    const EditorMeshBakeSnapshot& target,
    std::string* errorMessage) {
    const std::string afterGuid = change.after.record.has_value() ? change.after.record->guid : std::string{};
    const EditorAssetRecord* byGuid = !afterGuid.empty() ? registry.FindByGuid(afterGuid) : nullptr;
    if (!target.record.has_value()) {
        if (byGuid == nullptr) return true;
        if (!registry.Remove(byGuid->kind, byGuid->id)) {
            SetError(errorMessage, "Failed to remove Production Mesh from the Asset Registry.");
            return false;
        }
        return true;
    }
    const EditorAssetRecord& targetRecord = *target.record;
    if (const EditorAssetRecord* conflict = registry.Find(targetRecord.kind, targetRecord.id);
        conflict != nullptr && conflict->guid != targetRecord.guid) {
        SetError(errorMessage, "Production Mesh Asset ID is now owned by another GUID.");
        return false;
    }
    if (byGuid != nullptr) {
        if (!registry.Replace(byGuid->kind, byGuid->id, targetRecord)) {
            SetError(errorMessage, "Failed to replace Production Mesh registry state.");
            return false;
        }
        return true;
    }
    if (!registry.Register(targetRecord)) {
        SetError(errorMessage, "Failed to register Production Mesh Asset.");
        return false;
    }
    return true;
}

bool StageFile(
    EditorFileTransaction& transaction,
    const std::filesystem::path& path,
    const std::optional<std::vector<uint8_t>>& bytes,
    EditorAtomicFileWriter::Validator validator,
    std::string* errorMessage) {
    return bytes.has_value()
        ? transaction.StageWrite(path, *bytes, std::move(validator), errorMessage)
        : transaction.StageDelete(path, errorMessage);
}

} // namespace

void EditorMeshBakePipeline::Bind(
    EditorDocumentId document,
    EditorScene* scene,
    EditorAssetRegistry* registry,
    std::filesystem::path projectRoot) {
    document_ = std::move(document);
    scene_ = scene;
    registry_ = registry;
    projectRoot_ = std::move(projectRoot);
}

void EditorMeshBakePipeline::Clear() {
    document_ = {};
    scene_ = nullptr;
    registry_ = nullptr;
    projectRoot_.clear();
}

bool EditorMeshBakePipeline::Prepare(
    std::string_view entityGuid,
    const EditorGeometryMesh& geometry,
    const EditorGeneratedCollision* authoredCollision,
    std::string_view requestedAssetName,
    const EditorMeshBuildSettings& settings,
    EditorMeshBakePrepared& output,
    std::string* errorMessage) const {
    if (!document_.IsValid() || scene_ == nullptr || registry_ == nullptr || projectRoot_.empty()) {
        SetError(errorMessage, "Production Mesh Bake pipeline is not bound to an active Scene.");
        return false;
    }
    EditorSceneEntity* entity = scene_->FindEntity(entityGuid);
    EditorSceneComponent* component = entity != nullptr
        ? scene_->FindComponent(*entity, kEditorMeshRendererComponentType)
        : nullptr;
    if (component == nullptr) {
        SetError(errorMessage, "Production Mesh Bake requires a Scene Mesh Renderer target.");
        return false;
    }
    const std::string sanitized = SanitizeAssetName(requestedAssetName);
    if (sanitized.empty()) {
        SetError(errorMessage, "Production Mesh Asset Name must contain an alphanumeric character.");
        return false;
    }

    const std::optional<std::string> bakedGuid = PropertyValue(*component, kEditorBakedMeshGuidProperty);
    const EditorAssetRecord* existing = bakedGuid.has_value()
        ? registry_->FindByGuid(*bakedGuid)
        : nullptr;
    if (existing != nullptr && (existing->kind != EditorAssetKind::Mesh ||
            std::filesystem::path(existing->sourcePath).extension() != ".mesh")) {
        SetError(errorMessage, "Existing baked Asset reference is not a Production Mesh.");
        return false;
    }

    EditorAssetRecord record{};
    EditorMeshBakeFilePaths paths{};
    const bool rebake = existing != nullptr;
    if (rebake) {
        record = *existing;
        paths.source = record.sourcePath;
    } else {
        if (const EditorAssetRecord* conflict = registry_->Find(EditorAssetKind::Mesh, sanitized)) {
            SetError(errorMessage,
                "Mesh Asset ID already exists with GUID " + conflict->guid + ". Choose another Asset Name.");
            return false;
        }
        record.kind = EditorAssetKind::Mesh;
        record.id = sanitized;
        record.guid = GenerateEditorAssetGuid();
        record.displayName = sanitized;
        record.sourcePath = (std::filesystem::path("Resources") / "Generated" / "Meshes" /
            (sanitized + ".mesh")).generic_string();
        record.logicalPath = record.sourcePath;
        record.metadataPath = record.sourcePath + ".meta";
        record.thumbnailKey = "mesh:" + record.guid;
        record.tags = {"generated", "modeling"};
        record.referenceable = true;
        record.hasMetadata = true;
        record.provisionalGuid = false;
        paths.source = record.sourcePath;
    }
    paths.cooked = EditorCookedMeshPath(paths.source);
    paths.collision = EditorCookedCollisionPath(paths.source);
    paths.metadata = record.metadataPath;
    record.sourceTimestamp = static_cast<uint64_t>(
        std::filesystem::file_time_type::clock::now().time_since_epoch().count());

    EditorProductionMeshAssetDocument document{};
    document.assetGuid = record.guid;
    document.assetId = record.id;
    document.sourceGeometryHash = geometry.ContentHash();
    document.settings = settings;
    document.geometry = geometry;
    EditorCookedMeshArtifact cooked{};
    EditorCookedCollisionArtifact collision{};
    if (!BuildEditorCookedMeshArtifacts(
            geometry, authoredCollision, settings, cooked, collision, errorMessage)) return false;
    std::string sourceText;
    std::vector<uint8_t> cookedBytes;
    std::vector<uint8_t> collisionBytes;
    if (!document.Serialize(sourceText, errorMessage) ||
        !cooked.Serialize(cookedBytes, errorMessage) ||
        (settings.collisionMode != EditorMeshCollisionBuildMode::None &&
            !collision.Serialize(collisionBytes, errorMessage))) return false;

    std::ostringstream metadata;
    metadata << "guid=" << record.guid << '\n';
    metadata << "logicalPath=" << record.logicalPath << '\n';
    metadata << "tags=generated,modeling\n";
    metadata << "sourceGeometryHash=" << document.sourceGeometryHash << '\n';
    metadata << "buildSettingsHash=" << settings.ContentHash() << '\n';
    metadata << "cookedPath=" << paths.cooked.generic_string() << '\n';
    metadata << "collisionPath=" << paths.collision.generic_string() << '\n';

    EditorMeshBakePrepared prepared{};
    prepared.change.documentKey = document_.Key();
    prepared.change.entityGuid = std::string(entityGuid);
    prepared.change.paths = paths;
    prepared.change.before.record = rebake ? std::optional<EditorAssetRecord>{record} : std::nullopt;
    prepared.change.before.componentReferences = component->references;
    prepared.change.before.bakedGuid = PropertyValue(*component, kEditorBakedMeshGuidProperty);
    prepared.change.before.sourceHash = PropertyValue(*component, kEditorBakedMeshSourceHashProperty);
    prepared.change.before.buildHash = PropertyValue(*component, kEditorBakedMeshBuildHashProperty);
    if (!ReadOptionalFile(projectRoot_ / paths.source, prepared.change.before.sourceBytes, errorMessage, rebake) ||
        !ReadOptionalFile(projectRoot_ / paths.cooked, prepared.change.before.cookedBytes, errorMessage, rebake) ||
        !ReadOptionalFile(projectRoot_ / paths.collision, prepared.change.before.collisionBytes, errorMessage, false) ||
        !ReadOptionalFile(projectRoot_ / paths.metadata, prepared.change.before.metadataBytes, errorMessage, rebake)) {
        return false;
    }

    prepared.change.after.record = record;
    prepared.change.after.sourceBytes = std::vector<uint8_t>(sourceText.begin(), sourceText.end());
    prepared.change.after.cookedBytes = std::move(cookedBytes);
    if (settings.collisionMode != EditorMeshCollisionBuildMode::None) {
        prepared.change.after.collisionBytes = std::move(collisionBytes);
    }
    const std::string metadataText = metadata.str();
    prepared.change.after.metadataBytes = std::vector<uint8_t>(metadataText.begin(), metadataText.end());
    prepared.change.after.componentReferences = component->references;
    auto assetReference = std::find_if(
        prepared.change.after.componentReferences.begin(),
        prepared.change.after.componentReferences.end(),
        [](const EditorSceneObjectReference& reference) { return reference.property == "asset"; });
    if (assetReference == prepared.change.after.componentReferences.end()) {
        prepared.change.after.componentReferences.push_back({"asset", {}, record.guid});
    } else {
        assetReference->entityGuid.clear();
        assetReference->assetGuid = record.guid;
    }
    prepared.change.after.bakedGuid = record.guid;
    prepared.change.after.sourceHash = std::to_string(document.sourceGeometryHash);
    prepared.change.after.buildHash = std::to_string(settings.ContentHash());
    prepared.lodCount = static_cast<uint32_t>(cooked.lods.size());
    for (const EditorCookedMeshLod& lod : cooked.lods) {
        prepared.lodTriangleCounts.push_back(static_cast<uint32_t>(lod.indices.size() / 3));
    }
    prepared.collisionTriangles = static_cast<uint32_t>(collision.indices.size() / 3);
    prepared.rebake = rebake;
    prepared.artifactBytes = sourceText.size() + prepared.change.after.cookedBytes->size() +
        (prepared.change.after.collisionBytes.has_value() ? prepared.change.after.collisionBytes->size() : 0) +
        prepared.change.after.metadataBytes->size();
    output = std::move(prepared);
    return true;
}

void EditorMeshBakeExecutionService::Bind(
    EditorDocumentId document,
    EditorScene* scene,
    EditorAssetRegistry* registry,
    EditorProductionMeshRuntimeCache* runtimeCache,
    std::filesystem::path projectRoot,
    ChangedCallback onChanged) {
    document_ = std::move(document);
    scene_ = scene;
    registry_ = registry;
    runtimeCache_ = runtimeCache;
    projectRoot_ = std::move(projectRoot);
    onChanged_ = std::move(onChanged);
}

void EditorMeshBakeExecutionService::Clear() {
    document_ = {};
    scene_ = nullptr;
    registry_ = nullptr;
    runtimeCache_ = nullptr;
    projectRoot_.clear();
    onChanged_ = {};
}

EditorUndoResult EditorMeshBakeExecutionService::ApplyMeshBake(
    const EditorMeshBakeChange& change,
    EditorTransactionApplyMode mode) {
    if (!document_.IsValid() || document_.Key() != change.documentKey ||
        scene_ == nullptr || registry_ == nullptr || projectRoot_.empty()) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Mesh Bake command targets an unavailable Scene or Asset Registry.");
    }
    EditorSceneEntity* entity = scene_->FindEntity(change.entityGuid);
    EditorSceneComponent* component = entity != nullptr
        ? scene_->FindComponent(*entity, kEditorMeshRendererComponentType)
        : nullptr;
    if (component == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::InvalidArgument,
            "Mesh Bake command target has no Mesh Renderer component.");
    }
    const EditorMeshBakeSnapshot& target = mode == EditorTransactionApplyMode::Redo
        ? change.after : change.before;
    const std::string metadataGuid = target.record.has_value()
        ? target.record->guid
        : (change.after.record.has_value() ? change.after.record->guid : std::string{});
    EditorFileTransaction transaction(projectRoot_);
    std::string error;
    if (!StageFile(transaction, change.paths.source, target.sourceBytes, SourceValidator(), &error) ||
        !StageFile(transaction, change.paths.cooked, target.cookedBytes, CookedValidator(), &error) ||
        !StageFile(transaction, change.paths.collision, target.collisionBytes, CollisionValidator(), &error) ||
        !StageFile(transaction, change.paths.metadata, target.metadataBytes, MetadataValidator(metadataGuid), &error)) {
        return EditorUndoResult::Failure(EditorErrorCode::InvalidArgument,
            error.empty() ? "Mesh Bake file transaction could not be staged." : error);
    }
    EditorFileTransactionReceipt receipt{};
    if (!transaction.ApplyPrepared(&receipt, &error)) {
        return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed,
            error.empty() ? "Mesh Bake file transaction prepare failed." : error);
    }

    EditorAssetRegistry registryBackup = *registry_;
    const EditorSceneComponent componentBackup = *component;
    const uint64_t sceneRevision = scene_->revision;
    if (!ApplyRegistrySnapshot(*registry_, change, target, &error)) {
        *registry_ = std::move(registryBackup);
        transaction.RollbackPrepared(&receipt, nullptr);
        return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, error);
    }
    component->references = target.componentReferences;
    ApplyProperty(*component, kEditorBakedMeshGuidProperty, target.bakedGuid);
    ApplyProperty(*component, kEditorBakedMeshSourceHashProperty, target.sourceHash);
    ApplyProperty(*component, kEditorBakedMeshBuildHashProperty, target.buildHash);
    scene_->Touch();
    if (!transaction.CommitPrepared(&receipt, &error)) {
        *component = componentBackup;
        scene_->revision = sceneRevision;
        *registry_ = std::move(registryBackup);
        transaction.RollbackPrepared(&receipt, nullptr);
        return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed,
            error.empty() ? "Mesh Bake file transaction commit failed." : error);
    }

    const std::string assetGuid = change.after.record.has_value() ? change.after.record->guid : std::string{};
    if (runtimeCache_ != nullptr && !assetGuid.empty()) {
        runtimeCache_->Invalidate(assetGuid);
        if (target.record.has_value()) {
            EditorAssetRecord runtimeRecord = *target.record;
            runtimeRecord.sourcePath = (projectRoot_ / change.paths.source).generic_string();
            runtimeCache_->Load(runtimeRecord, nullptr);
        }
    }
    registry_->ScanDependencies();
    if (onChanged_) onChanged_(change.entityGuid, assetGuid);
    return EditorUndoResult::Success(
        mode == EditorTransactionApplyMode::Redo
            ? "Production Mesh Asset baked atomically."
            : "Production Mesh Bake reverted atomically.");
}

EditorMeshBakeUndoCommand::EditorMeshBakeUndoCommand(EditorMeshBakeChange change)
    : change_(std::move(change)) {}

EditorUndoResult EditorMeshBakeUndoCommand::Apply(
    EditorTransactionApplyMode mode,
    EditorExecutionContext& context) const {
    auto* service = dynamic_cast<IEditorMeshBakeExecutionService*>(
        context.Find(IEditorMeshBakeExecutionService::kServiceId));
    if (service == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Mesh Bake execution service is unavailable.");
    }
    return service->ApplyMeshBake(change_, mode);
}

std::size_t EditorMeshBakeUndoCommand::EstimatedBytes() const noexcept {
    return sizeof(*this) + change_.documentKey.capacity() + change_.entityGuid.capacity() +
        change_.paths.source.native().capacity() + change_.paths.cooked.native().capacity() +
        change_.paths.collision.native().capacity() + change_.paths.metadata.native().capacity() +
        SnapshotBytes(change_.before) + SnapshotBytes(change_.after);
}

} // namespace editor
