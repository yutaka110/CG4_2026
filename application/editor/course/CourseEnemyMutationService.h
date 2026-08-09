#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "CourseEnemyAuthoringModel.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"

namespace editor {

class EditorTransactionStack;

enum class CourseEnemyMutationKind {
    AddPlacements,
    SetPlacements,
    SetAnchors,
    RemovePlacements,
    DuplicatePlacements,
    ReplacePlacements,
    SetEnabled,
    SetVisible,
    SetLocked,
};

struct CourseEnemyMutationRequest final {
    CourseEnemyMutationKind kind = CourseEnemyMutationKind::SetPlacements;
    uint64_t expectedRevision = (std::numeric_limits<uint64_t>::max)();
    std::string label;
    std::vector<std::string> placementGuids;
    std::vector<CourseEnemyPlacement> placements;
    // x/y/z map to the rail anchor's lateral/vertical/forward offsets.
    Vector3 duplicateOffset{2.0f, 0.0f, 0.0f};
    bool stateValue = true;
    bool allowLocked = false;
};

struct CourseEnemyMutationSnapshot final {
    std::vector<CourseEnemyPlacement> placements;
    uint64_t revision = 0;
};

struct CourseEnemyMutationResult final {
    bool succeeded = false;
    bool changed = false;
    uint64_t revision = 0;
    std::string message;
    std::vector<std::string> affectedPlacementGuids;
};

// Sole write gateway for persistent enemy instances. Every request is applied
// to an isolated CourseAsset candidate, validated, and then committed as one
// optional EditorTransactionStack command.
class CourseEnemyMutationService final : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId =
        "editor.course.enemy-mutation";

    CourseEnemyMutationService(
        CourseAsset& course,
        std::string courseIdentity = {},
        std::function<void()> markDirty = {});

    std::string_view ServiceId() const noexcept override { return kServiceId; }
    uint64_t Revision() const noexcept { return revision_; }
    void NotifyExternalMutation() noexcept { ++revision_; }
    CourseEnemyMutationSnapshot CaptureSnapshot() const;

    CourseEnemyMutationResult Mutate(
        const CourseEnemyMutationRequest& request,
        EditorTransactionStack* transactions = nullptr);
    EditorUndoResult RestoreSnapshot(
        const CourseEnemyMutationSnapshot& snapshot,
        EditorTransactionApplyMode mode);

private:
    bool ApplyCommittedSnapshot(
        const CourseEnemyMutationSnapshot& snapshot,
        std::string* errorMessage);

    CourseAsset& course_;
    std::string courseIdentity_;
    std::function<void()> markDirty_;
    uint64_t revision_ = 0;
};

} // namespace editor
