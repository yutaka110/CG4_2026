#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "CourseWaveAuthoringModel.h"
#include "../core/EditorExecutionService.h"
#include "../core/EditorUndoCommand.h"

namespace editor {

class EditorTransactionStack;

enum class CourseWaveMutationKind {
    AddWaves,
    SetWaves,
    RemoveWaves,
    DuplicateWaves,
    ReplaceWaves,
    SetEnabled,
    SetVisible,
    SetLocked,
};

enum class CourseWaveReferencePolicy {
    Reject,
    ClearReferences,
};

struct CourseWaveMutationRequest final {
    CourseWaveMutationKind kind = CourseWaveMutationKind::SetWaves;
    uint64_t expectedRevision = (std::numeric_limits<uint64_t>::max)();
    std::string label;
    std::vector<std::string> waveGuids;
    std::vector<CourseWaveDefinition> waves;
    float duplicateDistanceOffset = 50.0f;
    bool stateValue = true;
    bool allowLocked = false;
    CourseWaveReferencePolicy referencePolicy = CourseWaveReferencePolicy::Reject;
};

struct CourseWaveMutationSnapshot final {
    std::vector<CourseWaveDefinition> waves;
    std::vector<CourseEnemyPlacement> enemyPlacements;
    uint64_t revision = 0;
};

struct CourseWaveMutationResult final {
    bool succeeded = false;
    bool changed = false;
    uint64_t revision = 0;
    std::string message;
    std::vector<std::string> affectedWaveGuids;
};

// Sole schema-v7 write gateway. Wave graph and enemy membership are captured
// in the same snapshot so referential cleanup is always Undoable atomically.
class CourseWaveMutationService final : public IEditorExecutionService {
public:
    static constexpr std::string_view kServiceId =
        "editor.course.wave-mutation";

    CourseWaveMutationService(
        CourseAsset& course,
        std::string courseIdentity = {},
        std::function<void()> markDirty = {});

    std::string_view ServiceId() const noexcept override { return kServiceId; }
    uint64_t Revision() const noexcept { return revision_; }
    void NotifyExternalMutation() noexcept { ++revision_; }
    CourseWaveMutationSnapshot CaptureSnapshot() const;

    CourseWaveMutationResult Mutate(
        const CourseWaveMutationRequest& request,
        EditorTransactionStack* transactions = nullptr);
    EditorUndoResult RestoreSnapshot(
        const CourseWaveMutationSnapshot& snapshot,
        EditorTransactionApplyMode mode);

private:
    bool ApplyCommittedSnapshot(
        const CourseWaveMutationSnapshot& snapshot,
        std::string* errorMessage);

    CourseAsset& course_;
    std::string courseIdentity_;
    std::function<void()> markDirty_;
    uint64_t revision_ = 0;
};

} // namespace editor
