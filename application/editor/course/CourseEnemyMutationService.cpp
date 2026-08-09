#include "CourseEnemyMutationService.h"

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <utility>

#include "../EditorTransactionStack.h"
#include "../core/EditorExecutionContext.h"
#include "../world/EditorWorldObjectRecord.h"

namespace editor {
namespace {

class CourseEnemyMutationUndoCommand final : public IEditorUndoCommand {
public:
    CourseEnemyMutationUndoCommand(
        CourseEnemyMutationSnapshot before,
        CourseEnemyMutationSnapshot after)
        : before_(std::move(before)), after_(std::move(after)) {}

    EditorUndoResult Apply(
        EditorTransactionApplyMode mode,
        EditorExecutionContext& context) const override {
        IEditorExecutionService* untyped = context.Find(
            CourseEnemyMutationService::kServiceId);
        auto* service = dynamic_cast<CourseEnemyMutationService*>(untyped);
        if (service == nullptr) {
            return EditorUndoResult::Failure(
                EditorErrorCode::MissingService,
                "Course enemy mutation service is not registered.");
        }
        return service->RestoreSnapshot(
            mode == EditorTransactionApplyMode::Undo ? before_ : after_, mode);
    }

    std::size_t EstimatedBytes() const noexcept override {
        auto bytes = [](const CourseEnemyMutationSnapshot& snapshot) {
            std::size_t total = sizeof(snapshot) +
                snapshot.placements.capacity() * sizeof(CourseEnemyPlacement);
            for (const CourseEnemyPlacement& value : snapshot.placements) {
                total += value.editorGuid.capacity() + value.actorAssetId.capacity() +
                    value.bulletPatternOverrideId.capacity() +
                    value.waveGroupGuid.capacity() +
                    value.railAnchor.segmentGuid.capacity() + 5;
            }
            return total;
        };
        return sizeof(*this) + bytes(before_) + bytes(after_);
    }

    std::string_view DomainId() const noexcept override { return "course"; }
    std::string_view TypeId() const noexcept override {
        return "course.enemy-mutation";
    }

private:
    CourseEnemyMutationSnapshot before_;
    CourseEnemyMutationSnapshot after_;
};

bool SameVector(const Vector3& a, const Vector3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool SameAnchor(const RailAnchor& a, const RailAnchor& b) {
    return a.segmentGuid == b.segmentGuid &&
        a.normalizedT == b.normalizedT &&
        a.lateralOffset == b.lateralOffset &&
        a.verticalOffset == b.verticalOffset &&
        a.forwardOffset == b.forwardOffset;
}

bool SamePlacement(const CourseEnemyPlacement& a, const CourseEnemyPlacement& b) {
    return a.editorGuid == b.editorGuid &&
        a.actorAssetId == b.actorAssetId &&
        a.bulletPatternOverrideId == b.bulletPatternOverrideId &&
        a.waveGroupGuid == b.waveGroupGuid &&
        SameAnchor(a.railAnchor, b.railAnchor) &&
        SameVector(a.localRotation, b.localRotation) &&
        SameVector(a.localScale, b.localScale) &&
        a.activationLeadDistance == b.activationLeadDistance &&
        a.enabled == b.enabled &&
        a.editorVisible == b.editorVisible &&
        a.editorLocked == b.editorLocked;
}

bool SamePlacements(
    const std::vector<CourseEnemyPlacement>& a,
    const std::vector<CourseEnemyPlacement>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t index = 0; index < a.size(); ++index) {
        if (!SamePlacement(a[index], b[index])) return false;
    }
    return true;
}

CourseEnemyPlacement* FindMutable(
    std::vector<CourseEnemyPlacement>& placements,
    std::string_view guid) {
    const auto it = std::find_if(placements.begin(), placements.end(),
        [guid](const CourseEnemyPlacement& value) {
            return value.editorGuid == guid;
        });
    return it == placements.end() ? nullptr : &*it;
}

const CourseEnemyPlacement* FindConst(
    const std::vector<CourseEnemyPlacement>& placements,
    std::string_view guid) {
    const auto it = std::find_if(placements.begin(), placements.end(),
        [guid](const CourseEnemyPlacement& value) {
            return value.editorGuid == guid;
        });
    return it == placements.end() ? nullptr : &*it;
}

std::string DefaultLabel(CourseEnemyMutationKind kind) {
    switch (kind) {
    case CourseEnemyMutationKind::AddPlacements: return "Add Enemy Placement";
    case CourseEnemyMutationKind::SetPlacements: return "Edit Enemy Placement";
    case CourseEnemyMutationKind::SetAnchors: return "Move Enemy Placement";
    case CourseEnemyMutationKind::RemovePlacements: return "Delete Enemy Placement";
    case CourseEnemyMutationKind::DuplicatePlacements: return "Duplicate Enemy Placement";
    case CourseEnemyMutationKind::ReplacePlacements: return "Replace Enemy Placements";
    case CourseEnemyMutationKind::SetEnabled: return "Set Enemy Enabled";
    case CourseEnemyMutationKind::SetVisible: return "Set Enemy Visibility";
    case CourseEnemyMutationKind::SetLocked: return "Set Enemy Lock";
    }
    return "Course Enemy Edit";
}

bool RequireTargets(
    const CourseEnemyMutationRequest& request,
    CourseEnemyMutationResult& result) {
    if (!request.placementGuids.empty()) return true;
    result.message = "Enemy mutation requires at least one placement GUID.";
    return false;
}

bool LockedPlacementsPreserved(
    const std::vector<CourseEnemyPlacement>& before,
    const std::vector<CourseEnemyPlacement>& after) {
    for (const CourseEnemyPlacement& value : before) {
        if (!value.editorLocked) continue;
        const CourseEnemyPlacement* replacement = FindConst(after, value.editorGuid);
        if (replacement == nullptr || !SamePlacement(value, *replacement)) return false;
    }
    return true;
}

} // namespace

CourseEnemyMutationService::CourseEnemyMutationService(
    CourseAsset& course,
    std::string courseIdentity,
    std::function<void()> markDirty)
    : course_(course),
      courseIdentity_(courseIdentity.empty() ? course.name : std::move(courseIdentity)),
      markDirty_(std::move(markDirty)) {
    const std::size_t assigned =
        CourseRailAuthoringModel::EnsureStableIdentity(course_, courseIdentity_) +
        CourseEnemyAuthoringModel::EnsureStableIdentity(course_, courseIdentity_);
    if (assigned > 0) {
        ++revision_;
        if (markDirty_) markDirty_();
    }
}

CourseEnemyMutationSnapshot CourseEnemyMutationService::CaptureSnapshot() const {
    return {course_.enemyPlacements, revision_};
}

CourseEnemyMutationResult CourseEnemyMutationService::Mutate(
    const CourseEnemyMutationRequest& request,
    EditorTransactionStack* transactions) {
    CourseEnemyMutationResult result{};
    result.revision = revision_;
    if (request.expectedRevision != (std::numeric_limits<uint64_t>::max)() &&
        request.expectedRevision != revision_) {
        result.message = "Course enemies changed since the edit was prepared.";
        return result;
    }

    const CourseEnemyAuthoringModel beforeModel(course_);
    if (!beforeModel.IsValid()) {
        result.message = beforeModel.ValidationError();
        return result;
    }
    const CourseEnemyMutationSnapshot beforeSnapshot = CaptureSnapshot();
    CourseAsset working = course_;

    switch (request.kind) {
    case CourseEnemyMutationKind::AddPlacements: {
        if (request.placements.empty()) {
            result.message = "Add requires at least one enemy placement.";
            return result;
        }
        for (CourseEnemyPlacement placement : request.placements) {
            if (placement.editorGuid.empty()) placement.editorGuid = GenerateEditorWorldGuid();
            if (FindMutable(working.enemyPlacements, placement.editorGuid) != nullptr) {
                result.message = "Added enemy placement GUID already exists.";
                return result;
            }
            result.affectedPlacementGuids.push_back(placement.editorGuid);
            working.enemyPlacements.push_back(std::move(placement));
        }
        break;
    }
    case CourseEnemyMutationKind::SetPlacements:
    case CourseEnemyMutationKind::SetAnchors: {
        if (request.placements.empty()) {
            result.message = "Edit requires at least one enemy placement payload.";
            return result;
        }
        std::unordered_set<std::string> edited;
        for (const CourseEnemyPlacement& replacement : request.placements) {
            if (replacement.editorGuid.empty() ||
                !edited.insert(replacement.editorGuid).second) {
                result.message = "Enemy edit payload GUIDs must be non-empty and unique.";
                return result;
            }
            CourseEnemyPlacement* target = FindMutable(
                working.enemyPlacements, replacement.editorGuid);
            if (target == nullptr) {
                result.message = "Enemy placement was not found.";
                return result;
            }
            if (target->editorLocked && !request.allowLocked) {
                result.message = "Locked enemy placement cannot be edited.";
                return result;
            }
            if (request.kind == CourseEnemyMutationKind::SetAnchors) {
                target->railAnchor = replacement.railAnchor;
            } else {
                const std::string stableGuid = target->editorGuid;
                *target = replacement;
                target->editorGuid = stableGuid;
            }
            result.affectedPlacementGuids.push_back(target->editorGuid);
        }
        break;
    }
    case CourseEnemyMutationKind::RemovePlacements: {
        if (!RequireTargets(request, result)) return result;
        std::unordered_set<std::string> targets(
            request.placementGuids.begin(), request.placementGuids.end());
        for (const std::string& guid : targets) {
            const CourseEnemyPlacement* target = FindConst(working.enemyPlacements, guid);
            if (target == nullptr) {
                result.message = "Enemy placement was not found.";
                return result;
            }
            if (target->editorLocked && !request.allowLocked) {
                result.message = "Locked enemy placement cannot be deleted.";
                return result;
            }
        }
        working.enemyPlacements.erase(
            std::remove_if(
                working.enemyPlacements.begin(), working.enemyPlacements.end(),
                [&targets](const CourseEnemyPlacement& value) {
                    return targets.find(value.editorGuid) != targets.end();
                }),
            working.enemyPlacements.end());
        result.affectedPlacementGuids.assign(targets.begin(), targets.end());
        break;
    }
    case CourseEnemyMutationKind::DuplicatePlacements: {
        if (!RequireTargets(request, result)) return result;
        std::vector<CourseEnemyPlacement> duplicates;
        duplicates.reserve(request.placementGuids.size());
        for (const std::string& guid : request.placementGuids) {
            const CourseEnemyPlacement* source = FindConst(working.enemyPlacements, guid);
            if (source == nullptr) {
                result.message = "Enemy placement was not found.";
                return result;
            }
            if (source->editorLocked && !request.allowLocked) {
                result.message = "Locked enemy placement cannot be duplicated.";
                return result;
            }
            CourseEnemyPlacement duplicate = *source;
            duplicate.editorGuid = GenerateEditorWorldGuid();
            duplicate.editorLocked = false;
            duplicate.railAnchor.lateralOffset += request.duplicateOffset.x;
            duplicate.railAnchor.verticalOffset += request.duplicateOffset.y;
            duplicate.railAnchor.forwardOffset += request.duplicateOffset.z;
            result.affectedPlacementGuids.push_back(duplicate.editorGuid);
            duplicates.push_back(std::move(duplicate));
        }
        working.enemyPlacements.insert(
            working.enemyPlacements.end(), duplicates.begin(), duplicates.end());
        break;
    }
    case CourseEnemyMutationKind::ReplacePlacements:
        if (!request.allowLocked && !LockedPlacementsPreserved(
                working.enemyPlacements, request.placements)) {
            result.message = "Replacement would modify a locked enemy placement.";
            return result;
        }
        working.enemyPlacements = request.placements;
        CourseEnemyAuthoringModel::EnsureStableIdentity(working, courseIdentity_);
        for (const CourseEnemyPlacement& value : working.enemyPlacements) {
            result.affectedPlacementGuids.push_back(value.editorGuid);
        }
        break;
    case CourseEnemyMutationKind::SetEnabled:
    case CourseEnemyMutationKind::SetVisible:
    case CourseEnemyMutationKind::SetLocked: {
        if (!RequireTargets(request, result)) return result;
        for (const std::string& guid : request.placementGuids) {
            CourseEnemyPlacement* target = FindMutable(working.enemyPlacements, guid);
            if (target == nullptr) {
                result.message = "Enemy placement was not found.";
                return result;
            }
            if (target->editorLocked && !request.allowLocked &&
                request.kind == CourseEnemyMutationKind::SetEnabled) {
                result.message = "Locked enemy placement cannot change gameplay state.";
                return result;
            }
            if (request.kind == CourseEnemyMutationKind::SetEnabled) {
                target->enabled = request.stateValue;
            } else if (request.kind == CourseEnemyMutationKind::SetVisible) {
                target->editorVisible = request.stateValue;
            } else {
                target->editorLocked = request.stateValue;
            }
            result.affectedPlacementGuids.push_back(guid);
        }
        break;
    }
    }

    CourseEnemyAuthoringModel::EnsureStableIdentity(working, courseIdentity_);
    const CourseEnemyAuthoringModel afterModel(working);
    if (!afterModel.IsValid()) {
        result.message = afterModel.ValidationError();
        return result;
    }
    if (SamePlacements(beforeSnapshot.placements, working.enemyPlacements)) {
        result.succeeded = true;
        result.message = "Course enemy mutation produced no changes.";
        return result;
    }

    CourseEnemyMutationSnapshot afterSnapshot{
        working.enemyPlacements, revision_ + 1};
    auto undoCommand = std::make_shared<CourseEnemyMutationUndoCommand>(
        beforeSnapshot, afterSnapshot);
    EditorObjectHandle transactionTarget{};
    transactionTarget.stableId = "course-enemies:" + courseIdentity_;
    transactionTarget.displayName = "Course Enemies";
    transactionTarget.generation = static_cast<uint32_t>(afterSnapshot.revision);
    const std::string label = request.label.empty()
        ? DefaultLabel(request.kind) : request.label;
    if (transactions != nullptr) {
        EditorError error{};
        if (!transactions->CanPushCommand(
                label, transactionTarget, undoCommand, &error)) {
            result.message = error.message.empty()
                ? "Course enemy transaction was rejected." : error.message;
            return result;
        }
    }

    std::string applyError;
    if (!ApplyCommittedSnapshot(afterSnapshot, &applyError)) {
        result.message = applyError;
        return result;
    }
    if (transactions != nullptr) {
        EditorError error{};
        if (!transactions->PushCommand(
                label, transactionTarget, undoCommand, &error)) {
            ApplyCommittedSnapshot(beforeSnapshot, nullptr);
            result.message = error.message.empty()
                ? "Failed to register course enemy transaction." : error.message;
            return result;
        }
    }

    result.succeeded = true;
    result.changed = true;
    result.revision = revision_;
    result.message = label;
    return result;
}

EditorUndoResult CourseEnemyMutationService::RestoreSnapshot(
    const CourseEnemyMutationSnapshot& snapshot,
    EditorTransactionApplyMode mode) {
    std::string error;
    if (!ApplyCommittedSnapshot(snapshot, &error)) {
        return EditorUndoResult::Failure(EditorErrorCode::ApplyFailed, std::move(error));
    }
    return EditorUndoResult::Success(
        mode == EditorTransactionApplyMode::Undo
            ? "Course enemy mutation undone."
            : "Course enemy mutation redone.");
}

bool CourseEnemyMutationService::ApplyCommittedSnapshot(
    const CourseEnemyMutationSnapshot& snapshot,
    std::string* errorMessage) {
    CourseAsset candidate = course_;
    candidate.enemyPlacements = snapshot.placements;
    const CourseEnemyAuthoringModel model(candidate);
    if (!model.IsValid()) {
        if (errorMessage != nullptr) *errorMessage = model.ValidationError();
        return false;
    }
    course_.enemyPlacements = std::move(candidate.enemyPlacements);
    revision_ = snapshot.revision;
    if (markDirty_) markDirty_();
    return true;
}

} // namespace editor
