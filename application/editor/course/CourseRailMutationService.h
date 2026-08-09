#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "CourseRailAuthoringModel.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"

namespace editor {

class EditorTransactionStack;

enum class CourseRailMutationKind {
    InsertPoint,
    MovePoint,
    SetPoint,
    RemovePoint,
    ReplaceRail,
    SetAnchor,
    RemoveAnchor,
};

struct CourseRailMutationRequest final {
    CourseRailMutationKind kind = CourseRailMutationKind::MovePoint;
    uint64_t expectedRevision = (std::numeric_limits<uint64_t>::max)();
    std::string label;
    std::string pointGuid;
    std::string segmentGuid;
    float normalizedT = 0.5f;
    RailPathControlPoint point{};
    std::vector<RailPathControlPoint> replacementPoints;
    std::string ownerGuid;
    RailAnchor anchor{};
    bool reprojectOrphanedAnchors = true;
};

struct CourseRailMutationSnapshot final {
    std::vector<RailPathControlPoint> railPoints;
    std::vector<CourseRailAnchorBinding> railAnchors;
    std::vector<CourseEnemyPlacement> enemyPlacements;
    uint64_t revision = 0;
};

struct CourseRailMutationResult final {
    bool succeeded = false;
    bool changed = false;
    uint64_t revision = 0;
    std::string message;
    std::string affectedPointGuid;
    std::string affectedSegmentGuid;
};

// The only write gateway for course rail topology. Mutations are validated on
// an isolated CourseAsset copy, then committed to both authoring and runtime in
// one step. Register this service in EditorExecutionContext to enable command
// based Undo/Redo through EditorTransactionStack.
class CourseRailMutationService final : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId = "editor.course.rail-mutation";

    CourseRailMutationService(
        CourseAsset& course,
        RailPath* runtimeRailPath = nullptr,
        std::string courseIdentity = {},
        std::function<void()> markDirty = {});

    std::string_view ServiceId() const noexcept override { return kServiceId; }
    uint64_t Revision() const noexcept { return revision_; }
    CourseRailMutationSnapshot CaptureSnapshot() const;

    CourseRailMutationResult Mutate(
        const CourseRailMutationRequest& request,
        EditorTransactionStack* transactions = nullptr);
    EditorUndoResult RestoreSnapshot(
        const CourseRailMutationSnapshot& snapshot,
        EditorTransactionApplyMode mode);

private:
    bool ApplyCommittedSnapshot(
        const CourseRailMutationSnapshot& snapshot,
        std::string* errorMessage);
    static void RefreshLegacyDistances(CourseAsset& course);

    CourseAsset& course_;
    RailPath* runtimeRailPath_ = nullptr;
    std::string courseIdentity_;
    std::function<void()> markDirty_;
    uint64_t revision_ = 0;
};

} // namespace editor
