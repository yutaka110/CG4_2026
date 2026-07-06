#pragma once

#include <vector>

#include "CourseAsset.h"
#include "../terrain/RailPath.h"
#include "utils/math/Vector.h"

struct CourseDebrisRenderInstance {
    std::string id;
    std::string meshId;
    CourseTerrainLayer layer = CourseTerrainLayer::HeroLandmark;
    CourseTerrainCollisionMode collisionMode = CourseTerrainCollisionMode::None;
    Vector3 position = {0.0f, 0.0f, 0.0f};
    Vector3 scale = {1.0f, 1.0f, 1.0f};
    Vector3 rotation = {0.0f, 0.0f, 0.0f};
    float sortDistance = 0.0f;
};

class DebrisCompositionSystem {
public:
    static void BuildVisibleRockInstances(
        const CourseAsset& course,
        float currentDistance,
        const RailPath& railPath,
        std::vector<CourseDebrisRenderInstance>& output);
};
