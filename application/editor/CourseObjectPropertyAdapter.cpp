#include "CourseObjectPropertyAdapter.h"

#include <algorithm>
#include <cmath>

#include "../AppRuntimeState.h"
#include "../course/CourseAsset.h"

namespace editor {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float DegreesToRadians(float degrees) {
    return degrees * kPi / 180.0f;
}

float RadiansToDegrees(float radians) {
    return radians * 180.0f / kPi;
}

Vector3 RadiansToDegrees(const Vector3& radians) {
    return {
        RadiansToDegrees(radians.x),
        RadiansToDegrees(radians.y),
        RadiansToDegrees(radians.z),
    };
}

Vector3 DegreesToRadians(const Vector3& degrees) {
    return {
        DegreesToRadians(degrees.x),
        DegreesToRadians(degrees.y),
        DegreesToRadians(degrees.z),
    };
}

bool SameFloat(float a, float b) {
    return std::fabs(a - b) <= 0.0001f;
}

bool SameVector3(const Vector3& a, const Vector3& b) {
    return SameFloat(a.x, b.x) && SameFloat(a.y, b.y) && SameFloat(a.z, b.z);
}

void SetError(std::string* errorMessage, const char* message) {
    if (errorMessage != nullptr) {
        *errorMessage = message != nullptr ? message : "";
    }
}

bool IsEditableCourseDomain(EditorDomainId domain) {
    return domain == EditorDomainId::CourseTerrainPlacement ||
        domain == EditorDomainId::CourseRockCluster;
}

CourseTerrainPlacement* FindTerrainPlacement(CourseAsset* course, const EditorObjectHandle& object) {
    if (course == nullptr || object.domain != EditorDomainId::CourseTerrainPlacement) {
        return nullptr;
    }
    const std::size_t index = static_cast<std::size_t>(object.localIndex);
    return index < course->terrainPlacements.size() ? &course->terrainPlacements[index] : nullptr;
}

const CourseTerrainPlacement* FindTerrainPlacement(const CourseAsset* course, const EditorObjectHandle& object) {
    if (course == nullptr || object.domain != EditorDomainId::CourseTerrainPlacement) {
        return nullptr;
    }
    const std::size_t index = static_cast<std::size_t>(object.localIndex);
    return index < course->terrainPlacements.size() ? &course->terrainPlacements[index] : nullptr;
}

CourseRockCluster* FindRockCluster(CourseAsset* course, const EditorObjectHandle& object) {
    if (course == nullptr || object.domain != EditorDomainId::CourseRockCluster) {
        return nullptr;
    }
    const std::size_t index = static_cast<std::size_t>(object.localIndex);
    return index < course->rockClusters.size() ? &course->rockClusters[index] : nullptr;
}

const CourseRockCluster* FindRockCluster(const CourseAsset* course, const EditorObjectHandle& object) {
    if (course == nullptr || object.domain != EditorDomainId::CourseRockCluster) {
        return nullptr;
    }
    const std::size_t index = static_cast<std::size_t>(object.localIndex);
    return index < course->rockClusters.size() ? &course->rockClusters[index] : nullptr;
}

bool AssignString(std::string& target, const std::string& value) {
    if (target == value) {
        return false;
    }
    target = value;
    return true;
}

bool AssignFloat(float& target, float value) {
    if (SameFloat(target, value)) {
        return false;
    }
    target = value;
    return true;
}

bool AssignVector3(Vector3& target, const Vector3& value) {
    if (SameVector3(target, value)) {
        return false;
    }
    target = value;
    return true;
}

bool AssignInt(int& target, int value) {
    if (target == value) {
        return false;
    }
    target = value;
    return true;
}

bool AssignUInt(uint32_t& target, uint32_t value) {
    if (target == value) {
        return false;
    }
    target = value;
    return true;
}

bool ParseTerrainLayer(const std::string& value, CourseTerrainLayer& out) {
    if (value == ToCourseTerrainLayerString(CourseTerrainLayer::GameplayCollision)) {
        out = CourseTerrainLayer::GameplayCollision;
        return true;
    }
    if (value == ToCourseTerrainLayerString(CourseTerrainLayer::HeroLandmark)) {
        out = CourseTerrainLayer::HeroLandmark;
        return true;
    }
    if (value == ToCourseTerrainLayerString(CourseTerrainLayer::VistaBackground)) {
        out = CourseTerrainLayer::VistaBackground;
        return true;
    }
    return false;
}

bool ParseTerrainCollisionMode(const std::string& value, CourseTerrainCollisionMode& out) {
    if (value == ToCourseTerrainCollisionModeString(CourseTerrainCollisionMode::None)) {
        out = CourseTerrainCollisionMode::None;
        return true;
    }
    if (value == ToCourseTerrainCollisionModeString(CourseTerrainCollisionMode::Proxy)) {
        out = CourseTerrainCollisionMode::Proxy;
        return true;
    }
    if (value == ToCourseTerrainCollisionModeString(CourseTerrainCollisionMode::Solid)) {
        out = CourseTerrainCollisionMode::Solid;
        return true;
    }
    return false;
}

bool ParseRockAnchor(const std::string& value, CourseRockClusterAnchor& out) {
    if (value == ToCourseRockClusterAnchorString(CourseRockClusterAnchor::LeftWall)) {
        out = CourseRockClusterAnchor::LeftWall;
        return true;
    }
    if (value == ToCourseRockClusterAnchorString(CourseRockClusterAnchor::RightWall)) {
        out = CourseRockClusterAnchor::RightWall;
        return true;
    }
    if (value == ToCourseRockClusterAnchorString(CourseRockClusterAnchor::Floor)) {
        out = CourseRockClusterAnchor::Floor;
        return true;
    }
    if (value == ToCourseRockClusterAnchorString(CourseRockClusterAnchor::CeilingBreak)) {
        out = CourseRockClusterAnchor::CeilingBreak;
        return true;
    }
    if (value == ToCourseRockClusterAnchorString(CourseRockClusterAnchor::VistaWall)) {
        out = CourseRockClusterAnchor::VistaWall;
        return true;
    }
    return false;
}

bool ParseRockType(const std::string& value, CourseRockClusterType& out) {
    if (value == ToCourseRockClusterTypeString(CourseRockClusterType::AttachedDebris)) {
        out = CourseRockClusterType::AttachedDebris;
        return true;
    }
    if (value == ToCourseRockClusterTypeString(CourseRockClusterType::HeroFracture)) {
        out = CourseRockClusterType::HeroFracture;
        return true;
    }
    if (value == ToCourseRockClusterTypeString(CourseRockClusterType::FallingDebris)) {
        out = CourseRockClusterType::FallingDebris;
        return true;
    }
    if (value == ToCourseRockClusterTypeString(CourseRockClusterType::VistaSilhouette)) {
        out = CourseRockClusterType::VistaSilhouette;
        return true;
    }
    return false;
}

void MarkCourseEdited(AppRuntimeState* runtimeState) {
    if (runtimeState != nullptr) {
        ++runtimeState->terrain.courseObjectEditRevision;
    }
}

} // namespace

CourseObjectPropertyAdapter::CourseObjectPropertyAdapter(
    CourseAsset* course,
    AppRuntimeState* runtimeState)
    : course_(course)
    , runtimeState_(runtimeState) {
}

bool CourseObjectPropertyAdapter::CanAccess(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor) const {
    if (descriptor.domain != object.domain || !IsEditableCourseDomain(object.domain)) {
        return false;
    }
    if (object.domain == EditorDomainId::CourseTerrainPlacement) {
        return FindTerrainPlacement(course_, object) != nullptr;
    }
    if (object.domain == EditorDomainId::CourseRockCluster) {
        return FindRockCluster(course_, object) != nullptr &&
            descriptor.name.find("CourseRockCluster.instanceOverrides.") != 0;
    }
    return false;
}

bool CourseObjectPropertyAdapter::Get(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor,
    EditorPropertyValue& outValue) const {
    if (!CanAccess(object, descriptor)) {
        return false;
    }

    if (const CourseTerrainPlacement* placement = FindTerrainPlacement(course_, object)) {
        const std::string& name = descriptor.name;
        if (name == "CourseTerrainPlacement.id") {
            outValue.stringValue = placement->id;
            return true;
        }
        if (name == "CourseTerrainPlacement.meshId") {
            outValue.stringValue = placement->meshId;
            return true;
        }
        if (name == "CourseTerrainPlacement.layer") {
            outValue.stringValue = ToCourseTerrainLayerString(placement->layer);
            return true;
        }
        if (name == "CourseTerrainPlacement.collisionMode") {
            outValue.stringValue = ToCourseTerrainCollisionModeString(placement->collisionMode);
            return true;
        }
        if (name == "CourseTerrainPlacement.distance") {
            outValue.floatValue = placement->distance;
            return true;
        }
        if (name == "CourseTerrainPlacement.lateralOffset") {
            outValue.floatValue = placement->lateralOffset;
            return true;
        }
        if (name == "CourseTerrainPlacement.verticalOffset") {
            outValue.floatValue = placement->verticalOffset;
            return true;
        }
        if (name == "CourseTerrainPlacement.forwardOffset") {
            outValue.floatValue = placement->forwardOffset;
            return true;
        }
        if (name == "CourseTerrainPlacement.scale") {
            outValue.vec3Value = placement->scale;
            return true;
        }
        if (name == "CourseTerrainPlacement.rotation") {
            outValue.vec3Value = RadiansToDegrees(placement->rotation);
            return true;
        }
        if (name == "CourseTerrainPlacement.renderPriority") {
            outValue.intValue = placement->renderPriority;
            return true;
        }
        if (name == "CourseTerrainPlacement.cullBehindDistance") {
            outValue.floatValue = placement->cullBehindDistance;
            return true;
        }
        if (name == "CourseTerrainPlacement.cullAheadDistance") {
            outValue.floatValue = placement->cullAheadDistance;
            return true;
        }
    }

    if (const CourseRockCluster* cluster = FindRockCluster(course_, object)) {
        const std::string& name = descriptor.name;
        if (name == "CourseRockCluster.id") {
            outValue.stringValue = cluster->id;
            return true;
        }
        if (name == "CourseRockCluster.meshId") {
            outValue.stringValue = cluster->meshId;
            return true;
        }
        if (name == "CourseRockCluster.anchor") {
            outValue.stringValue = ToCourseRockClusterAnchorString(cluster->anchor);
            return true;
        }
        if (name == "CourseRockCluster.type") {
            outValue.stringValue = ToCourseRockClusterTypeString(cluster->type);
            return true;
        }
        if (name == "CourseRockCluster.rotation") {
            outValue.vec3Value = RadiansToDegrees(cluster->rotation);
            return true;
        }
        if (name == "CourseRockCluster.distance") {
            outValue.floatValue = cluster->distance;
            return true;
        }
        if (name == "CourseRockCluster.count") {
            outValue.uintValue = cluster->count;
            return true;
        }
        if (name == "CourseRockCluster.minScale") {
            outValue.floatValue = cluster->minScale;
            return true;
        }
        if (name == "CourseRockCluster.maxScale") {
            outValue.floatValue = cluster->maxScale;
            return true;
        }
        if (name == "CourseRockCluster.spread") {
            outValue.vec3Value = cluster->spread;
            return true;
        }
        if (name == "CourseRockCluster.clearLaneRadius") {
            outValue.floatValue = cluster->clearLaneRadius;
            return true;
        }
        if (name == "CourseRockCluster.cullBehindDistance") {
            outValue.floatValue = cluster->cullBehindDistance;
            return true;
        }
        if (name == "CourseRockCluster.cullAheadDistance") {
            outValue.floatValue = cluster->cullAheadDistance;
            return true;
        }
    }

    return false;
}

bool CourseObjectPropertyAdapter::Set(
    const EditorObjectHandle& object,
    const EditorPropertyDescriptor& descriptor,
    const EditorPropertyValue& value,
    std::string* errorMessage) {
    if (!CanAccess(object, descriptor)) {
        SetError(errorMessage, "Property is not accessible for the selected Course object.");
        return false;
    }

    bool changed = false;
    if (CourseTerrainPlacement* placement = FindTerrainPlacement(course_, object)) {
        const std::string& name = descriptor.name;
        if (name == "CourseTerrainPlacement.id") {
            changed = AssignString(placement->id, value.stringValue);
        } else if (name == "CourseTerrainPlacement.meshId") {
            changed = AssignString(placement->meshId, value.stringValue);
        } else if (name == "CourseTerrainPlacement.layer") {
            CourseTerrainLayer parsed{};
            if (!ParseTerrainLayer(value.stringValue, parsed)) {
                SetError(errorMessage, "Unknown terrain layer value.");
                return false;
            }
            changed = placement->layer != parsed;
            placement->layer = parsed;
        } else if (name == "CourseTerrainPlacement.collisionMode") {
            CourseTerrainCollisionMode parsed{};
            if (!ParseTerrainCollisionMode(value.stringValue, parsed)) {
                SetError(errorMessage, "Unknown collision mode value.");
                return false;
            }
            changed = placement->collisionMode != parsed;
            placement->collisionMode = parsed;
        } else if (name == "CourseTerrainPlacement.distance") {
            changed = AssignFloat(placement->distance, value.floatValue);
        } else if (name == "CourseTerrainPlacement.lateralOffset") {
            changed = AssignFloat(placement->lateralOffset, value.floatValue);
        } else if (name == "CourseTerrainPlacement.verticalOffset") {
            changed = AssignFloat(placement->verticalOffset, value.floatValue);
        } else if (name == "CourseTerrainPlacement.forwardOffset") {
            changed = AssignFloat(placement->forwardOffset, value.floatValue);
        } else if (name == "CourseTerrainPlacement.scale") {
            changed = AssignVector3(placement->scale, value.vec3Value);
        } else if (name == "CourseTerrainPlacement.rotation") {
            changed = AssignVector3(placement->rotation, DegreesToRadians(value.vec3Value));
        } else if (name == "CourseTerrainPlacement.renderPriority") {
            changed = AssignInt(placement->renderPriority, value.intValue);
        } else if (name == "CourseTerrainPlacement.cullBehindDistance") {
            changed = AssignFloat(placement->cullBehindDistance, value.floatValue);
        } else if (name == "CourseTerrainPlacement.cullAheadDistance") {
            changed = AssignFloat(placement->cullAheadDistance, value.floatValue);
        } else {
            SetError(errorMessage, "Unsupported terrain property.");
            return false;
        }
    } else if (CourseRockCluster* cluster = FindRockCluster(course_, object)) {
        const std::string& name = descriptor.name;
        if (name == "CourseRockCluster.id") {
            changed = AssignString(cluster->id, value.stringValue);
        } else if (name == "CourseRockCluster.meshId") {
            changed = AssignString(cluster->meshId, value.stringValue);
        } else if (name == "CourseRockCluster.anchor") {
            CourseRockClusterAnchor parsed{};
            if (!ParseRockAnchor(value.stringValue, parsed)) {
                SetError(errorMessage, "Unknown rock anchor value.");
                return false;
            }
            changed = cluster->anchor != parsed;
            cluster->anchor = parsed;
        } else if (name == "CourseRockCluster.type") {
            CourseRockClusterType parsed{};
            if (!ParseRockType(value.stringValue, parsed)) {
                SetError(errorMessage, "Unknown rock type value.");
                return false;
            }
            changed = cluster->type != parsed;
            cluster->type = parsed;
        } else if (name == "CourseRockCluster.rotation") {
            changed = AssignVector3(cluster->rotation, DegreesToRadians(value.vec3Value));
        } else if (name == "CourseRockCluster.distance") {
            changed = AssignFloat(cluster->distance, value.floatValue);
        } else if (name == "CourseRockCluster.count") {
            changed = AssignUInt(cluster->count, (std::min)(value.uintValue, 32u));
        } else if (name == "CourseRockCluster.minScale") {
            changed = AssignFloat(cluster->minScale, value.floatValue);
        } else if (name == "CourseRockCluster.maxScale") {
            changed = AssignFloat(cluster->maxScale, value.floatValue);
        } else if (name == "CourseRockCluster.spread") {
            changed = AssignVector3(cluster->spread, value.vec3Value);
        } else if (name == "CourseRockCluster.clearLaneRadius") {
            changed = AssignFloat(cluster->clearLaneRadius, value.floatValue);
        } else if (name == "CourseRockCluster.cullBehindDistance") {
            changed = AssignFloat(cluster->cullBehindDistance, value.floatValue);
        } else if (name == "CourseRockCluster.cullAheadDistance") {
            changed = AssignFloat(cluster->cullAheadDistance, value.floatValue);
        } else {
            SetError(errorMessage, "Unsupported rock cluster property.");
            return false;
        }
    }

    if (changed) {
        MarkCourseEdited(runtimeState_);
    }
    return true;
}

} // namespace editor
