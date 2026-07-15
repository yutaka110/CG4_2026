#include "EditorRuntimeApplyUndoCommand.h"

#include "IEditorRuntimeApplyExecutionService.h"
#include "../core/EditorExecutionContext.h"

#include <utility>

namespace editor {
namespace {

std::size_t StringBytes(const std::string& value) noexcept {
    return value.capacity() + 1;
}

template <typename T>
std::size_t VectorBytes(const std::vector<T>& values) noexcept {
    return values.capacity() * sizeof(T);
}

std::size_t CourseBytes(const CourseAsset& course) noexcept {
    std::size_t bytes = sizeof(course) + StringBytes(course.name) +
        VectorBytes(course.railPoints) + VectorBytes(course.cameraKeys) +
        VectorBytes(course.sections) + VectorBytes(course.events) +
        VectorBytes(course.terrainPlacements) + VectorBytes(course.rockClusters) +
        VectorBytes(course.lightingPresets) + VectorBytes(course.cameraShotPresets) +
        VectorBytes(course.cameraBlendAssets) + VectorBytes(course.cinematicCameraShots) +
        VectorBytes(course.terrainMaterialPresets) + VectorBytes(course.cinematicShotSets);
    for (const CourseSection& value : course.sections) {
        bytes += StringBytes(value.name) + StringBytes(value.category);
    }
    for (const CourseEventMarker& value : course.events) {
        bytes += StringBytes(value.type) + StringBytes(value.id) + StringBytes(value.payload);
    }
    for (const CourseTerrainPlacement& value : course.terrainPlacements) {
        bytes += StringBytes(value.id) + StringBytes(value.meshId);
    }
    for (const CourseRockCluster& value : course.rockClusters) {
        bytes += StringBytes(value.id) + StringBytes(value.meshId) +
            VectorBytes(value.instanceOverrides);
    }
    for (const CourseLightingPreset& value : course.lightingPresets) {
        bytes += StringBytes(value.id);
    }
    for (const CourseCameraShotPreset& value : course.cameraShotPresets) {
        bytes += StringBytes(value.id) + StringBytes(value.mode);
    }
    for (const CourseCameraBlendAsset& value : course.cameraBlendAssets) {
        bytes += StringBytes(value.id) + StringBytes(value.curve);
    }
    for (const CourseCinematicCameraShot& value : course.cinematicCameraShots) {
        bytes += StringBytes(value.id) + StringBytes(value.mode) + StringBytes(value.presetId) +
            StringBytes(value.blendAssetId);
    }
    for (const CourseTerrainMaterialPreset& value : course.terrainMaterialPresets) {
        bytes += StringBytes(value.id);
    }
    for (const CourseCinematicShotSet& value : course.cinematicShotSets) {
        bytes += StringBytes(value.id) + StringBytes(value.label) +
            StringBytes(value.lightingPresetId) + StringBytes(value.cameraShotId) +
            StringBytes(value.terrainMaterialId) + StringBytes(value.fogMood) +
            StringBytes(value.compositionNotes) + VectorBytes(value.heroLandmarkIds) +
            VectorBytes(value.vistaLandmarkIds);
        for (const std::string& id : value.heroLandmarkIds) bytes += StringBytes(id);
        for (const std::string& id : value.vistaLandmarkIds) bytes += StringBytes(id);
    }
    return bytes;
}

std::size_t VfxBytes(const std::unordered_map<std::string, EffectAsset>& assets) noexcept {
    std::size_t bytes = assets.size() * (sizeof(std::string) + sizeof(EffectAsset));
    for (const auto& [id, asset] : assets) {
        bytes += StringBytes(id) + StringBytes(asset.name) + StringBytes(asset.shader) +
            StringBytes(asset.texture) + StringBytes(asset.techniqueDisplayName) +
            StringBytes(asset.techniqueCategory) + StringBytes(asset.techniqueDescription) +
            asset.Components().ComponentCount() * (sizeof(EffectComponentAsset) * 2);
    }
    return bytes;
}

std::size_t PostProcessBytes(const std::vector<PostProcessPass>& passes) noexcept {
    std::size_t bytes = VectorBytes(passes);
    for (const PostProcessPass& pass : passes) {
        bytes += StringBytes(pass.name) + StringBytes(pass.inputResource) +
            StringBytes(pass.outputResource) + StringBytes(pass.pipeline) +
            StringBytes(pass.secondaryInputResource) + StringBytes(pass.tertiaryInputResource);
    }
    return bytes;
}

} // namespace

EditorRuntimeApplyUndoCommand::EditorRuntimeApplyUndoCommand(EditorRuntimeApplyChange change)
    : change_(std::move(change)) {}

EditorUndoResult EditorRuntimeApplyUndoCommand::Apply(
    EditorTransactionApplyMode mode,
    EditorExecutionContext& context) const {
    IEditorExecutionService* untyped = context.Find(IEditorRuntimeApplyExecutionService::kServiceId);
    auto* service = dynamic_cast<IEditorRuntimeApplyExecutionService*>(untyped);
    if (service == nullptr) {
        return EditorUndoResult::Failure(
            EditorErrorCode::MissingService,
            "Runtime Apply execution service is not registered.");
    }
    return service->ApplyRuntimeChange(change_, mode);
}

std::size_t EditorRuntimeApplyUndoCommand::EstimatedBytes() const noexcept {
    return sizeof(*this) + CourseBytes(change_.beforeCourse) + CourseBytes(change_.afterCourse) +
        VfxBytes(change_.beforeVfxAuthoring) + VfxBytes(change_.afterVfxAuthoring) +
        PostProcessBytes(change_.beforePostProcess) + PostProcessBytes(change_.afterPostProcess);
}

} // namespace editor
