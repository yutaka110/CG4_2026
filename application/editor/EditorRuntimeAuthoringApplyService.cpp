#include "EditorRuntimeAuthoringApplyService.h"

#include "EditorNotificationCenter.h"

#include <cstdint>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace editor {
namespace {

uint64_t HashBytes(uint64_t hash, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint64_t>(bytes[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

uint64_t HashString(uint64_t hash, const std::string& value) {
    return HashBytes(hash, value.data(), value.size());
}

template <typename T>
uint64_t HashValue(uint64_t hash, const T& value) {
    return HashBytes(hash, &value, sizeof(T));
}

uint64_t CourseSignature(const CourseAsset& course) {
    uint64_t hash = 1469598103934665603ull;
    hash = HashString(hash, course.name);
    hash = HashValue(hash, course.railPoints.size());
    for (const RailPathControlPoint& point : course.railPoints) {
        hash = HashValue(hash, point.position.x);
        hash = HashValue(hash, point.position.y);
        hash = HashValue(hash, point.position.z);
        hash = HashValue(hash, point.corridorRadius);
        hash = HashValue(hash, point.speed);
    }
    hash = HashValue(hash, course.terrainPlacements.size());
    for (const CourseTerrainPlacement& placement : course.terrainPlacements) {
        hash = HashString(hash, placement.id);
        hash = HashString(hash, placement.meshId);
        hash = HashValue(hash, placement.distance);
        hash = HashValue(hash, placement.lateralOffset);
        hash = HashValue(hash, placement.verticalOffset);
        hash = HashValue(hash, placement.forwardOffset);
        hash = HashValue(hash, placement.scale.x);
        hash = HashValue(hash, placement.scale.y);
        hash = HashValue(hash, placement.scale.z);
    }
    hash = HashValue(hash, course.rockClusters.size());
    for (const CourseRockCluster& cluster : course.rockClusters) {
        hash = HashString(hash, cluster.id);
        hash = HashString(hash, cluster.meshId);
        hash = HashValue(hash, cluster.distance);
        hash = HashValue(hash, cluster.count);
        hash = HashValue(hash, cluster.minScale);
        hash = HashValue(hash, cluster.maxScale);
        hash = HashValue(hash, cluster.spread.x);
        hash = HashValue(hash, cluster.spread.y);
        hash = HashValue(hash, cluster.spread.z);
    }
    hash = HashValue(hash, course.cameraKeys.size());
    for (const CourseCameraKey& key : course.cameraKeys) {
        hash = HashValue(hash, key.distance);
        hash = HashValue(hash, key.backDistance);
        hash = HashValue(hash, key.verticalOffset);
        hash = HashValue(hash, key.fovY);
        hash = HashValue(hash, key.roll);
    }
    hash = HashValue(hash, course.sections.size());
    for (const CourseSection& section : course.sections) {
        hash = HashValue(hash, section.startDistance);
        hash = HashValue(hash, section.endDistance);
        hash = HashString(hash, section.name);
        hash = HashString(hash, section.category);
    }
    hash = HashValue(hash, course.events.size());
    for (const CourseEventMarker& event : course.events) {
        hash = HashValue(hash, event.distance);
        hash = HashString(hash, event.type);
        hash = HashString(hash, event.id);
        hash = HashString(hash, event.payload);
    }
    hash = HashValue(hash, course.lightingPresets.size());
    for (const CourseLightingPreset& preset : course.lightingPresets) {
        hash = HashValue(hash, preset.distance);
        hash = HashString(hash, preset.id);
        hash = HashValue(hash, preset.sunIntensity);
        hash = HashValue(hash, preset.fogIntensity);
        hash = HashValue(hash, preset.fogDensity);
    }
    hash = HashValue(hash, course.cameraShotPresets.size());
    for (const CourseCameraShotPreset& preset : course.cameraShotPresets) {
        hash = HashString(hash, preset.id);
        hash = HashString(hash, preset.mode);
        hash = HashValue(hash, preset.fovOffset);
        hash = HashValue(hash, preset.shakeAmount);
    }
    hash = HashValue(hash, course.cameraBlendAssets.size());
    for (const CourseCameraBlendAsset& blend : course.cameraBlendAssets) {
        hash = HashString(hash, blend.id);
        hash = HashString(hash, blend.curve);
        hash = HashValue(hash, blend.blendInDistance);
        hash = HashValue(hash, blend.blendOutDistance);
    }
    hash = HashValue(hash, course.cinematicCameraShots.size());
    for (const CourseCinematicCameraShot& shot : course.cinematicCameraShots) {
        hash = HashValue(hash, shot.startDistance);
        hash = HashValue(hash, shot.endDistance);
        hash = HashString(hash, shot.id);
        hash = HashString(hash, shot.mode);
        hash = HashString(hash, shot.presetId);
        hash = HashString(hash, shot.blendAssetId);
    }
    hash = HashValue(hash, course.terrainMaterialPresets.size());
    for (const CourseTerrainMaterialPreset& material : course.terrainMaterialPresets) {
        hash = HashValue(hash, material.distance);
        hash = HashString(hash, material.id);
        hash = HashValue(hash, material.brightness);
        hash = HashValue(hash, material.noiseStrength);
        hash = HashValue(hash, material.specularStrength);
    }
    hash = HashValue(hash, course.cinematicShotSets.size());
    for (const CourseCinematicShotSet& shotSet : course.cinematicShotSets) {
        hash = HashValue(hash, shotSet.startDistance);
        hash = HashValue(hash, shotSet.endDistance);
        hash = HashString(hash, shotSet.id);
        hash = HashString(hash, shotSet.label);
        hash = HashString(hash, shotSet.lightingPresetId);
        hash = HashString(hash, shotSet.cameraShotId);
        hash = HashString(hash, shotSet.terrainMaterialId);
        hash = HashString(hash, shotSet.fogMood);
        hash = HashString(hash, shotSet.compositionNotes);
    }
    return hash;
}

uint64_t TerrainSignature(const TerrainAuthoringState& terrain) {
    uint64_t hash = 1469598103934665603ull;
    hash = HashValue(hash, terrain.courseObjectEditRevision);
    hash = HashValue(hash, terrain.previewSpeed);
    hash = HashValue(hash, terrain.previewDistance);
    hash = HashValue(hash, terrain.selectedCourseTerrainPlacement);
    hash = HashValue(hash, terrain.selectedCourseRockCluster);
    hash = HashValue(hash, terrain.settings.seed);
    hash = HashValue(hash, terrain.settings.chunkLength);
    hash = HashValue(hash, terrain.settings.visibleAheadChunks);
    hash = HashValue(hash, terrain.settings.volumeRoughness);
    hash = HashValue(hash, terrain.settings.wallHeight);
    return hash;
}

std::string BuildSummary(const CourseAsset& course, const TerrainAuthoringState& terrain) {
    std::ostringstream stream;
    stream << "courseHash=0x" << std::hex << CourseSignature(course)
           << " terrainHash=0x" << TerrainSignature(terrain)
           << std::dec
           << " events=" << course.events.size()
           << " cameras=" << course.cameraKeys.size()
           << " terrain=" << course.terrainPlacements.size()
           << " rocks=" << course.rockClusters.size()
           << " revision=" << terrain.courseObjectEditRevision
           << " previewSpeed=" << std::fixed << std::setprecision(3) << terrain.previewSpeed;
    return stream.str();
}

EditorObjectHandle RuntimeApplyTarget(uint64_t sessionSerial) {
    EditorObjectHandle target{};
    target.domain = EditorDomainId::Unknown;
    target.stableId = "runtime-authoring-apply:" + std::to_string(sessionSerial);
    target.displayName = "Runtime Authoring Apply";
    target.generation = static_cast<uint32_t>(sessionSerial);
    return target;
}

EditorRuntimeAuthoringApplyResult Fail(
    const EditorRuntimeAuthoringApplyRequest& request,
    std::string message) {
    if (request.notifications != nullptr) {
        request.notifications->Push(
            EditorNotificationSeverity::Error,
            request.source,
            message);
    }
    EditorRuntimeAuthoringApplyResult result{};
    result.message = std::move(message);
    return result;
}

void NotifySuccess(
    const EditorRuntimeAuthoringApplyRequest& request,
    const std::string& message) {
    if (request.notifications != nullptr) {
        request.notifications->Push(
            EditorNotificationSeverity::Info,
            request.source,
            message);
    }
}

bool ValidateRequest(
    const EditorRuntimeAuthoringApplyRequest& request,
    std::string& outMessage) {
    if (request.playSession == nullptr) {
        outMessage = "Play session state is unavailable.";
        return false;
    }
    if (!request.playSession->IsActive()) {
        outMessage = "Runtime changes can only be applied during Play/Sim.";
        return false;
    }
    if (request.snapshot == nullptr || !request.snapshot->Captured()) {
        outMessage = "Play/Sim snapshot has not been captured.";
        return false;
    }
    if (request.snapshot->SessionSerial() != 0 &&
        request.snapshot->SessionSerial() != request.playSession->SessionSerial()) {
        outMessage = "Play/Sim snapshot belongs to a different session.";
        return false;
    }
    if (request.course == nullptr) {
        outMessage = "Course asset is unavailable for runtime apply.";
        return false;
    }
    if (request.runtimeState == nullptr) {
        outMessage = "Runtime state is unavailable for runtime apply.";
        return false;
    }
    if (request.transactions == nullptr) {
        outMessage = "Transaction stack is unavailable for runtime apply.";
        return false;
    }
    if (request.validationErrorCount > 0) {
        outMessage = "Runtime apply blocked: validation errors.";
        return false;
    }
    return true;
}

void MarkRuntimeApplyDirty(const EditorRuntimeAuthoringApplyRequest& request) {
    if (request.dirtyState == nullptr || request.runtimeState == nullptr) {
        return;
    }
    request.dirtyState->MarkDirty(
        EditorDirtyDomain::CourseAuthoring,
        "runtime-authoring-apply",
        "Runtime Apply",
        "Runtime changes explicitly applied to authoring.",
        request.runtimeState->terrain.courseObjectEditRevision);
}

} // namespace

EditorRuntimeAuthoringApplyResult EditorRuntimeAuthoringApplyService::Apply(
    const EditorRuntimeAuthoringApplyRequest& request) const {
    std::string validationMessage;
    if (!ValidateRequest(request, validationMessage)) {
        return Fail(request, validationMessage);
    }

    const CourseAsset* beforeCourse = request.snapshot->CapturedCourse();
    const TerrainAuthoringState* beforeTerrain = request.snapshot->CapturedTerrain();
    if (beforeCourse == nullptr || beforeTerrain == nullptr) {
        return Fail(request, "Play/Sim snapshot is missing authoring state.");
    }

    const std::string beforeSummary = BuildSummary(*beforeCourse, *beforeTerrain);
    std::string afterSummary = BuildSummary(*request.course, request.runtimeState->terrain);
    if (beforeSummary == afterSummary) {
        return Fail(request, "No runtime authoring changes to apply.");
    }

    ++request.runtimeState->terrain.courseObjectEditRevision;
    afterSummary = BuildSummary(*request.course, request.runtimeState->terrain);

    EditorRuntimeAuthoringApplyChange change{};
    change.sessionSerial = request.playSession->SessionSerial();
    change.beforeCourse = *beforeCourse;
    change.afterCourse = *request.course;
    change.beforeTerrain = *beforeTerrain;
    change.afterTerrain = request.runtimeState->terrain;

    request.transactions->PushRuntimeAuthoringApply(
        "Apply Runtime Changes To Authoring",
        RuntimeApplyTarget(change.sessionSerial),
        change);

    std::string adoptError;
    if (!request.snapshot->Adopt(
            EditorPlaySessionIsolationSnapshotTarget{request.course, request.runtimeState},
            &adoptError)) {
        return Fail(
            request,
            adoptError.empty()
                ? std::string("Failed to adopt runtime state into Play/Sim snapshot.")
                : adoptError);
    }
    request.snapshot->BindSession(request.playSession->SessionSerial());
    MarkRuntimeApplyDirty(request);

    EditorRuntimeAuthoringApplyResult result{};
    result.succeeded = true;
    result.changed = true;
    result.sessionSerial = request.playSession->SessionSerial();
    result.beforeSummary = beforeSummary;
    result.afterSummary = afterSummary;
    result.message = "Applied runtime changes to authoring.";
    NotifySuccess(request, result.message);
    return result;
}

EditorRuntimeAuthoringApplyResult EditorRuntimeAuthoringApplyService::ApplyTransaction(
    const EditorRuntimeAuthoringApplyRequest& request,
    const EditorTransactionRecord& record,
    EditorTransactionApplyMode mode) const {
    if (record.payload.kind != EditorTransactionPayloadKind::RuntimeAuthoringApply) {
        return Fail(request, "Transaction is not a runtime authoring apply payload.");
    }
    if (request.course == nullptr || request.runtimeState == nullptr) {
        return Fail(request, "Runtime apply transaction target is unavailable.");
    }

    const EditorRuntimeAuthoringApplyChange& change = record.payload.runtimeAuthoringApply;
    if (mode == EditorTransactionApplyMode::Undo) {
        *request.course = change.beforeCourse;
        request.runtimeState->terrain = change.beforeTerrain;
    } else {
        *request.course = change.afterCourse;
        request.runtimeState->terrain = change.afterTerrain;
    }
    ++request.runtimeState->terrain.courseObjectEditRevision;
    MarkRuntimeApplyDirty(request);

    EditorRuntimeAuthoringApplyResult result{};
    result.succeeded = true;
    result.changed = true;
    result.sessionSerial = change.sessionSerial;
    result.beforeSummary = BuildSummary(change.beforeCourse, change.beforeTerrain);
    result.afterSummary = BuildSummary(change.afterCourse, change.afterTerrain);
    result.message =
        mode == EditorTransactionApplyMode::Undo
            ? "Undid runtime authoring apply."
            : "Redid runtime authoring apply.";
    NotifySuccess(request, result.message);
    return result;
}

} // namespace editor
