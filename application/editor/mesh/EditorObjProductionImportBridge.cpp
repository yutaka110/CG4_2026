#include "EditorObjProductionImportBridge.h"

#include "../io/EditorFileTransaction.h"
#include "../world/EditorWorldObjectRecord.h"
#include "../../ModelLoaderAssimp.h"
#include "utils/math/MathUtils.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

void SetError(EditorObjProductionImportResult& result, std::string message) {
    result.message = std::move(message);
    result.diagnostics.push_back(result.message);
}

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return value;
}

std::string SanitizeAssetName(std::string_view requested) {
    std::string output;
    output.reserve((std::min)(requested.size(), std::size_t{64}));
    bool previousUnderscore = false;
    for (const unsigned char byte : requested) {
        char value = static_cast<char>(std::tolower(byte));
        if (std::isalnum(byte) == 0 && value != '-' && value != '_') value = '_';
        if (value == '_' && previousUnderscore) continue;
        output.push_back(value);
        previousUnderscore = value == '_';
        if (output.size() == 64) break;
    }
    while (!output.empty() && output.back() == '_') output.pop_back();
    return output;
}

std::filesystem::path ResolveProjectPath(
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& path) {
    return (path.is_absolute() ? path : projectRoot / path).lexically_normal();
}

uint64_t HashFile(const std::filesystem::path& path, std::string* errorMessage) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (errorMessage != nullptr) {
            *errorMessage = "OBJ source could not be opened: " + path.generic_string();
        }
        return 0;
    }
    uint64_t hash = 1469598103934665603ull;
    char buffer[64 * 1024];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const std::streamsize count = input.gcount();
        for (std::streamsize index = 0; index < count; ++index) {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= 1099511628211ull;
        }
    }
    if (!input.eof()) {
        if (errorMessage != nullptr) {
            *errorMessage = "OBJ source could not be read completely: " + path.generic_string();
        }
        return 0;
    }
    return hash;
}

uint64_t CurrentTimestamp() {
    return static_cast<uint64_t>(
        std::filesystem::file_time_type::clock::now().time_since_epoch().count());
}

Vector3 Subtract(const Vector3& left, const Vector3& right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vector3 Cross(const Vector3& left, const Vector3& right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x};
}

float LengthSquared(const Vector3& value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

std::string StableElementGuid(
    std::string_view sourceIdentity,
    char elementKind,
    std::size_t index) {
    return "obj-" + std::string(sourceIdentity) + "-" + elementKind + "-" +
        std::to_string(index);
}

bool ConvertObjModel(
    const EditorAssetRecord& source,
    const ModelData& model,
    EditorGeometryMesh& geometry,
    uint32_t& materialSlotCount,
    std::vector<std::string>& diagnostics,
    std::string* errorMessage) {
    if (model.vertices.empty() || model.indices.size() < 3 ||
        model.indices.size() % 3 != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = "OBJ importer produced no triangulated geometry.";
        }
        return false;
    }
    if (model.vertices.size() > EditorGeometryMesh::kMaxVertices ||
        model.indices.size() / 3 > EditorGeometryMesh::kMaxTriangles) {
        if (errorMessage != nullptr) {
            *errorMessage =
                "OBJ exceeds the Production Mesh authoring budget (65535 vertices / "
                "131072 triangles).";
        }
        return false;
    }

    const std::string sourceIdentity = !source.guid.empty()
        ? source.guid
        : BuildEditorAssetProvisionalGuid(source.kind, source.logicalPath);
    geometry.vertices.reserve(model.vertices.size());
    for (std::size_t index = 0; index < model.vertices.size(); ++index) {
        const VertexData& sourceVertex = model.vertices[index];
        EditorGeometryVertex vertex{};
        vertex.guid = StableElementGuid(sourceIdentity, 'v', index);
        vertex.position = {
            sourceVertex.position.x,
            sourceVertex.position.y,
            sourceVertex.position.z};
        vertex.normal = sourceVertex.normal;
        vertex.u = sourceVertex.texcoord.x;
        vertex.v = sourceVertex.texcoord.y;
        geometry.vertices.push_back(std::move(vertex));
    }

    const std::size_t sourceTriangleCount = model.indices.size() / 3;
    std::vector<uint32_t> triangleMaterialSlots(sourceTriangleCount, 0);
    for (const SubMeshData& subMesh : model.subMeshes) {
        if (subMesh.indexStart > model.indices.size() ||
            subMesh.indexCount > model.indices.size() - subMesh.indexStart) {
            if (errorMessage != nullptr) {
                *errorMessage = "OBJ submesh range is outside the shared index buffer.";
            }
            return false;
        }
        const std::size_t firstTriangle = subMesh.indexStart / 3;
        const std::size_t endTriangle =
            (subMesh.indexStart + subMesh.indexCount) / 3;
        for (std::size_t triangle = firstTriangle;
             triangle < endTriangle && triangle < triangleMaterialSlots.size();
             ++triangle) {
            triangleMaterialSlots[triangle] = subMesh.materialIndex;
        }
        materialSlotCount = (std::max)(
            materialSlotCount,
            subMesh.materialIndex + 1);
    }
    materialSlotCount = (std::max)(
        materialSlotCount,
        static_cast<uint32_t>((std::max)(std::size_t{1}, model.materials.size())));

    geometry.triangles.reserve(sourceTriangleCount);
    uint32_t droppedDegenerateTriangles = 0;
    for (std::size_t triangleIndex = 0;
         triangleIndex < sourceTriangleCount;
         ++triangleIndex) {
        const uint32_t a = model.indices[triangleIndex * 3 + 0];
        const uint32_t b = model.indices[triangleIndex * 3 + 1];
        const uint32_t c = model.indices[triangleIndex * 3 + 2];
        if (a >= geometry.vertices.size() ||
            b >= geometry.vertices.size() ||
            c >= geometry.vertices.size()) {
            if (errorMessage != nullptr) {
                *errorMessage = "OBJ triangle references an invalid vertex.";
            }
            return false;
        }
        const Vector3 edgeA = Subtract(
            geometry.vertices[b].position,
            geometry.vertices[a].position);
        const Vector3 edgeB = Subtract(
            geometry.vertices[c].position,
            geometry.vertices[a].position);
        if (a == b || b == c || c == a ||
            LengthSquared(Cross(edgeA, edgeB)) <= 1.0e-12f) {
            ++droppedDegenerateTriangles;
            continue;
        }
        EditorGeometryTriangle triangle{};
        triangle.guid = StableElementGuid(sourceIdentity, 't', triangleIndex);
        triangle.vertices[0] = a;
        triangle.vertices[1] = b;
        triangle.vertices[2] = c;
        triangle.materialSlot = triangleMaterialSlots[triangleIndex];
        geometry.triangles.push_back(std::move(triangle));
    }
    if (droppedDegenerateTriangles != 0) {
        diagnostics.push_back(
            "Dropped " + std::to_string(droppedDegenerateTriangles) +
            " degenerate OBJ triangles during import.");
    }
    geometry.CompactUnusedVertices();
    const EditorGeometryValidationReport validation = geometry.Validate();
    if (!validation.Succeeded()) {
        if (errorMessage != nullptr) {
            *errorMessage = validation.errors.front();
        }
        return false;
    }
    diagnostics.insert(
        diagnostics.end(),
        validation.warnings.begin(),
        validation.warnings.end());
    return true;
}

bool ReadAllBytes(
    const std::filesystem::path& path,
    std::vector<uint8_t>& output) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 ||
        static_cast<uint64_t>(size) >
            EditorCookedMeshArtifact::kMaxArtifactBytes) {
        return false;
    }
    input.seekg(0, std::ios::beg);
    output.resize(static_cast<std::size_t>(size));
    if (!output.empty()) {
        input.read(
            reinterpret_cast<char*>(output.data()),
            static_cast<std::streamsize>(output.size()));
    }
    return input.good() || input.eof();
}

EditorAtomicFileWriter::Validator ProductionSourceValidator() {
    return [](const std::filesystem::path& path, std::string* errorMessage) {
        std::vector<uint8_t> bytes;
        if (!ReadAllBytes(path, bytes)) return false;
        EditorProductionMeshAssetDocument document{};
        return EditorProductionMeshAssetDocument::Deserialize(
            std::string_view(
                reinterpret_cast<const char*>(bytes.data()),
                bytes.size()),
            document,
            errorMessage);
    };
}

EditorAtomicFileWriter::Validator CookedMeshValidator() {
    return [](const std::filesystem::path& path, std::string* errorMessage) {
        std::vector<uint8_t> bytes;
        if (!ReadAllBytes(path, bytes)) return false;
        EditorCookedMeshArtifact artifact{};
        return EditorCookedMeshArtifact::Deserialize(
            bytes,
            artifact,
            errorMessage);
    };
}

EditorAtomicFileWriter::Validator CookedCollisionValidator() {
    return [](const std::filesystem::path& path, std::string* errorMessage) {
        std::vector<uint8_t> bytes;
        if (!ReadAllBytes(path, bytes)) return false;
        EditorCookedCollisionArtifact artifact{};
        return EditorCookedCollisionArtifact::Deserialize(
            bytes,
            artifact,
            errorMessage);
    };
}

EditorAtomicFileWriter::Validator MetadataValidator(
    std::string guid,
    std::string sourcePath) {
    return [
        guid = std::move(guid),
        sourcePath = std::move(sourcePath)](
            const std::filesystem::path& path,
            std::string* errorMessage) {
        std::vector<uint8_t> bytes;
        if (!ReadAllBytes(path, bytes)) return false;
        const std::string text(bytes.begin(), bytes.end());
        const bool valid =
            text.find("guid=" + guid + "\n") != std::string::npos &&
            text.find("importSourcePath=" + sourcePath + "\n") !=
                std::string::npos;
        if (!valid && errorMessage != nullptr) {
            *errorMessage = "OBJ Production Mesh metadata is incomplete.";
        }
        return valid;
    };
}

bool MetadataMatchesSource(
    const std::filesystem::path& metadataPath,
    std::string_view sourcePath) {
    std::ifstream input(metadataPath);
    if (!input) return false;
    const std::string expected = "importSourcePath=" + std::string(sourcePath);
    std::string line;
    while (std::getline(input, line)) {
        if (line == expected) return true;
    }
    return false;
}

} // namespace

EditorObjProductionImportBridge::EditorObjProductionImportBridge(
    EditorAssetRegistry& registry,
    EditorProductionMeshRuntimeCache* runtimeCache,
    std::filesystem::path projectRoot)
    : registry_(registry)
    , runtimeCache_(runtimeCache)
    , projectRoot_(std::move(projectRoot)) {
}

bool EditorObjProductionImportBridge::CanImport(
    const EditorAssetRecord& source) {
    const std::string extension = Lowercase(
        std::filesystem::path(source.sourcePath).extension().string());
    return source.kind == EditorAssetKind::Mesh &&
        source.referenceable &&
        !source.missing &&
        (extension == ".obj" || extension == ".gltf" ||
         extension == ".glb" || extension == ".fbx");
}

std::string EditorObjProductionImportBridge::DefaultOutputAssetName(
    const EditorAssetRecord& source) {
    const std::string sourceName = !source.id.empty()
        ? source.id
        : std::filesystem::path(source.sourcePath).stem().string();
    const std::string sanitized = SanitizeAssetName(sourceName);
    return (sanitized.empty() ? std::string{"imported_obj"} : sanitized) +
        "_production";
}

bool EditorObjProductionImportBridge::IsProductionForSource(
    const EditorAssetRecord& production,
    const EditorAssetRecord& source,
    const std::filesystem::path& projectRoot) {
    if (production.kind != EditorAssetKind::Mesh || production.missing ||
        !production.referenceable || production.guid.empty() ||
        Lowercase(std::filesystem::path(production.sourcePath).extension().string()) !=
            ".mesh") {
        return false;
    }
    const std::string sourceLogicalPath = source.logicalPath.empty()
        ? source.sourcePath
        : source.logicalPath;
    return MetadataMatchesSource(
        ResolveProjectPath(projectRoot, production.metadataPath),
        sourceLogicalPath);
}

bool EditorObjProductionImportBridge::LoadSourceGeometry(
    const EditorAssetRecord& source,
    const std::filesystem::path& projectRoot,
    EditorGeometryMesh& geometry,
    uint64_t* sourceFileHash,
    uint32_t* materialSlotCount,
    std::vector<std::string>* diagnostics,
    std::string* errorMessage) {
    geometry = {};
    if (sourceFileHash != nullptr) *sourceFileHash = 0;
    if (materialSlotCount != nullptr) *materialSlotCount = 0;
    if (!CanImport(source)) {
        if (errorMessage != nullptr) {
            *errorMessage =
                "Mesh source must be an available OBJ, glTF, GLB, or FBX Asset.";
        }
        return false;
    }
    const std::filesystem::path sourcePath =
        ResolveProjectPath(projectRoot, source.sourcePath);
    std::string hashError;
    const uint64_t fileHash = HashFile(sourcePath, &hashError);
    if (fileHash == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = hashError.empty()
                ? "Mesh source hash could not be computed." : std::move(hashError);
        }
        return false;
    }
    ModelData model = LoadObjFile_Assimp(
        sourcePath.parent_path().string(), sourcePath.filename().string());
    uint32_t resolvedMaterialSlotCount = 0;
    std::vector<std::string> localDiagnostics;
    std::string conversionError;
    if (!ConvertObjModel(source, model, geometry, resolvedMaterialSlotCount,
            localDiagnostics, &conversionError)) {
        if (errorMessage != nullptr) {
            *errorMessage = conversionError.empty()
                ? "Mesh source could not be converted to Production Geometry."
                : std::move(conversionError);
        }
        return false;
    }
    if (sourceFileHash != nullptr) *sourceFileHash = fileHash;
    if (materialSlotCount != nullptr) {
        *materialSlotCount = resolvedMaterialSlotCount;
    }
    if (diagnostics != nullptr) {
        diagnostics->insert(diagnostics->end(), localDiagnostics.begin(),
            localDiagnostics.end());
    }
    return true;
}

EditorObjProductionImportResult
EditorObjProductionImportBridge::ImportAndBake(
    const EditorObjProductionImportRequest& request) {
    EditorObjProductionImportResult result{};
    if (projectRoot_.empty()) {
        SetError(result, "Mesh Production Import has no project root.");
        return result;
    }
    const EditorAssetRecord* source =
        registry_.FindByGuid(request.sourceAssetGuid);
    if (source == nullptr) {
        SetError(result, "Selected source Mesh Asset no longer exists in the Asset Registry.");
        return result;
    }
    const EditorAssetRecord sourceSnapshot = *source;
    if (!CanImport(sourceSnapshot)) {
        SetError(
            result,
            "Mesh Production Import requires an available OBJ, glTF, GLB, or FBX source.");
        return result;
    }
    std::string settingsError;
    if (!request.settings.Validate(&settingsError)) {
        SetError(
            result,
            settingsError.empty()
                ? "Mesh Production Import settings are invalid."
                : settingsError);
        return result;
    }

    const std::filesystem::path sourcePath =
        ResolveProjectPath(projectRoot_, sourceSnapshot.sourcePath);
    EditorGeometryMesh geometry{};
    uint64_t sourceFileHash = 0;
    uint32_t materialSlotCount = 0;
    std::string conversionError;
    if (!LoadSourceGeometry(sourceSnapshot, projectRoot_, geometry,
            &sourceFileHash, &materialSlotCount, &result.diagnostics,
            &conversionError)) {
        SetError(
            result,
            conversionError.empty()
                ? "Source Mesh could not be converted to editable Geometry."
                : conversionError);
        return result;
    }

    const std::string outputId = SanitizeAssetName(
        request.outputAssetName.empty()
            ? DefaultOutputAssetName(sourceSnapshot)
            : request.outputAssetName);
    if (outputId.empty()) {
        SetError(result, "Production Mesh output Asset name is empty.");
        return result;
    }
    const std::filesystem::path relativeProductionPath =
        std::filesystem::path("Resources") /
        "Generated" /
        "Imported" /
        (outputId + ".mesh");
    std::string sourceFormatTag =
        Lowercase(sourcePath.extension().string());
    if (!sourceFormatTag.empty() && sourceFormatTag.front() == '.') {
        sourceFormatTag.erase(sourceFormatTag.begin());
    }
    if (sourceFormatTag.empty()) {
        sourceFormatTag = "mesh-source";
    }

    EditorAssetRecord outputRecord{};
    const EditorAssetRecord* existing =
        registry_.Find(EditorAssetKind::Mesh, outputId);
    if (existing != nullptr) {
        if (Lowercase(
                std::filesystem::path(existing->sourcePath)
                    .extension()
                    .string()) != ".mesh" ||
            !MetadataMatchesSource(
                ResolveProjectPath(projectRoot_, existing->metadataPath),
                sourceSnapshot.logicalPath.empty()
                    ? sourceSnapshot.sourcePath
                    : sourceSnapshot.logicalPath)) {
            SetError(
                result,
                "Production Mesh Asset ID '" + outputId +
                    "' is owned by another source. Choose a different output name.");
            return result;
        }
        outputRecord = *existing;
        result.reimported = true;
    } else {
        outputRecord.kind = EditorAssetKind::Mesh;
        outputRecord.id = outputId;
        outputRecord.guid = GenerateEditorAssetGuid();
        outputRecord.displayName = outputId;
        outputRecord.sourcePath = relativeProductionPath.generic_string();
        outputRecord.logicalPath = outputRecord.sourcePath;
        outputRecord.metadataPath = outputRecord.sourcePath + ".meta";
        outputRecord.thumbnailKey = "mesh:" + outputRecord.guid;
        outputRecord.referenceable = true;
        outputRecord.hasMetadata = true;
        outputRecord.provisionalGuid = false;
        outputRecord.tags = {"generated", "imported", sourceFormatTag};
    }
    outputRecord.missing = false;
    outputRecord.sourceTimestamp = CurrentTimestamp();

    EditorProductionMeshAssetDocument document{};
    document.assetGuid = outputRecord.guid;
    document.assetId = outputRecord.id;
    document.sourceGeometryHash = geometry.ContentHash();
    document.settings = request.settings;
    document.geometry = std::move(geometry);
    EditorCookedMeshArtifact cooked{};
    EditorCookedCollisionArtifact collision{};
    std::string buildError;
    if (!BuildEditorCookedMeshArtifacts(
            document.geometry,
            nullptr,
            request.settings,
            cooked,
            collision,
            &buildError)) {
        SetError(
            result,
            buildError.empty()
                ? "Production Mesh cook failed."
                : buildError);
        return result;
    }

    std::string sourceText;
    std::vector<uint8_t> cookedBytes;
    std::vector<uint8_t> collisionBytes;
    if (!document.Serialize(sourceText, &buildError) ||
        !cooked.Serialize(cookedBytes, &buildError) ||
        (request.settings.collisionMode !=
                EditorMeshCollisionBuildMode::None &&
            !collision.Serialize(collisionBytes, &buildError))) {
        SetError(
            result,
            buildError.empty()
                ? "Production Mesh artifacts could not be serialized."
                : buildError);
        return result;
    }

    const std::string sourceLogicalPath =
        sourceSnapshot.logicalPath.empty()
        ? sourceSnapshot.sourcePath
        : sourceSnapshot.logicalPath;
    const std::filesystem::path cookedPath =
        EditorCookedMeshPath(outputRecord.sourcePath);
    const std::filesystem::path collisionPath =
        EditorCookedCollisionPath(outputRecord.sourcePath);
    std::ostringstream metadata;
    metadata << "guid=" << outputRecord.guid << '\n';
    metadata << "logicalPath=" << outputRecord.logicalPath << '\n';
    metadata << "tags=generated,imported," << sourceFormatTag << '\n';
    metadata << "importSourceGuid=" << sourceSnapshot.guid << '\n';
    metadata << "importSourcePath=" << sourceLogicalPath << '\n';
    metadata << "importSourceHash=" << sourceFileHash << '\n';
    metadata << "sourceGeometryHash=" << document.sourceGeometryHash << '\n';
    metadata << "buildSettingsHash=" << request.settings.ContentHash() << '\n';
    metadata << "cookedPath=" << cookedPath.generic_string() << '\n';
    metadata << "collisionPath=" << collisionPath.generic_string() << '\n';

    EditorFileTransaction transaction(projectRoot_);
    std::string transactionError;
    if (!transaction.StageTextWrite(
            outputRecord.sourcePath,
            sourceText,
            ProductionSourceValidator(),
            &transactionError) ||
        !transaction.StageWrite(
            cookedPath,
            cookedBytes,
            CookedMeshValidator(),
            &transactionError) ||
        (request.settings.collisionMode !=
                EditorMeshCollisionBuildMode::None &&
            !transaction.StageWrite(
                collisionPath,
                collisionBytes,
                CookedCollisionValidator(),
                &transactionError)) ||
        !transaction.StageTextWrite(
            outputRecord.metadataPath,
            metadata.str(),
            MetadataValidator(outputRecord.guid, sourceLogicalPath),
            &transactionError)) {
        SetError(
            result,
            transactionError.empty()
                ? "Mesh Production Import file transaction could not be staged."
                : transactionError);
        return result;
    }
    EditorFileTransactionReceipt receipt{};
    if (!transaction.ApplyPrepared(&receipt, &transactionError)) {
        SetError(
            result,
            transactionError.empty()
                ? "Mesh Production Import file transaction could not be prepared."
                : transactionError);
        return result;
    }

    const EditorAssetRegistry registryBackup = registry_;
    if (!registry_.Register(outputRecord)) {
        registry_ = registryBackup;
        transaction.RollbackPrepared(&receipt, nullptr);
        SetError(result, "Production Mesh could not be registered.");
        return result;
    }
    std::string cacheError;
    if (runtimeCache_ != nullptr &&
        !runtimeCache_->Load(outputRecord, &cacheError)) {
        registry_ = registryBackup;
        transaction.RollbackPrepared(&receipt, nullptr);
        SetError(
            result,
            cacheError.empty()
                ? "Production Mesh Runtime Cache rejected the imported artifacts."
                : cacheError);
        return result;
    }
    if (!transaction.CommitPrepared(&receipt, &transactionError)) {
        registry_ = registryBackup;
        if (runtimeCache_ != nullptr) {
            runtimeCache_->Invalidate(outputRecord.guid);
        }
        transaction.RollbackPrepared(&receipt, nullptr);
        SetError(
            result,
            transactionError.empty()
                ? "OBJ Production Import file transaction could not be committed."
                : transactionError);
        return result;
    }

    registry_.ScanDependencies();
    result.succeeded = true;
    result.record = outputRecord;
    result.vertexCount =
        static_cast<uint32_t>(document.geometry.vertices.size());
    result.triangleCount =
        static_cast<uint32_t>(document.geometry.triangles.size());
    result.materialSlotCount = materialSlotCount;
    result.lodCount = static_cast<uint32_t>(cooked.lods.size());
    result.artifactBytes =
        sourceText.size() +
        cookedBytes.size() +
        collisionBytes.size() +
        metadata.str().size();
    result.message =
        std::string(result.reimported ? "Reimported " : "Imported ") +
        "Mesh source as Production Mesh '" + outputRecord.id + "' (" +
        std::to_string(result.vertexCount) + " vertices, " +
        std::to_string(result.triangleCount) + " triangles, " +
        std::to_string(result.lodCount) + " LODs).";
    return result;
}

} // namespace editor
