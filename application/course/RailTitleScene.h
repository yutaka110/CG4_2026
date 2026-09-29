#pragma once

#include "CourseRailTrackMeshBakePipeline.h"
#include "RailVehicleRenderer.h"
#include "RailVehicleWheelContactPresentationBridge.h"

// A presentation-only world: it never advances the playable course or combat.
class RailTitleScene final {
public:
    bool Initialize();
    void Update(float deltaTime);
    void BeginStart();
    bool Starting() const { return starting_; }
    bool ReadyForGameplay() const { return starting_ && startTime_ >= 1.8f; }
    float StartProgress() const;
    float MenuOpacity() const;
    float Blackout() const;
    float AmbienceGain() const;
    float Age() const { return age_; }
    const RailPath& Path() const { return path_; }
    const CourseAsset& Scenery() const { return scenery_; }
    const CourseRailTrackMeshBakeResult& Track() const { return track_.Result(); }
    const RailVehicleRenderFrame& Vehicle() const { return renderer_.Frame(); }
    const RailVehicleWheelContactPresentationFrame& Wheels() const { return wheels_.Frame(); }
    Vector3 CameraPosition() const { return cameraPosition_; }
    Vector3 CameraTarget() const { return cameraTarget_; }
    float Distance() const { return state_.distance; }
    float LapLength() const { return lapLength_; }
    static constexpr float Speed = 12.0f;
    // Shared 1600 x 900 layout coordinates, including letterboxing.
    static int HitTest(float x, float y, float width, float height);
private:
    RailPath path_;
    CourseAsset scenery_;
    CourseRailTrackMeshBakePipeline track_;
    RailVehicleDefinition definition_ = RailVehicleDefinition::MineCartDefaults();
    RailVehicleRuntimeState state_;
    RailVehicleTrackContactPoseSolver contacts_;
    RailVehicleActor actor_;
    RailVehicleRenderer renderer_;
    RailVehicleWheelContactPresentationBridge wheels_;
    double travel_ = 0.0;
    bool starting_ = false;
    float startTime_ = 0.0f;
    float age_ = 0.0f;
    float lapStart_ = 0.0f;
    float lapLength_ = 0.0f;
    Vector3 cameraPosition_{};
    Vector3 cameraTarget_{};
};
