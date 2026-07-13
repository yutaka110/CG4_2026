#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "../AppRuntimeState.h"
#include "../course/CourseAsset.h"

struct AppRuntimeState;
struct CourseAsset;

namespace editor {

struct EditorPlaySessionIsolationSnapshotTarget {
    CourseAsset* course = nullptr;
    AppRuntimeState* runtimeState = nullptr;
};

class EditorPlaySessionIsolationSnapshot {
public:
    bool Capture(const EditorPlaySessionIsolationSnapshotTarget& target, std::string* errorMessage = nullptr);
    bool Adopt(const EditorPlaySessionIsolationSnapshotTarget& target, std::string* errorMessage = nullptr);
    bool Restore(const EditorPlaySessionIsolationSnapshotTarget& target, std::string* errorMessage = nullptr);
    void Clear();
    void BindSession(uint64_t sessionSerial);

    bool Captured() const { return captured_; }
    bool Restored() const { return restored_; }
    uint64_t SessionSerial() const { return sessionSerial_; }
    uint32_t RestoreCount() const { return restoreCount_; }
    uint32_t CourseObjectRevision() const { return courseObjectRevision_; }
    std::size_t TerrainPlacementCount() const { return terrainPlacementCount_; }
    std::size_t RockClusterCount() const { return rockClusterCount_; }
    const CourseAsset* CapturedCourse() const { return captured_ ? &course_ : nullptr; }
    const TerrainAuthoringState* CapturedTerrain() const { return captured_ ? &terrain_ : nullptr; }

    const char* StateLabel() const;

private:
    CourseAsset course_{};
    TerrainAuthoringState terrain_{};
    bool captured_ = false;
    bool restored_ = false;
    uint64_t sessionSerial_ = 0;
    uint32_t restoreCount_ = 0;
    uint32_t courseObjectRevision_ = 0;
    std::size_t terrainPlacementCount_ = 0;
    std::size_t rockClusterCount_ = 0;
};

} // namespace editor
