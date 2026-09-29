#include "RailTitleScene.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float Pi = 3.14159265358979323846f;
Vector3 Add(Vector3 a, Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 Scale(Vector3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
}

bool RailTitleScene::Initialize() {
    travel_ = 0.0;
    starting_ = false; startTime_ = age_ = 0.0f;
    constexpr int segments = 24;
    constexpr float radius = 70.0f;
    const float handle = radius * (4.0f/3.0f) * std::tan(Pi/(2.0f*segments));
    std::vector<RailPathControlPoint> points;
    // Repeat the closed path so the contact solver can sample both sides of
    // the wrap. Only one local rail window is rendered, avoiding overlap.
    for (int i=0;i<=segments*3;++i) {
        const float angle = (i%segments) * (2.0f*Pi/segments);
        RailPathControlPoint p;
        p.position = {radius*std::cos(angle),0.0f,radius*std::sin(angle)};
        p.speed = Speed;
        p.tangentMode = RailPathTangentMode::Mirrored;
        p.outgoingTangent = {-handle*std::sin(angle),0.0f,handle*std::cos(angle)};
        p.incomingTangent = Scale(p.outgoingTangent,-1.0f);
        points.push_back(p);
    }
    path_.SetControlPoints(std::move(points));
    lapStart_ = path_.SegmentStartDistance(segments);
    lapLength_ = path_.SegmentStartDistance(segments*2)-lapStart_;
    auto trackDefinition = CourseRailTrackDefinitionAsset::MineCartDefaults();
    trackDefinition.assetId = "title_loop";
    trackDefinition.renderAheadDistance = 150.0f;
    trackDefinition.renderBehindDistance = 150.0f;
    trackDefinition.nearDetailDistance = 120.0f;
    // Align all authored detail to a complete lap as well as the path itself.
    trackDefinition.bakeSegmentLength = lapLength_ / 120.0f;
    trackDefinition.sleeperSpacing = lapLength_ / 192.0f;
    trackDefinition.supportSpacing = lapLength_ / 48.0f;
    trackDefinition.maximumVisibleInstances = 768;
    if (!track_.Bake(trackDefinition,path_)) return false;
    scenery_ = {};
    const auto place = [this](const char* id, const char* mesh, float fraction,
        float lateral, float vertical, Vector3 scale, float yaw = 0.0f) {
        CourseTerrainPlacement p;
        p.id = id;
        p.meshId = mesh;
        p.distance = lapStart_ + lapLength_*fraction;
        p.lateralOffset = lateral;
        p.verticalOffset = vertical;
        p.scale = scale;
        p.rotation.y = yaw;
        // Static world scenery must not pop when the title distance wraps.
        p.cullAheadDistance = p.cullBehindDistance = 5000.0f;
        scenery_.terrainPlacements.push_back(p);
    };
    // The mesh origin is the centre of the circular course. One connected
    // ground surface replaces the overlapping wall tiles beneath the rails.
    place("title_ground", "title_ground", 0.0f, -70.0f, 0.0f, {1,1,1});
    place("title_central_cliff", "title_cliff", 0.0f, -70.0f, -1.5f, {33,25,30}, 0.35f);
    place("title_distant_cliff_a", "title_cliff", 0.24f, 120.0f, -2.0f, {38,32,29}, 0.8f);
    place("title_distant_cliff_b", "title_cliff", 0.68f, 150.0f, -2.0f, {48,26,35}, -0.6f);
    // Broad gaps and modest scale differences; never a ring of repeated rocks.
    place("title_rock_a", "title_boulder", 0.07f, -26.0f, -0.8f, {3.4f,3.0f,2.7f}, 0.3f);
    place("title_rock_b", "title_boulder", 0.32f, -22.0f, -0.8f, {2.4f,2.0f,3.6f}, 1.0f);
    place("title_rock_c", "title_boulder", 0.57f, -24.0f, -0.8f, {4.0f,3.6f,3.0f}, -0.5f);
    place("title_rock_d", "title_boulder", 0.78f, 42.0f, -1.4f, {5.0f,3.2f,4.2f}, 0.7f);
    place("title_rock_e", "title_boulder", 0.92f, 55.0f, -1.4f, {3.8f,2.4f,5.0f}, -0.8f);
    state_ = {};
    state_.initialized = true;
    state_.vehicleId = definition_.vehicleId;
    state_.hitPoints = definition_.maximumHitPoints;
    state_.speed = Speed;
    Update(0.0f);
    return true;
}

void RailTitleScene::Update(float deltaTime) {
    if (lapLength_ <= 0.0f) return;
    const float dt = std::isfinite(deltaTime) ? (std::clamp)(deltaTime,0.0f,0.05f) : 0.0f;
    travel_ += dt*Speed;
    age_ += dt;
    if(starting_) startTime_ = (std::min)(1.8f,startTime_+dt);
    state_.previousDistance = state_.distance;
    state_.distance = lapStart_ + static_cast<float>(std::fmod(travel_,double(lapLength_)));
    const auto sample = path_.Evaluate(state_.distance);
    state_.position = sample.position;
    state_.forward = sample.tangent;
    state_.right = sample.right;
    state_.up = sample.up;
    ++state_.revision;
    RailVehicleTrackContactPoseInput solve;
    solve.definition = &definition_; solve.state = &state_; solve.railPath = &path_;
    const auto& contact = contacts_.Solve(solve);
    RailVehiclePresentationFrame presentation;
    presentation.visible = true;
    presentation.visualPosition = contact.visualPosition;
    presentation.forward = contact.forward;
    presentation.right = contact.right;
    presentation.up = contact.up;
    // Unwrapped travel prevents wheel phase snapping at the loop boundary.
    presentation.wheelRotationRadians = static_cast<float>(std::fmod(travel_/0.62,2.0*Pi));
    presentation.speedNormalized = Speed/definition_.maximumSpeed;
    presentation.revision = state_.revision;
    presentation.sourceVehicleRevision = state_.revision;
    actor_.Update({&definition_,&state_,&presentation});
    cameraPosition_ = Add(sample.position,Add(Scale(sample.tangent,13.0f),Scale(sample.right,15.0f)));
    cameraPosition_.y += 8.0f;
    // Shift the focal point left in camera space, leaving the cart on the right.
    const Vector3 screenRight = { -sample.tangent.z*13.0f-sample.right.z*15.0f,0.0f,
                                  sample.tangent.x*13.0f+sample.right.x*15.0f };
    cameraTarget_ = Add(contact.visualPosition,Scale(screenRight,-5.5f/std::sqrt(394.0f)));
    cameraTarget_.y += 0.6f;
    if(starting_) {
        const float t = StartProgress();
        const float blend = t*t*t*(t*(t*6.0f-15.0f)+10.0f);
        // Orbit outside the cart rather than interpolating through its body.
        const float angle = 0.8567f + (Pi-0.8567f)*blend;
        const float radius = 19.8494f + (9.5f-19.8494f)*blend;
        cameraPosition_ = Add(sample.position,Add(Scale(sample.tangent,std::cos(angle)*radius),
            Scale(sample.right,std::sin(angle)*radius)));
        cameraPosition_.y += 8.0f+(5.2f-8.0f)*blend;
        const Vector3 chaseTarget = Add(sample.position,Add(Scale(sample.tangent,14.0f),Vector3{0,2.0f,0}));
        cameraTarget_ = Add(Scale(cameraTarget_,1-blend),Scale(chaseTarget,blend));
    }
    renderer_.Update({&actor_.Frame(),cameraPosition_});
    wheels_.Update({&contact,&presentation});
}

int RailTitleScene::HitTest(float x,float y,float width,float height) {
    const float s = (std::min)(width/1600.0f,height/900.0f);
    if (s<=0.0f) return -1;
    x = (x-(width-1600.0f*s)*0.5f)/s;
    y = (y-(height-900.0f*s)*0.5f)/s;
    if (x<96.0f || x>560.0f) return -1;
    for (int i=0;i<3;++i) if (y>=480.0f+i*82.0f && y<=548.0f+i*82.0f) return i;
    return -1;
}

void RailTitleScene::BeginStart() {
    if(starting_) return;
    starting_ = true;
    startTime_ = 0.0f;
}
float RailTitleScene::StartProgress() const { return (std::clamp)(startTime_/1.6f,0.0f,1.0f); }
float RailTitleScene::MenuOpacity() const {
    const float t=(std::clamp)(startTime_/0.35f,0.0f,1.0f);
    return 1.0f-t*t*(3.0f-2.0f*t);
}
float RailTitleScene::Blackout() const {
    const float t=(std::clamp)((startTime_-1.48f)/0.32f,0.0f,1.0f);
    return t*t*(3.0f-2.0f*t);
}
float RailTitleScene::AmbienceGain() const {
    const float t=(std::clamp)(age_/0.8f,0.0f,1.0f);
    return t*t*(3.0f-2.0f*t)*(1.0f-Blackout());
}
