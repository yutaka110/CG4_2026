#pragma once

#include "../../AppRuntimeState.h"
#include "../../EffectSystem.h"
#include "../../PostProcessStack.h"
#include "../../course/CourseAsset.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace editor {

struct EditorRuntimeApplyChange {
    uint64_t sessionSerial = 0;
    bool includesCourse = false;
    CourseAsset beforeCourse;
    CourseAsset afterCourse;
    bool includesTerrain = false;
    TerrainAuthoringState beforeTerrain;
    TerrainAuthoringState afterTerrain;
    bool includesVfxAuthoring = false;
    std::unordered_map<std::string, EffectAsset> beforeVfxAuthoring;
    std::unordered_map<std::string, EffectAsset> afterVfxAuthoring;
    bool includesPostProcess = false;
    std::vector<PostProcessPass> beforePostProcess;
    std::vector<PostProcessPass> afterPostProcess;
};

} // namespace editor
