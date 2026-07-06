#pragma once

#include <string>

#include "CourseSpawnRuntime.h"
#include "utils/math/Vector.h"

struct BulletPatternAsset {
    std::string id;
    std::string displayName;
    CourseEnemyFirePattern firePattern = CourseEnemyFirePattern::Single;
    int bulletCount = 1;
    float lateralSpreadSpeed = 0.0f;
    float verticalSpreadSpeed = 0.0f;
    float bulletRadius = 0.34f;
    float bulletLifetime = 4.0f;
    float damage = 8.0f;
    Vector4 color{1.0f, 0.18f, 0.08f, 1.0f};

    bool LoadFromFile(const std::string& path, std::string* errorMessage = nullptr);
};
