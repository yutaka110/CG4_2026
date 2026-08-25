#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "utils/math/Vector.h"

inline constexpr uint32_t kCourseRailTrackAssetSchemaVersion = 1;

// Immutable, course-selected presentation contract for the physical rail.
// Gameplay distance remains owned by RailPath; this asset only defines how
// that path is baked and presented.
struct CourseRailTrackDefinitionAsset final {
    uint32_t schemaVersion = kCourseRailTrackAssetSchemaVersion;
    std::string assetId = "mine_cart_standard";
    std::string displayName = "Mine Cart Standard Track";
    bool enabled = true;

    float trackGauge = 3.8f;
    float railHeadWidth = 0.26f;
    float railHeadHeight = 0.20f;
    float railHeadVerticalOffset = 0.0f;
    float bakeSegmentLength = 4.0f;

    float sleeperSpacing = 2.4f;
    float sleeperLength = 5.0f;
    float sleeperWidth = 0.48f;
    float sleeperHeight = 0.18f;
    float sleeperVerticalOffset = -0.19f;

    bool supportsEnabled = true;
    float supportSpacing = 9.6f;
    float supportWidth = 0.30f;
    float supportDepth = 0.34f;
    float supportHeight = 0.55f;

    float renderBehindDistance = 90.0f;
    float renderAheadDistance = 360.0f;
    float nearDetailDistance = 180.0f;
    uint32_t farDetailStride = 4;
    uint32_t maximumVisibleInstances = 512;

    std::string trackUnitMeshId = "course_rail.track_unit";
    std::string wheelProxyMeshId = "course_rail.wheel_proxy";
    float wheelWidth = 0.42f;
    Vector4 railColor{0.20f, 0.24f, 0.28f, 1.0f};
    Vector4 sleeperColor{0.10f, 0.12f, 0.14f, 1.0f};
    Vector4 supportColor{0.075f, 0.09f, 0.11f, 1.0f};
    Vector4 wheelColor{0.055f, 0.065f, 0.075f, 1.0f};

    bool LoadFromFile(
        const std::filesystem::path& path,
        std::string* errorMessage = nullptr);
    bool SaveToFile(
        const std::filesystem::path& path,
        std::string* errorMessage = nullptr) const;
    bool Validate(std::string* errorMessage = nullptr) const;

    static CourseRailTrackDefinitionAsset MineCartDefaults();
};
