#include "EditorPlaySessionIsolationSnapshot.h"

namespace editor {

bool EditorPlaySessionIsolationSnapshot::Capture(
    const EditorPlaySessionIsolationSnapshotTarget& target,
    std::string* errorMessage) {
    if (target.course == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course asset is unavailable for Play/Sim snapshot.";
        }
        return false;
    }
    if (target.runtimeState == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Runtime state is unavailable for Play/Sim snapshot.";
        }
        return false;
    }

    course_ = *target.course;
    terrain_ = target.runtimeState->terrain;
    captured_ = true;
    restored_ = false;
    sessionSerial_ = 0;
    courseObjectRevision_ = terrain_.courseObjectEditRevision;
    terrainPlacementCount_ = course_.terrainPlacements.size();
    rockClusterCount_ = course_.rockClusters.size();
    return true;
}

bool EditorPlaySessionIsolationSnapshot::Adopt(
    const EditorPlaySessionIsolationSnapshotTarget& target,
    std::string* errorMessage) {
    if (target.course == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course asset is unavailable for Play/Sim snapshot adoption.";
        }
        return false;
    }
    if (target.runtimeState == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Runtime state is unavailable for Play/Sim snapshot adoption.";
        }
        return false;
    }

    course_ = *target.course;
    terrain_ = target.runtimeState->terrain;
    captured_ = true;
    restored_ = false;
    courseObjectRevision_ = terrain_.courseObjectEditRevision;
    terrainPlacementCount_ = course_.terrainPlacements.size();
    rockClusterCount_ = course_.rockClusters.size();
    return true;
}

bool EditorPlaySessionIsolationSnapshot::Restore(
    const EditorPlaySessionIsolationSnapshotTarget& target,
    std::string* errorMessage) {
    if (!captured_) {
        if (errorMessage != nullptr) {
            *errorMessage = "Play/Sim snapshot has not been captured.";
        }
        return false;
    }
    if (target.course == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course asset is unavailable for Play/Sim restore.";
        }
        return false;
    }
    if (target.runtimeState == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Runtime state is unavailable for Play/Sim restore.";
        }
        return false;
    }

    *target.course = course_;
    target.runtimeState->terrain = terrain_;
    restored_ = true;
    ++restoreCount_;
    return true;
}

void EditorPlaySessionIsolationSnapshot::Clear() {
    course_ = CourseAsset{};
    terrain_ = TerrainAuthoringState{};
    captured_ = false;
    restored_ = false;
    sessionSerial_ = 0;
    courseObjectRevision_ = 0;
    terrainPlacementCount_ = 0;
    rockClusterCount_ = 0;
}

void EditorPlaySessionIsolationSnapshot::BindSession(uint64_t sessionSerial) {
    if (!captured_) {
        return;
    }
    sessionSerial_ = sessionSerial;
}

const char* EditorPlaySessionIsolationSnapshot::StateLabel() const {
    if (!captured_) {
        return "none";
    }
    return restored_ ? "restored" : "captured";
}

} // namespace editor
