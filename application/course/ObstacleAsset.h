#pragma once

#include <string>

#include "utils/math/Vector.h"

struct ObstacleAsset {
    std::string id;
    std::string displayName;
    std::string meshId = "animated_cube";
    std::string vfxCueId;
    float lifetime = 12.0f;
    float distanceOffset = 22.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float forwardSpeed = 0.0f;
    float hitPoints = 80.0f;
    bool breakable = true;
    Vector3 halfExtents{3.5f, 3.0f, 3.5f};
    Vector4 color{1.0f, 0.62f, 0.12f, 1.0f};

    bool LoadFromFile(const std::string& path, std::string* errorMessage = nullptr);
};
