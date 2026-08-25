#pragma once

#include "EditorScene.h"
#include "../../level/BlenderLevelJsonLoader.h"
#include "../../course/RailWorldScale.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

class EditorAssetRegistry;

inline constexpr std::string_view kEditorBlenderSceneSourceComponentType =
    "editor.blender-scene-source";
inline constexpr std::string_view kEditorBlenderObjectSourceComponentType =
    "editor.blender-object-source";
inline constexpr std::string_view kEditorGameplaySpawnPointComponentType =
    "gameplay.spawn-point";
inline constexpr std::string_view kEditorBoxColliderComponentType =
    "engine.box-collider";

enum class EditorBlenderSceneImportMode {
    Import,
    Reimport,
    ImportOrReimport,
};

enum class EditorBlenderSceneImportErrorCode {
    None,
    LoadFailed,
    InvalidArgument,
    PlayabilityViolation,
    SourceAlreadyImported,
    SourceNotImported,
    IdentityConflict,
    SceneValidationFailed,
};

struct EditorBlenderSceneImportOptions {
    std::string destinationParentGuid;
    double worldUnitsPerMeter =
        static_cast<double>(RailWorldScale::kWorldUnitsPerMeter);
    bool requireExactlyOnePlayerSpawn = true;
    bool removeMissingSourceObjects = true;
    bool updateEntityNames = true;
    bool resolveMeshReferences = true;
};

struct EditorBlenderSceneImportResult {
    bool succeeded = false;
    bool reimported = false;
    EditorBlenderSceneImportErrorCode errorCode =
        EditorBlenderSceneImportErrorCode::None;
    std::string message;
    std::filesystem::path sourcePath;
    std::string sceneGuid;
    std::string rootEntityGuid;
    std::size_t createdObjectCount = 0;
    std::size_t updatedObjectCount = 0;
    std::size_t removedObjectCount = 0;
    std::size_t preservedUserChildCount = 0;
    std::size_t unresolvedMeshCount = 0;
    std::vector<std::string> warnings;
    std::optional<ge3::level::BlenderLevelLoadError> loadError;
};

class EditorBlenderSceneImportService {
public:
    explicit EditorBlenderSceneImportService(
        const EditorAssetRegistry* assets = nullptr,
        ge3::level::BlenderLevelJsonLoader loader =
            ge3::level::BlenderLevelJsonLoader{
                ge3::level::BlenderLevelJsonLimits{}});

    EditorBlenderSceneImportResult ImportFile(
        const std::filesystem::path& sourcePath,
        EditorScene& scene,
        const EditorBlenderSceneImportOptions& options = {}) const;
    EditorBlenderSceneImportResult ReimportFile(
        const std::filesystem::path& sourcePath,
        EditorScene& scene,
        const EditorBlenderSceneImportOptions& options = {}) const;
    EditorBlenderSceneImportResult ImportOrReimportFile(
        const std::filesystem::path& sourcePath,
        EditorScene& scene,
        const EditorBlenderSceneImportOptions& options = {}) const;

    EditorBlenderSceneImportResult Import(
        const ge3::level::BlenderLevelData& level,
        const std::filesystem::path& sourcePath,
        EditorScene& scene,
        const EditorBlenderSceneImportOptions& options = {}) const;
    EditorBlenderSceneImportResult Reimport(
        const ge3::level::BlenderLevelData& level,
        const std::filesystem::path& sourcePath,
        EditorScene& scene,
        const EditorBlenderSceneImportOptions& options = {}) const;
    EditorBlenderSceneImportResult ImportOrReimport(
        const ge3::level::BlenderLevelData& level,
        const std::filesystem::path& sourcePath,
        EditorScene& scene,
        const EditorBlenderSceneImportOptions& options = {}) const;

private:
    EditorBlenderSceneImportResult LoadAndApply(
        EditorBlenderSceneImportMode mode,
        const std::filesystem::path& sourcePath,
        EditorScene& scene,
        const EditorBlenderSceneImportOptions& options) const;
    EditorBlenderSceneImportResult Apply(
        EditorBlenderSceneImportMode mode,
        const ge3::level::BlenderLevelData& level,
        const std::filesystem::path& sourcePath,
        EditorScene& scene,
        const EditorBlenderSceneImportOptions& options) const;

    const EditorAssetRegistry* assets_ = nullptr;
    ge3::level::BlenderLevelJsonLoader loader_;
};

const char* ToString(EditorBlenderSceneImportErrorCode value) noexcept;

} // namespace editor
