#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "../AppRuntimeState.h"
#include "../course/CourseAsset.h"
#include "play/EditorBuiltinPlayIsolationProviders.h"
#include "play/EditorPlayIsolationRegistry.h"
#include "play/EditorPlaySnapshot.h"
#include "play/EditorRuntimeChangeSet.h"

struct AppRuntimeState;
struct CourseAsset;

namespace editor {

struct EditorPlaySessionIsolationSnapshotTarget {
    CourseAsset* course = nullptr;
    AppRuntimeState* runtimeState = nullptr;
    EffectRuntime* effectRuntime = nullptr;
    PostProcessStack* postProcessStack = nullptr;
};

class EditorPlaySessionIsolationSnapshot {
public:
    EditorPlaySessionIsolationSnapshot();
    EditorPlaySessionIsolationSnapshot(const EditorPlaySessionIsolationSnapshot&) = delete;
    EditorPlaySessionIsolationSnapshot& operator=(const EditorPlaySessionIsolationSnapshot&) = delete;

    bool Capture(const EditorPlaySessionIsolationSnapshotTarget& target, std::string* errorMessage = nullptr);
    bool Adopt(const EditorPlaySessionIsolationSnapshotTarget& target, std::string* errorMessage = nullptr);
    bool AdoptSelected(const EditorPlaySessionIsolationSnapshotTarget& target, std::string* errorMessage = nullptr);
    bool Restore(const EditorPlaySessionIsolationSnapshotTarget& target, std::string* errorMessage = nullptr);
    bool RefreshRuntimeChangeSet(
        const EditorPlaySessionIsolationSnapshotTarget& target,
        std::string* errorMessage = nullptr);
    bool FingerprintsMatch(
        const EditorPlaySessionIsolationSnapshotTarget& target,
        std::string* errorMessage = nullptr);
    bool RegisterProvider(IEditorPlayIsolationProvider* provider, std::string* errorMessage = nullptr);
    void Clear();
    void BindSession(uint64_t sessionSerial);

    bool Captured() const { return captured_; }
    bool Restored() const { return restored_; }
    uint64_t SessionSerial() const { return snapshot_.SessionSerial(); }
    uint32_t RestoreCount() const { return restoreCount_; }
    uint32_t CourseObjectRevision() const { return courseObjectRevision_; }
    std::size_t TerrainPlacementCount() const { return terrainPlacementCount_; }
    std::size_t RockClusterCount() const { return rockClusterCount_; }
    const CourseAsset* CapturedCourse() const;
    const TerrainAuthoringState* CapturedTerrain() const;
    const EditorVfxAuthoringSnapshot* CapturedVfxAuthoring() const;
    const EditorPostProcessAuthoringSnapshot* CapturedPostProcess() const;
    EditorRuntimeChangeSet& RuntimeChanges() { return runtimeChanges_; }
    const EditorRuntimeChangeSet& RuntimeChanges() const { return runtimeChanges_; }
    std::size_t ProviderCount() const { return registry_.Count(); }
    const EditorPlayIsolationRegistry& Registry() const { return registry_; }
    const EditorPlaySnapshot& Snapshot() const { return snapshot_; }

    const char* StateLabel() const;

private:
    bool BindTargets(const EditorPlaySessionIsolationSnapshotTarget& target, std::string* errorMessage);

    EditorCoursePlayIsolationProvider courseProvider_{};
    EditorTerrainPlayIsolationProvider terrainProvider_{};
    EditorVfxPlayIsolationProvider vfxProvider_{};
    EditorPostProcessPlayIsolationProvider postProcessProvider_{};
    EditorPlayIsolationRegistry registry_{};
    EditorPlaySnapshot snapshot_{};
    EditorRuntimeChangeSet runtimeChanges_{};
    bool captured_ = false;
    bool restored_ = false;
    uint32_t restoreCount_ = 0;
    uint32_t courseObjectRevision_ = 0;
    std::size_t terrainPlacementCount_ = 0;
    std::size_t rockClusterCount_ = 0;
    bool vfxProviderRegistered_ = false;
    bool postProcessProviderRegistered_ = false;
};

} // namespace editor
