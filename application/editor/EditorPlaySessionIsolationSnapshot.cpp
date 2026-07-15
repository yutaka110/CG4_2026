#include "EditorPlaySessionIsolationSnapshot.h"

namespace editor {

namespace {

bool ReportError(const EditorError& error, std::string* errorMessage, const char* fallback) {
    if (errorMessage != nullptr) {
        *errorMessage = error.message.empty() ? fallback : error.message;
    }
    return false;
}

} // namespace

EditorPlaySessionIsolationSnapshot::EditorPlaySessionIsolationSnapshot() {
    EditorError ignored;
    registry_.Register(&courseProvider_, &ignored);
    registry_.Register(&terrainProvider_, &ignored);
}

bool EditorPlaySessionIsolationSnapshot::BindTargets(
    const EditorPlaySessionIsolationSnapshotTarget& target,
    std::string* errorMessage) {
    if (target.course == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Course asset is unavailable for Play/Sim isolation.";
        }
        return false;
    }
    if (target.runtimeState == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = "Runtime state is unavailable for Play/Sim isolation.";
        }
        return false;
    }
    courseProvider_.Bind(target.course);
    terrainProvider_.Bind(&target.runtimeState->terrain);
    if (target.effectRuntime != nullptr) {
        vfxProvider_.Bind(target.effectRuntime);
        if (!vfxProviderRegistered_) {
            EditorError error;
            if (!registry_.Register(&vfxProvider_, &error)) {
                return ReportError(error, errorMessage, "Failed to register VFX Play isolation provider.");
            }
            vfxProviderRegistered_ = true;
        }
    } else if (vfxProviderRegistered_) {
        vfxProvider_.Bind(nullptr);
    }
    if (target.postProcessStack != nullptr) {
        postProcessProvider_.Bind(target.postProcessStack);
        if (!postProcessProviderRegistered_) {
            EditorError error;
            if (!registry_.Register(&postProcessProvider_, &error)) {
                return ReportError(error, errorMessage, "Failed to register Post-process Play isolation provider.");
            }
            postProcessProviderRegistered_ = true;
        }
    } else if (postProcessProviderRegistered_) {
        postProcessProvider_.Bind(nullptr);
    }
    return true;
}

bool EditorPlaySessionIsolationSnapshot::Capture(
    const EditorPlaySessionIsolationSnapshotTarget& target,
    std::string* errorMessage) {
    if (!BindTargets(target, errorMessage)) {
        return false;
    }
    EditorError error;
    EditorPlaySnapshot captured;
    if (!registry_.CaptureAll(captured, &error)) {
        return ReportError(error, errorMessage, "Failed to capture Play/Sim provider snapshot.");
    }
    snapshot_ = std::move(captured);
    runtimeChanges_.Clear();
    captured_ = true;
    restored_ = false;
    courseObjectRevision_ = target.runtimeState->terrain.courseObjectEditRevision;
    terrainPlacementCount_ = target.course->terrainPlacements.size();
    rockClusterCount_ = target.course->rockClusters.size();
    return true;
}

bool EditorPlaySessionIsolationSnapshot::Adopt(
    const EditorPlaySessionIsolationSnapshotTarget& target,
    std::string* errorMessage) {
    if (!RefreshRuntimeChangeSet(target, errorMessage)) {
        return false;
    }
    runtimeChanges_.SelectAll(true);
    return AdoptSelected(target, errorMessage);
}

bool EditorPlaySessionIsolationSnapshot::AdoptSelected(
    const EditorPlaySessionIsolationSnapshotTarget& target,
    std::string* errorMessage) {
    if (!captured_) {
        if (errorMessage != nullptr) *errorMessage = "Play/Sim snapshot has not been captured.";
        return false;
    }
    if (!BindTargets(target, errorMessage)) return false;
    EditorError error;
    if (!registry_.AdoptSelected(snapshot_, runtimeChanges_, &error)) {
        return ReportError(error, errorMessage, "Failed to adopt selected runtime changes.");
    }
    captured_ = true;
    restored_ = false;
    const CourseAsset* capturedCourse = CapturedCourse();
    const TerrainAuthoringState* capturedTerrain = CapturedTerrain();
    courseObjectRevision_ = capturedTerrain != nullptr
        ? capturedTerrain->courseObjectEditRevision
        : 0;
    terrainPlacementCount_ = capturedCourse != nullptr
        ? capturedCourse->terrainPlacements.size()
        : 0;
    rockClusterCount_ = capturedCourse != nullptr
        ? capturedCourse->rockClusters.size()
        : 0;
    runtimeChanges_.Clear();
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
    if (!BindTargets(target, errorMessage)) return false;
    EditorError error;
    if (!registry_.RestoreAll(snapshot_, &error)) {
        return ReportError(error, errorMessage, "Failed to restore Play/Sim provider snapshot.");
    }
    if (!registry_.FingerprintsMatch(snapshot_, &error)) {
        return ReportError(error, errorMessage, "Play/Sim provider fingerprints did not restore exactly.");
    }
    restored_ = true;
    ++restoreCount_;
    runtimeChanges_.Clear();
    return true;
}

bool EditorPlaySessionIsolationSnapshot::RefreshRuntimeChangeSet(
    const EditorPlaySessionIsolationSnapshotTarget& target,
    std::string* errorMessage) {
    if (!captured_) {
        if (errorMessage != nullptr) *errorMessage = "Play/Sim snapshot has not been captured.";
        return false;
    }
    if (!BindTargets(target, errorMessage)) return false;
    EditorError error;
    if (!registry_.BuildRuntimeChangeSet(snapshot_, runtimeChanges_, &error)) {
        return ReportError(error, errorMessage, "Failed to build Play/Sim runtime changes.");
    }
    return true;
}

bool EditorPlaySessionIsolationSnapshot::FingerprintsMatch(
    const EditorPlaySessionIsolationSnapshotTarget& target,
    std::string* errorMessage) {
    if (!BindTargets(target, errorMessage)) return false;
    EditorError error;
    if (!registry_.FingerprintsMatch(snapshot_, &error)) {
        return ReportError(error, errorMessage, "Play/Sim authoring fingerprints do not match.");
    }
    return true;
}

bool EditorPlaySessionIsolationSnapshot::RegisterProvider(
    IEditorPlayIsolationProvider* provider,
    std::string* errorMessage) {
    if (captured_) {
        if (errorMessage != nullptr) *errorMessage = "Cannot register Play isolation providers during an active snapshot.";
        return false;
    }
    EditorError error;
    if (!registry_.Register(provider, &error)) {
        return ReportError(error, errorMessage, "Failed to register Play isolation provider.");
    }
    return true;
}

void EditorPlaySessionIsolationSnapshot::Clear() {
    snapshot_.Clear();
    runtimeChanges_.Clear();
    captured_ = false;
    restored_ = false;
    courseObjectRevision_ = 0;
    terrainPlacementCount_ = 0;
    rockClusterCount_ = 0;
}

void EditorPlaySessionIsolationSnapshot::BindSession(uint64_t sessionSerial) {
    if (!captured_) {
        return;
    }
    snapshot_.BindSession(sessionSerial);
}

const CourseAsset* EditorPlaySessionIsolationSnapshot::CapturedCourse() const {
    return captured_ ? snapshot_.Read<CourseAsset>(kCoursePlayIsolationProviderId) : nullptr;
}

const TerrainAuthoringState* EditorPlaySessionIsolationSnapshot::CapturedTerrain() const {
    return captured_ ? snapshot_.Read<TerrainAuthoringState>(kTerrainPlayIsolationProviderId) : nullptr;
}

const EditorVfxAuthoringSnapshot* EditorPlaySessionIsolationSnapshot::CapturedVfxAuthoring() const {
    return captured_ && vfxProviderRegistered_
        ? snapshot_.Read<EditorVfxAuthoringSnapshot>(kVfxPlayIsolationProviderId)
        : nullptr;
}

const EditorPostProcessAuthoringSnapshot* EditorPlaySessionIsolationSnapshot::CapturedPostProcess() const {
    return captured_ && postProcessProviderRegistered_
        ? snapshot_.Read<EditorPostProcessAuthoringSnapshot>(kPostProcessPlayIsolationProviderId)
        : nullptr;
}

const char* EditorPlaySessionIsolationSnapshot::StateLabel() const {
    if (!captured_) {
        return "none";
    }
    return restored_ ? "restored" : "captured";
}

} // namespace editor
