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
    startTravel_ = 0.0; startDistance_ = 0.0f;
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
    trackDefinition.renderAheadDistance = 220.0f;
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
    // Keep this vista behind the longer cave approach at every departure angle.
    place("title_distant_cliff_b", "title_cliff", 0.68f, 220.0f, -8.0f, {48,26,35}, -0.6f);
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
    if(starting_) {
        startTime_ = (std::min)(StartDuration,startTime_+dt);
        // Keep the idle rolling speed throughout the approach.
        travel_=startTravel_+Speed*double(startTime_);
    } else travel_ += dt*Speed;
    age_ += dt;
    state_.speed=CurrentSpeed();
    state_.previousDistance = state_.distance;
    state_.distance = lapStart_ + static_cast<float>(std::fmod(travel_,double(lapLength_)));
    // Continue from the retained curve onto its straight tangent extension.
    if(starting_) state_.distance=startDistance_+static_cast<float>(travel_-startTravel_);
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
    presentation.speedNormalized = (std::min)(1.0f,state_.speed/definition_.maximumSpeed);
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
        // Settle into a close, fixed chase shot without a speed-dependent zoom.
        const float radius = 19.8494f + (ChaseDistance()-19.8494f)*blend;
        cameraPosition_ = Add(sample.position,Add(Scale(sample.tangent,std::cos(angle)*radius),
            Scale(sample.right,std::sin(angle)*radius)));
        cameraPosition_.y += 8.0f+(5.2f-8.0f)*blend;
        // Keep the distant cave centered along the straight departure rail.
        const float remaining=TunnelLead-static_cast<float>(travel_-startTravel_);
        const float lookAhead=(std::clamp)(remaining,14.0f,38.0f);
        const Vector3 chaseTarget = Add(path_.Evaluate(state_.distance+lookAhead).position,Vector3{0,2.0f,0});
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
    const auto departure=path_.Evaluate(state_.distance);
    const float departureYaw=std::atan2(departure.tangent.x,departure.tangent.z);
    // Split the current Bezier segment exactly. Keep every earlier control
    // point so rails behind the cart do not change shape when start is pressed.
    uint32_t segment=0;
    while(segment+1<path_.SegmentCount() && path_.SegmentStartDistance(segment+1)<=state_.distance) ++segment;
    const float t=(std::clamp)((state_.distance-path_.SegmentStartDistance(segment))/path_.SegmentLength(segment),0.0f,1.0f);
    auto retained=path_.ControlPoints();
    const Vector3 p0=retained[segment].position;
    const Vector3 p1=Add(p0,retained[segment].outgoingTangent);
    const Vector3 p3=retained[segment+1].position;
    const Vector3 p2=Add(p3,retained[segment+1].incomingTangent);
    const auto lerp=[t](Vector3 a,Vector3 b){return Add(Scale(a,1-t),Scale(b,t));};
    const Vector3 a=lerp(p0,p1),b=lerp(p1,p2),c=lerp(p2,p3);
    const Vector3 d=lerp(a,b),e=lerp(b,c),cut=lerp(d,e);
    retained.resize(segment+1);
    RailPathControlPoint join;
    join.position=cut;join.speed=Speed;join.tangentMode=RailPathTangentMode::Broken;
    join.incomingTangent=Add(d,Scale(cut,-1));
    if(t>0.000001f) {
        retained.back().tangentMode=RailPathTangentMode::Broken;
        retained.back().outgoingTangent=Add(a,Scale(p0,-1));
        retained.push_back(join);
    }
    // At an exact control point, keep the original incoming curve handle.
    retained.back().tangentMode=RailPathTangentMode::Broken;
    retained.back().outgoingTangent=Scale(departure.tangent,400.0f/3.0f);
    const auto joinIndex=static_cast<uint32_t>(retained.size()-1);
    RailPathControlPoint endPoint;
    endPoint.position=Add(departure.position,Scale(departure.tangent,400.0f));
    endPoint.speed=Speed;endPoint.tangentMode=RailPathTangentMode::Broken;
    endPoint.incomingTangent=Scale(departure.tangent,-400.0f/3.0f);
    retained.push_back(endPoint);
    // Preserve scenery in world space before replacing the looping rail.
    for(auto& p:scenery_.terrainPlacements) {
        const auto old=path_.Evaluate(p.distance);
        const Vector3 world=Add(old.position,Add(Scale(old.right,p.lateralOffset),
            Scale(old.tangent,p.forwardOffset)));
        const Vector3 delta=Add(world,Scale(departure.position,-1));
        p.distance=400.0f;
        p.forwardOffset=delta.x*departure.tangent.x+delta.z*departure.tangent.z;
        p.lateralOffset=delta.x*departure.right.x+delta.z*departure.right.z;
        p.rotation.y+=std::atan2(old.tangent.x,old.tangent.z)-departureYaw;
    }
    std::vector<RailPathControlPoint> straight;
    constexpr float end=400.0f;
    for(float offset:{-end,0.0f,end}) {
        RailPathControlPoint p;
        p.position=Add(departure.position,Scale(departure.tangent,offset));
        p.speed=Speed;
        p.tangentMode=RailPathTangentMode::Mirrored;
        p.incomingTangent=Scale(departure.tangent,-end/3.0f);
        p.outgoingTangent=Scale(departure.tangent,end/3.0f);
        straight.push_back(p);
    }
    // A separate straight coordinate frame keeps scenery fixed in world space,
    // including scenery behind the join where the motion rail is still curved.
    sceneryPath_.SetControlPoints(std::move(straight));
    path_.SetControlPoints(std::move(retained));
    auto trackDefinition=track_.Result().definition;
    trackDefinition.assetId="title_departure";
    track_.Bake(trackDefinition,path_);
    state_.distance=state_.previousDistance=path_.SegmentStartDistance(joinIndex);
    starting_ = true;
    startTime_ = 0.0f;
    startTravel_=travel_; startDistance_=state_.distance;
    CourseTerrainPlacement tunnel;
    tunnel.id="title_start_tunnel"; tunnel.meshId="title_tunnel";
    tunnel.distance=400.0f+TunnelLead;
    tunnel.scale={1,1,1};
    tunnel.cullAheadDistance=tunnel.cullBehindDistance=5000.0f;
    scenery_.terrainPlacements.push_back(tunnel);
}
float RailTitleScene::StartProgress() const { return (std::clamp)(startTime_/OrbitDuration,0.0f,1.0f); }
RailTitleColors RailTitleScene::Colors(Vector4 gameplaySunColor) const {
    RailTitleColors result;
    const auto luminance=[](Vector3 c) {return c.x*0.2126f+c.y*0.7152f+c.z*0.0722f;};
    // Begin after the menu disappears, arrive as the camera finishes its orbit.
    // Darkness is owned solely by Blackout(), never by this color transition.
    const float t=(std::clamp)((startTime_-0.35f)/1.10f,0.0f,1.0f);
    result.transition=t*t*(3.0f-2.0f*t);
    Vector3 target{gameplaySunColor.x,gameplaySunColor.y,gameplaySunColor.z};
    if(!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z) ||
        target.x<0 || target.y<0 || target.z<0 || luminance(target)<0.001f) return result;
    const Vector3 original{result.light.x,result.light.y,result.light.z};
    target=Scale(target,luminance(original)/luminance(target));
    // Only borrow a little of the course hue; keep the title's readable key light.
    const float blend=0.12f+0.28f*result.transition;
    const Vector3 light=Add(Scale(original,1-blend),Scale(target,blend));
    result.light={light.x,light.y,light.z,1};
    // Match the title shader's haze tint, preserving the old background luminance.
    const Vector3 tinted{result.background.x*light.x/original.x,
        result.background.y*light.y/original.y,result.background.z*light.z/original.z};
    result.background=Scale(tinted,luminance(result.background)/luminance(tinted));
    return result;
}
float RailTitleScene::MenuOpacity() const {
    const float t=(std::clamp)(startTime_/0.35f,0.0f,1.0f);
    return 1.0f-t*t*(3.0f-2.0f*t);
}
float RailTitleScene::Blackout() const {
    // Wait until the chase camera has crossed the mouth, not just the cart.
    const float t=(std::clamp)((TunnelCameraDepth()-2.0f)/2.5f,0.0f,1.0f);
    return t*t*(3.0f-2.0f*t);
}
float RailTitleScene::TunnelCameraDepth() const {
    return starting_ ? static_cast<float>(travel_-startTravel_)-TunnelLead-ChaseDistance() : -TunnelLead;
}
float RailTitleScene::TunnelShade() const {
    if(!starting_) return 0.0f;
    const float t=(std::clamp)((static_cast<float>(travel_-startTravel_)-TunnelLead)/12.0f,0.0f,1.0f);
    return t*t*(3.0f-2.0f*t);
}
float RailTitleScene::AmbienceGain() const {
    const float t=(std::clamp)(age_/0.8f,0.0f,1.0f);
    return t*t*(3.0f-2.0f*t)*(1.0f-Blackout());
}
