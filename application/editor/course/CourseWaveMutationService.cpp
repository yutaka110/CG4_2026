#include "CourseWaveMutationService.h"

#include "../EditorTransactionStack.h"
#include "../core/EditorExecutionContext.h"
#include "../world/EditorWorldObjectRecord.h"

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <utility>

namespace editor {
namespace {

class CourseWaveMutationUndoCommand final : public IEditorUndoCommand {
public:
    CourseWaveMutationUndoCommand(
        CourseWaveMutationSnapshot before,
        CourseWaveMutationSnapshot after)
        : before_(std::move(before)), after_(std::move(after)) {}

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override {
        auto* service = dynamic_cast<CourseWaveMutationService*>(
            context.Find(CourseWaveMutationService::kServiceId));
        if (service == nullptr) {
            return EditorUndoResult::Failure(
                EditorErrorCode::MissingService,
                "Course wave mutation service is not registered.");
        }
        return service->RestoreSnapshot(
            mode == EditorTransactionApplyMode::Undo ? before_ : after_, mode);
    }

    std::size_t EstimatedBytes() const noexcept override {
        const auto bytes = [](const CourseWaveMutationSnapshot& snapshot) {
            std::size_t total = sizeof(snapshot) +
                snapshot.waves.capacity() * sizeof(CourseWaveDefinition) +
                snapshot.enemyPlacements.capacity() * sizeof(CourseEnemyPlacement);
            for (const CourseWaveDefinition& wave : snapshot.waves) {
                total += wave.editorGuid.capacity() + wave.displayName.capacity() +
                    wave.nextWaveGuid.capacity() + wave.triggerEventId.capacity() + 4;
            }
            for (const CourseEnemyPlacement& placement : snapshot.enemyPlacements) {
                total += placement.editorGuid.capacity() +
                    placement.actorAssetId.capacity() +
                    placement.bulletPatternOverrideId.capacity() +
                    placement.waveGroupGuid.capacity() +
                    placement.railAnchor.segmentGuid.capacity() + 5;
            }
            return total;
        };
        return sizeof(*this) + bytes(before_) + bytes(after_);
    }

    std::string_view DomainId() const noexcept override { return "course"; }
    std::string_view TypeId() const noexcept override {
        return "course.wave-mutation";
    }

private:
    CourseWaveMutationSnapshot before_;
    CourseWaveMutationSnapshot after_;
};

bool SameWave(const CourseWaveDefinition& a, const CourseWaveDefinition& b) {
    return a.editorGuid == b.editorGuid && a.displayName == b.displayName &&
        a.triggerRailDistance == b.triggerRailDistance &&
        a.prewarmDistance == b.prewarmDistance &&
        a.timeoutSeconds == b.timeoutSeconds &&
        a.completionCondition == b.completionCondition &&
        a.executionPolicy == b.executionPolicy &&
        a.nextWaveGuid == b.nextWaveGuid &&
        a.triggerEventId == b.triggerEventId && a.enabled == b.enabled &&
        a.editorVisible == b.editorVisible && a.editorLocked == b.editorLocked;
}

bool SameWaves(
    const std::vector<CourseWaveDefinition>& a,
    const std::vector<CourseWaveDefinition>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t index = 0; index < a.size(); ++index) {
        if (!SameWave(a[index], b[index])) return false;
    }
    return true;
}

bool SameMembership(
    const std::vector<CourseEnemyPlacement>& a,
    const std::vector<CourseEnemyPlacement>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t index = 0; index < a.size(); ++index) {
        if (a[index].editorGuid != b[index].editorGuid ||
            a[index].waveGroupGuid != b[index].waveGroupGuid) {
            return false;
        }
    }
    return true;
}

CourseWaveDefinition* FindMutable(
    std::vector<CourseWaveDefinition>& waves,
    std::string_view guid) {
    const auto found = std::find_if(waves.begin(), waves.end(),
        [guid](const CourseWaveDefinition& wave) {
            return wave.editorGuid == guid;
        });
    return found == waves.end() ? nullptr : &*found;
}

const CourseWaveDefinition* FindConst(
    const std::vector<CourseWaveDefinition>& waves,
    std::string_view guid) {
    const auto found = std::find_if(waves.begin(), waves.end(),
        [guid](const CourseWaveDefinition& wave) {
            return wave.editorGuid == guid;
        });
    return found == waves.end() ? nullptr : &*found;
}

bool LockedWavesPreserved(
    const std::vector<CourseWaveDefinition>& before,
    const std::vector<CourseWaveDefinition>& after) {
    for (const CourseWaveDefinition& wave : before) {
        if (!wave.editorLocked) continue;
        const CourseWaveDefinition* replacement = FindConst(after, wave.editorGuid);
        if (replacement == nullptr || !SameWave(wave, *replacement)) return false;
    }
    return true;
}

std::string DefaultLabel(CourseWaveMutationKind kind) {
    switch (kind) {
    case CourseWaveMutationKind::AddWaves: return "Add Course Wave";
    case CourseWaveMutationKind::SetWaves: return "Edit Course Wave";
    case CourseWaveMutationKind::RemoveWaves: return "Delete Course Wave";
    case CourseWaveMutationKind::DuplicateWaves: return "Duplicate Course Wave";
    case CourseWaveMutationKind::ReplaceWaves: return "Replace Course Waves";
    case CourseWaveMutationKind::SetEnabled: return "Set Course Wave Enabled";
    case CourseWaveMutationKind::SetVisible: return "Set Course Wave Visibility";
    case CourseWaveMutationKind::SetLocked: return "Set Course Wave Lock";
    }
    return "Course Wave Edit";
}

} // namespace

CourseWaveMutationService::CourseWaveMutationService(
    CourseAsset& course,
    std::string courseIdentity,
    std::function<void()> markDirty)
    : course_(course),
      courseIdentity_(courseIdentity.empty() ? course.name : std::move(courseIdentity)),
      markDirty_(std::move(markDirty)) {
    const CourseWaveLegacyUpgradeResult upgraded =
        CourseWaveAuthoringModel::UpgradeLegacyWaveGroups(course_, courseIdentity_);
    if (upgraded.Changed()) {
        ++revision_;
        if (markDirty_) markDirty_();
    }
}

CourseWaveMutationSnapshot CourseWaveMutationService::CaptureSnapshot() const {
    return {course_.waveDefinitions, course_.enemyPlacements, revision_};
}

CourseWaveMutationResult CourseWaveMutationService::Mutate(
    const CourseWaveMutationRequest& request,
    EditorTransactionStack* transactions) {
    CourseWaveMutationResult result{};
    result.revision = revision_;
    if (request.expectedRevision != (std::numeric_limits<uint64_t>::max)() &&
        request.expectedRevision != revision_) {
        result.message = "Course waves changed since the edit was prepared.";
        return result;
    }
    const CourseWaveAuthoringModel beforeModel(course_);
    if (!beforeModel.IsValid()) {
        result.message = beforeModel.ValidationError();
        return result;
    }

    const CourseWaveMutationSnapshot before = CaptureSnapshot();
    CourseAsset working = course_;
    switch (request.kind) {
    case CourseWaveMutationKind::AddWaves:
        if (request.waves.empty()) {
            result.message = "Add requires at least one Course wave.";
            return result;
        }
        for (CourseWaveDefinition wave : request.waves) {
            if (wave.editorGuid.empty()) wave.editorGuid = GenerateEditorWorldGuid();
            if (FindMutable(working.waveDefinitions, wave.editorGuid) != nullptr) {
                result.message = "Added Course wave GUID already exists.";
                return result;
            }
            result.affectedWaveGuids.push_back(wave.editorGuid);
            working.waveDefinitions.push_back(std::move(wave));
        }
        break;
    case CourseWaveMutationKind::SetWaves:
        if (request.waves.empty()) {
            result.message = "Edit requires at least one Course wave payload.";
            return result;
        }
        {
            std::unordered_set<std::string> edited;
            for (const CourseWaveDefinition& replacement : request.waves) {
                if (replacement.editorGuid.empty() ||
                    !edited.insert(replacement.editorGuid).second) {
                    result.message = "Wave edit GUIDs must be non-empty and unique.";
                    return result;
                }
                CourseWaveDefinition* target = FindMutable(
                    working.waveDefinitions, replacement.editorGuid);
                if (target == nullptr) {
                    result.message = "Course wave was not found.";
                    return result;
                }
                if (target->editorLocked && !request.allowLocked) {
                    result.message = "Locked Course wave cannot be edited.";
                    return result;
                }
                const std::string stableGuid = target->editorGuid;
                *target = replacement;
                target->editorGuid = stableGuid;
                result.affectedWaveGuids.push_back(stableGuid);
            }
        }
        break;
    case CourseWaveMutationKind::RemoveWaves: {
        if (request.waveGuids.empty()) {
            result.message = "Delete requires at least one Course wave GUID.";
            return result;
        }
        const std::unordered_set<std::string> targets(
            request.waveGuids.begin(), request.waveGuids.end());
        bool hasReferences = false;
        for (const std::string& guid : targets) {
            const CourseWaveDefinition* wave = FindConst(working.waveDefinitions, guid);
            if (wave == nullptr) {
                result.message = "Course wave was not found.";
                return result;
            }
            if (wave->editorLocked && !request.allowLocked) {
                result.message = "Locked Course wave cannot be deleted.";
                return result;
            }
        }
        for (const CourseEnemyPlacement& placement : working.enemyPlacements) {
            hasReferences |= targets.contains(placement.waveGroupGuid);
        }
        for (const CourseWaveDefinition& wave : working.waveDefinitions) {
            hasReferences |= targets.contains(wave.nextWaveGuid) &&
                !targets.contains(wave.editorGuid);
        }
        if (hasReferences &&
            request.referencePolicy == CourseWaveReferencePolicy::Reject) {
            result.message =
                "Course wave is referenced; choose ClearReferences to delete atomically.";
            return result;
        }
        if (request.referencePolicy == CourseWaveReferencePolicy::ClearReferences) {
            for (CourseEnemyPlacement& placement : working.enemyPlacements) {
                if (targets.contains(placement.waveGroupGuid)) {
                    placement.waveGroupGuid.clear();
                }
            }
            for (CourseWaveDefinition& wave : working.waveDefinitions) {
                if (!targets.contains(wave.editorGuid) &&
                    targets.contains(wave.nextWaveGuid)) {
                    wave.nextWaveGuid.clear();
                }
            }
        }
        working.waveDefinitions.erase(
            std::remove_if(
                working.waveDefinitions.begin(), working.waveDefinitions.end(),
                [&targets](const CourseWaveDefinition& wave) {
                    return targets.contains(wave.editorGuid);
                }),
            working.waveDefinitions.end());
        result.affectedWaveGuids.assign(targets.begin(), targets.end());
        break;
    }
    case CourseWaveMutationKind::DuplicateWaves:
        if (request.waveGuids.empty()) {
            result.message = "Duplicate requires at least one Course wave GUID.";
            return result;
        }
        {
            std::vector<CourseWaveDefinition> duplicates;
            for (const std::string& guid : request.waveGuids) {
                const CourseWaveDefinition* source =
                    FindConst(working.waveDefinitions, guid);
                if (source == nullptr) {
                    result.message = "Course wave was not found.";
                    return result;
                }
                if (source->editorLocked && !request.allowLocked) {
                    result.message = "Locked Course wave cannot be duplicated.";
                    return result;
                }
                CourseWaveDefinition duplicate = *source;
                duplicate.editorGuid = GenerateEditorWorldGuid();
                duplicate.displayName += " Copy";
                duplicate.triggerRailDistance += request.duplicateDistanceOffset;
                duplicate.nextWaveGuid.clear();
                duplicate.editorLocked = false;
                result.affectedWaveGuids.push_back(duplicate.editorGuid);
                duplicates.push_back(std::move(duplicate));
            }
            working.waveDefinitions.insert(
                working.waveDefinitions.end(), duplicates.begin(), duplicates.end());
        }
        break;
    case CourseWaveMutationKind::ReplaceWaves:
        if (!request.allowLocked && !LockedWavesPreserved(
                working.waveDefinitions, request.waves)) {
            result.message = "Replacement would modify a locked Course wave.";
            return result;
        }
        working.waveDefinitions = request.waves;
        CourseWaveAuthoringModel::EnsureStableIdentity(working, courseIdentity_);
        for (const CourseWaveDefinition& wave : working.waveDefinitions) {
            result.affectedWaveGuids.push_back(wave.editorGuid);
        }
        break;
    case CourseWaveMutationKind::SetEnabled:
    case CourseWaveMutationKind::SetVisible:
    case CourseWaveMutationKind::SetLocked:
        if (request.waveGuids.empty()) {
            result.message = "State edit requires at least one Course wave GUID.";
            return result;
        }
        for (const std::string& guid : request.waveGuids) {
            CourseWaveDefinition* wave = FindMutable(working.waveDefinitions, guid);
            if (wave == nullptr) {
                result.message = "Course wave was not found.";
                return result;
            }
            if (wave->editorLocked && !request.allowLocked &&
                request.kind == CourseWaveMutationKind::SetEnabled) {
                result.message = "Locked Course wave cannot change gameplay state.";
                return result;
            }
            if (request.kind == CourseWaveMutationKind::SetEnabled) {
                wave->enabled = request.stateValue;
            } else if (request.kind == CourseWaveMutationKind::SetVisible) {
                wave->editorVisible = request.stateValue;
            } else {
                wave->editorLocked = request.stateValue;
            }
            result.affectedWaveGuids.push_back(guid);
        }
        break;
    }

    CourseWaveAuthoringModel::EnsureStableIdentity(working, courseIdentity_);
    const CourseWaveAuthoringModel afterModel(working);
    if (!afterModel.IsValid()) {
        result.message = afterModel.ValidationError();
        return result;
    }
    if (SameWaves(before.waves, working.waveDefinitions) &&
        SameMembership(before.enemyPlacements, working.enemyPlacements)) {
        result.succeeded = true;
        result.message = "Course wave mutation produced no changes.";
        return result;
    }

    CourseWaveMutationSnapshot after{
        working.waveDefinitions, working.enemyPlacements, revision_ + 1};
    auto command = std::make_shared<CourseWaveMutationUndoCommand>(before, after);
    EditorObjectHandle target{};
    target.stableId = "course-waves:" + courseIdentity_;
    target.displayName = "Course Waves";
    target.generation = static_cast<uint32_t>(after.revision);
    const std::string label = request.label.empty()
        ? DefaultLabel(request.kind) : request.label;
    if (transactions != nullptr) {
        EditorError error{};
        if (!transactions->CanPushCommand(label, target, command, &error)) {
            result.message = error.message.empty()
                ? "Course wave transaction was rejected." : error.message;
            return result;
        }
    }
    std::string applyError;
    if (!ApplyCommittedSnapshot(after, &applyError)) {
        result.message = applyError;
        return result;
    }
    if (transactions != nullptr) {
        EditorError error{};
        if (!transactions->PushCommand(label, target, command, &error)) {
            ApplyCommittedSnapshot(before, nullptr);
            result.message = error.message.empty()
                ? "Failed to register Course wave transaction." : error.message;
            return result;
        }
    }
    result.succeeded = true;
    result.changed = true;
    result.revision = revision_;
    result.message = label;
    return result;
}

EditorUndoResult CourseWaveMutationService::RestoreSnapshot(
    const CourseWaveMutationSnapshot& snapshot,
    EditorTransactionApplyMode mode) {
    std::string error;
    if (!ApplyCommittedSnapshot(snapshot, &error)) {
        return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, std::move(error));
    }
    return EditorUndoResult::Success(
        mode == EditorTransactionApplyMode::Undo
            ? "Course wave mutation undone."
            : "Course wave mutation redone.");
}

bool CourseWaveMutationService::ApplyCommittedSnapshot(
    const CourseWaveMutationSnapshot& snapshot,
    std::string* errorMessage) {
    CourseAsset candidate = course_;
    candidate.waveDefinitions = snapshot.waves;
    candidate.enemyPlacements = snapshot.enemyPlacements;
    const CourseWaveAuthoringModel model(candidate);
    if (!model.IsValid()) {
        if (errorMessage != nullptr) *errorMessage = model.ValidationError();
        return false;
    }
    course_.waveDefinitions = std::move(candidate.waveDefinitions);
    course_.enemyPlacements = std::move(candidate.enemyPlacements);
    revision_ = snapshot.revision;
    if (markDirty_) markDirty_();
    return true;
}

} // namespace editor
