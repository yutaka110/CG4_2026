#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "RailVehicleAudioBridge.h"
#include "RailVehicleCameraMountBridge.h"
#include "RailVehicleCollisionFeedbackBridge.h"
#include "RailVehicleEvasionConstraintResolver.h"
#include "RailVehicleEvasionFeedbackBridge.h"
#include "RailVehicleHitboxProfile.h"
#include "RailVehicleMountedEvasionPresentationBridge.h"
#include "RailVehicleOccupantClearanceSystem.h"
#include "RailVehiclePresentationBridge.h"
#include "RailVehicleRideDynamicsSystem.h"
#include "RailVehicleTrackContactPoseSolver.h"
#include "RailSpeedDirector.h"

inline constexpr uint32_t kRailVehicleControlAssetSchemaVersion = 4;

// One immutable tuning unit for authoritative vehicle motion, mounted evasion,
// collision clearance, camera response and presentation feel. Runtime state is
// deliberately not serialized into this asset.
struct RailVehicleControlDefinitionAsset final {
    uint32_t schemaVersion = kRailVehicleControlAssetSchemaVersion;
    std::string presetId = "mine_cart_standard";
    std::string displayName = "Mine Cart Standard";
    RailVehicleDefinition vehicle = RailVehicleDefinition::MineCartDefaults();
    RailSpeedDirectorSettings speedPolicy{};
    RailVehicleHitboxProfile hitbox =
        RailVehicleHitboxProfile::MineCartDefaults();
    RailVehicleMountedEvasionDefinition evasion =
        RailVehicleMountedEvasionDefinition::MineCartDefaults();
    RailVehicleOccupantClearanceDefinition clearance =
        RailVehicleOccupantClearanceDefinition::MineCartDefaults();
    RailVehicleEvasionConstraintDefinition constraint =
        RailVehicleEvasionConstraintDefinition::MineCartDefaults();
    RailVehicleCameraMountDefinition camera =
        RailVehicleCameraMountDefinition::MineCartDefaults();
    RailVehicleTrackContactPoseSettings trackContact{};
    RailVehiclePresentationSettings vehiclePresentation{};
    RailVehicleRideDynamicsSettings rideDynamics{};
    RailVehicleMountedEvasionPresentationSettings occupantPresentation{};
    RailVehicleAudioSettings vehicleAudio{};
    RailVehicleEvasionFeedbackSettings evasionFeedback{};
    RailVehicleCollisionFeedbackSettings collisionFeedback{};

    bool LoadFromFile(
        const std::filesystem::path& path,
        std::string* errorMessage = nullptr);
    bool Validate(std::string* errorMessage = nullptr) const;
};
