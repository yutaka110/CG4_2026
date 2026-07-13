#include "EditorRuntimeWatchBuilder.h"

#include "EditorPlaySessionIsolationSnapshot.h"
#include "EditorPlaySessionState.h"
#include "EditorRailRuntimePause.h"
#include "EditorSelection.h"

#include "../AppRuntimeState.h"
#include "../EffectAssetLoader.h"
#include "../EffectRuntime.h"
#include "../course/CourseAsset.h"
#include "../course/CourseCollisionSystem.h"
#include "../course/CourseSpawnRuntime.h"
#include "../course/PlayerCombatFeelSystem.h"
#include "../course/SectionCheckpointSystem.h"

#include <algorithm>
#include <sstream>

namespace editor {
namespace {

void AddRecord(
    EditorRuntimeInspector& inspector,
    std::string domain,
    std::string displayName,
    std::string state,
    std::string detail,
    EditorRuntimeWatchSeverity severity = EditorRuntimeWatchSeverity::Info,
    uint64_t frameIndex = 0) {
    inspector.AddRecord(
        EditorRuntimeWatchRecord{
            std::move(domain),
            std::move(displayName),
            std::move(state),
            std::move(detail),
            severity,
            frameIndex});
}

std::string BoolLabel(bool value) {
    return value ? "true" : "false";
}

void AppendVfxRuntime(
    EditorRuntimeInspector& inspector,
    const EffectRuntime* effectRuntime,
    const std::vector<LoadedEffectAsset>* loadedEffectAssets,
    uint32_t selectedEffectInstanceId) {
    if (effectRuntime == nullptr) {
        AddRecord(
            inspector,
            "VFX",
            "Effect Runtime",
            "Missing",
            "EffectRuntime is unavailable to Runtime Watch.",
            EditorRuntimeWatchSeverity::Warning);
        return;
    }

    const std::vector<EffectInstance>& instances = effectRuntime->Instances();
    std::ostringstream runtimeDetail;
    runtimeDetail << "Active instances " << instances.size()
                  << " paused " << BoolLabel(effectRuntime->IsPaused())
                  << " speed " << effectRuntime->SpeedMultiplier()
                  << " assets " << effectRuntime->Assets().size()
                  << " particlePoolResetSerial " << effectRuntime->ParticlePoolResetSerial();
    AddRecord(
        inspector,
        "VFX",
        "Effect Runtime",
        effectRuntime->IsAttached() ? "Attached" : "Detached",
        runtimeDetail.str(),
        effectRuntime->IsAttached()
            ? EditorRuntimeWatchSeverity::Info
            : EditorRuntimeWatchSeverity::Warning);

    AddRecord(
        inspector,
        "VFX",
        "Loaded Effect Assets",
        loadedEffectAssets != nullptr ? "Available" : "Missing",
        loadedEffectAssets != nullptr
            ? "Loaded assets " + std::to_string(loadedEffectAssets->size())
            : std::string("LoadedEffectAsset list is unavailable."),
        loadedEffectAssets != nullptr
            ? EditorRuntimeWatchSeverity::Info
            : EditorRuntimeWatchSeverity::Warning);

    if (selectedEffectInstanceId == 0) {
        AddRecord(
            inspector,
            "VFX",
            "Selected Effect Instance",
            "None",
            "No effect instance selected.");
        return;
    }

    const EffectInstance* selectedInstance = effectRuntime->FindInstance(selectedEffectInstanceId);
    if (selectedInstance == nullptr) {
        AddRecord(
            inspector,
            "VFX",
            "Selected Effect Instance",
            "Missing",
            "Selected id " + std::to_string(selectedEffectInstanceId) + " is not active.",
            EditorRuntimeWatchSeverity::Warning);
        return;
    }

    std::ostringstream detail;
    detail << "Asset "
           << (selectedInstance->asset != nullptr ? selectedInstance->asset->name : selectedInstance->assetName)
           << ", age " << selectedInstance->age
           << ", components " << selectedInstance->components.size()
           << ", previewLoop " << (selectedInstance->previewLoop ? "yes" : "no");
    AddRecord(
        inspector,
        "VFX",
        "Selected Effect Instance #" + std::to_string(selectedEffectInstanceId),
        selectedInstance->attached ? "Attached" : "Runtime",
        detail.str());
}

void AppendPlaySession(
    EditorRuntimeInspector& inspector,
    const EditorPlaySessionState* playSession,
    const EditorPlaySessionIsolationSnapshot* snapshot) {
    if (playSession == nullptr) {
        AddRecord(
            inspector,
            "Editor",
            "Play Session",
            "Missing",
            "Play session state is unavailable.",
            EditorRuntimeWatchSeverity::Warning);
        return;
    }

    std::string detail;
    EditorRuntimeWatchSeverity severity = EditorRuntimeWatchSeverity::Info;
    if (playSession->RuntimeIsolationSnapshotActive()) {
        detail = "Snapshot active; authoring will be restored on Stop";
    } else if (playSession->RuntimeIsolationPending()) {
        detail = "Boundary active; runtime clone/isolation pending";
        severity = EditorRuntimeWatchSeverity::Warning;
    } else if (playSession->RuntimeIsolationRestored()) {
        detail = "Authoring/runtime boundary restored from snapshot";
    } else {
        detail = "Authoring/runtime boundary inactive";
    }
    if (snapshot != nullptr && snapshot->Captured()) {
        detail +=
            " / snapshot " + std::string(snapshot->StateLabel()) +
            " rev " + std::to_string(snapshot->CourseObjectRevision()) +
            " placements " + std::to_string(snapshot->TerrainPlacementCount()) +
            " rocks " + std::to_string(snapshot->RockClusterCount());
    }
    detail += playSession->RuntimePaused() ? " / runtime paused" : " / runtime live";
    if (playSession->RuntimeStepRequested()) {
        detail += " / step queued";
    }
    detail += " / runtimeFrames " + std::to_string(playSession->RuntimeFrameCount());
    detail += " / resets " + std::to_string(playSession->RuntimeResetCount());

    AddRecord(
        inspector,
        "Editor",
        "Play Session",
        ToString(playSession->Mode()),
        detail,
        playSession->RuntimePaused() ? EditorRuntimeWatchSeverity::Warning : severity,
        playSession->FrameCount());
}

void AppendSelection(EditorRuntimeInspector& inspector, const EditorSelection* selection) {
    if (selection == nullptr) {
        AddRecord(
            inspector,
            "Editor",
            "Selection",
            "Missing",
            "Editor selection service is unavailable.",
            EditorRuntimeWatchSeverity::Warning);
        return;
    }

    const EditorObjectHandle* primary = selection->Primary();
    if (primary == nullptr) {
        AddRecord(
            inspector,
            "Editor",
            "Selection",
            "None",
            "Selected objects 0 revision " + std::to_string(selection->Revision()));
        return;
    }

    std::ostringstream detail;
    detail << "Selected objects " << selection->Count()
           << " revision " << selection->Revision()
           << " stableId " << primary->stableId
           << " localIndex " << primary->localIndex
           << " generation " << primary->generation;
    AddRecord(
        inspector,
        "Editor",
        "Selection",
        ToString(primary->domain),
        detail.str(),
        EditorRuntimeWatchSeverity::Info,
        primary->generation);
}

void AppendRailPause(EditorRuntimeInspector& inspector, const EditorRailRuntimePause* railPause) {
    if (railPause == nullptr) {
        AddRecord(
            inspector,
            "Course Runtime",
            "Preview Freeze",
            "Missing",
            "Rail runtime pause state is unavailable.",
            EditorRuntimeWatchSeverity::Warning);
        return;
    }

    const EditorRailRuntimePauseState& state = railPause->State();
    std::ostringstream detail;
    detail << "distance=" << state.distance
           << " speed=" << state.speed
           << " frozenFrames=" << state.frozenFrames
           << " revision=" << state.revision;
    AddRecord(
        inspector,
        "Course Runtime",
        "Preview Freeze",
        railPause->StatusLabel(),
        detail.str(),
        state.frozen ? EditorRuntimeWatchSeverity::Warning : EditorRuntimeWatchSeverity::Info,
        state.frozenFrames);
}

void AppendCourseRuntime(EditorRuntimeInspector& inspector, const EditorRuntimeWatchBuildInput& input) {
    if (input.course == nullptr || input.runtimeState == nullptr) {
        AddRecord(
            inspector,
            "Course Runtime",
            "Course Director",
            "Missing",
            "Course or runtime terrain state is unavailable.",
            EditorRuntimeWatchSeverity::Warning);
        return;
    }

    const TerrainAuthoringState& terrain = input.runtimeState->terrain;
    std::ostringstream detail;
    detail << "distance=" << input.courseDistance
           << " speed=" << input.courseSpeed
           << " railLength=" << input.courseRailLength
           << " previewDistance=" << terrain.previewDistance
           << " authoringRevision=" << terrain.courseObjectEditRevision
           << " sections=" << input.course->sections.size()
           << " events=" << input.course->events.size()
           << " terrain=" << input.course->terrainPlacements.size()
           << " rocks=" << input.course->rockClusters.size();
    AddRecord(
        inspector,
        "Course Runtime",
        "Course Director",
        input.courseRailLength > 0.0f ? "Loaded" : "NoRail",
        detail.str(),
        input.courseRailLength > 0.0f ? EditorRuntimeWatchSeverity::Info : EditorRuntimeWatchSeverity::Warning);

    if (input.courseCheckpointSystem != nullptr) {
        const SectionCheckpointStats& stats = input.courseCheckpointSystem->LastStats();
        std::ostringstream sectionDetail;
        sectionDetail << "index=" << stats.currentSectionIndex
                      << " name=" << stats.currentSectionName
                      << " category=" << stats.currentSectionCategory
                      << " checkpoint=" << stats.checkpointDistance
                      << " transitions=" << stats.sectionTransitions
                      << " teleports=" << stats.authoringTeleports;
        AddRecord(
            inspector,
            "Course Runtime",
            "Section Checkpoint",
            stats.currentSectionIndex >= 0 ? "Active" : "None",
            sectionDetail.str(),
            stats.currentSectionIndex >= 0 ? EditorRuntimeWatchSeverity::Info : EditorRuntimeWatchSeverity::Warning,
            stats.sectionTransitions);
    }
}

void AppendGameplaySystems(EditorRuntimeInspector& inspector, const EditorRuntimeWatchBuildInput& input) {
    if (input.courseSpawnRuntime != nullptr) {
        const CourseEnemyFireSafetyStats& fireStats = input.courseSpawnRuntime->LastFireSafetyStats();
        std::ostringstream detail;
        detail << "enemies=" << input.courseSpawnRuntime->ActiveEnemyCount()
               << " bullets=" << input.courseSpawnRuntime->ActiveBulletCount()
               << " obstacles=" << input.courseSpawnRuntime->ActiveObstacleCount()
               << " vfxCues=" << input.courseSpawnRuntime->ActiveVfxCueCount()
               << " bulletsEmitted=" << fireStats.bulletsEmitted
               << " blockedCamera=" << fireStats.blockedByCamera
               << " blockedRange=" << fireStats.blockedByRange
               << " lastAllowed=" << fireStats.lastAllowedReason
               << " lastBlocked=" << fireStats.lastBlockedReason;
        AddRecord(
            inspector,
            "Gameplay",
            "Spawn Runtime",
            input.courseSpawnRuntime->ActiveEnemyCount() > 0 ? "Active" : "Idle",
            detail.str());
    }

    if (input.courseCollisionSystem != nullptr) {
        const CourseCollisionFrameStats& stats = input.courseCollisionSystem->LastFrameStats();
        const CourseCollisionPlayerState& player = input.courseCollisionSystem->Player();
        std::ostringstream detail;
        detail << "playerDistance=" << player.distance
               << " hp=" << player.hitPoints
               << " shots=" << stats.playerShotsFired
               << " shotEnemyHits=" << stats.playerShotEnemyHits
               << " shotObstacleHits=" << stats.playerShotObstacleHits
               << " enemyBulletHits=" << stats.enemyBulletHits
               << " obstacleHits=" << stats.obstacleHits
               << " playerDamage=" << stats.playerDamage
               << " lastShotVisible=" << BoolLabel(input.courseCollisionSystem->LastShotVisible());
        AddRecord(
            inspector,
            "Gameplay",
            "Collision System",
            stats.playerDamage > 0.0f ? "Damage" : "Tracking",
            detail.str(),
            stats.playerDamage > 0.0f ? EditorRuntimeWatchSeverity::Warning : EditorRuntimeWatchSeverity::Info,
            stats.playerShotsFired);
    }

    if (input.playerCombatFeelSystem != nullptr) {
        const PlayerCombatFeelStats& stats = input.playerCombatFeelSystem->LastStats();
        std::ostringstream detail;
        detail << "score=" << stats.score
               << " combo=" << stats.combo
               << " maxCombo=" << stats.maxCombo
               << " lockActive=" << BoolLabel(stats.lockOnActive)
               << " lockTarget=" << stats.lockOnTarget
               << " tokens=" << stats.lastLockTokenCount
               << " hits=" << stats.lastLockHitCount
               << " hitStop=" << stats.hitStopTime
               << " cameraShake=" << stats.cameraShake;
        AddRecord(
            inspector,
            "Gameplay",
            "Combat Feel",
            stats.lockOnActive ? "LockOn" : "Normal",
            detail.str(),
            stats.combo > 0 ? EditorRuntimeWatchSeverity::Info : EditorRuntimeWatchSeverity::Info,
            stats.score);
    }
}

void AppendRenderGraph(EditorRuntimeInspector& inspector, const EditorRuntimeWatchBuildInput& input) {
    if (input.renderPassDebugInfo == nullptr) {
        AddRecord(
            inspector,
            "RenderGraph",
            "Pass Summary",
            "Missing",
            "RenderGraph debug info is unavailable.",
            EditorRuntimeWatchSeverity::Warning);
        return;
    }

    const std::vector<ge3::graphics::RenderPassDebugInfo>& passes = *input.renderPassDebugInfo;
    const uint32_t executedCount = static_cast<uint32_t>(
        std::count_if(
            passes.begin(),
            passes.end(),
            [](const ge3::graphics::RenderPassDebugInfo& pass) {
                return pass.executed;
            }));
    std::ostringstream detail;
    detail << "passes=" << passes.size()
           << " executed=" << executedCount
           << " transientTargets=" << input.transientTargetCount << "/" << input.transientTargetStorageCount
           << " transientBuffers=" << input.transientBufferCount << "/" << input.transientBufferStorageCount;
    if (input.renderGraphError != nullptr && !input.renderGraphError->empty()) {
        detail << " error=" << *input.renderGraphError;
    }
    AddRecord(
        inspector,
        "RenderGraph",
        "Pass Summary",
        input.renderGraphError != nullptr && !input.renderGraphError->empty() ? "Error" : "Ready",
        detail.str(),
        input.renderGraphError != nullptr && !input.renderGraphError->empty()
            ? EditorRuntimeWatchSeverity::Error
            : EditorRuntimeWatchSeverity::Info,
        executedCount);

    for (const ge3::graphics::RenderPassDebugInfo& pass : passes) {
        if (!pass.executed && !pass.forceExecute) {
            continue;
        }
        std::ostringstream passDetail;
        passDetail << "accesses=" << pass.accesses.size()
                   << " outputs=" << pass.requiredOutputs.size()
                   << " depth=" << pass.depthTarget
                   << " reason=" << pass.reason;
        AddRecord(
            inspector,
            "RenderGraph",
            pass.name,
            pass.executed ? "Executed" : "Forced",
            passDetail.str(),
            pass.executed ? EditorRuntimeWatchSeverity::Info : EditorRuntimeWatchSeverity::Warning,
            static_cast<uint64_t>((std::max)(pass.executionIndex, 0)));
    }
}

} // namespace

void BuildEditorRuntimeWatch(const EditorRuntimeWatchBuildInput& input) {
    if (input.inspector == nullptr) {
        return;
    }

    EditorRuntimeInspector& inspector = *input.inspector;
    inspector.Clear();
    AppendDefaultEditorRuntimeWatch(input);
}

void AppendDefaultEditorRuntimeWatch(const EditorRuntimeWatchBuildInput& input) {
    if (input.inspector == nullptr) {
        return;
    }

    EditorRuntimeInspector& inspector = *input.inspector;
    AppendPlaySession(inspector, input.playSession, input.playSessionSnapshot);
    AppendSelection(inspector, input.selection);
    AppendRailPause(inspector, input.railRuntimePause);
    AppendCourseRuntime(inspector, input);
    AppendVfxRuntime(
        inspector,
        input.effectRuntime,
        input.loadedEffectAssets,
        input.selectedEffectInstanceId);
    AppendGameplaySystems(inspector, input);
    AppendRenderGraph(inspector, input);
}

} // namespace editor
