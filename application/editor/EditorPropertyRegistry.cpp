#include "EditorPropertyRegistry.h"

#include <algorithm>
#include <utility>

namespace editor {
namespace {

EditorPropertyDescriptor MakeDescriptor(
    EditorDomainId domain,
    std::string name,
    std::string displayName,
    EditorPropertyKind kind,
    std::string category,
    std::string valueType,
    float minValue = 0.0f,
    float maxValue = 0.0f,
    bool hasRange = false) {
    EditorPropertyDescriptor descriptor{};
    descriptor.domain = domain;
    descriptor.name = std::move(name);
    descriptor.displayName = std::move(displayName);
    descriptor.kind = kind;
    descriptor.category = std::move(category);
    descriptor.valueType = std::move(valueType);
    descriptor.minValue = minValue;
    descriptor.maxValue = maxValue;
    descriptor.hasRange = hasRange;
    return descriptor;
}

EditorPropertyDescriptor MakeEnumDescriptor(
    EditorDomainId domain,
    std::string name,
    std::string displayName,
    std::string category,
    std::vector<std::string> options) {
    EditorPropertyDescriptor descriptor = MakeDescriptor(
        domain,
        std::move(name),
        std::move(displayName),
        EditorPropertyKind::Enum,
        std::move(category),
        "enum");
    descriptor.enumOptions = std::move(options);
    return descriptor;
}

EditorPropertyDescriptor MakeAssetDescriptor(
    EditorDomainId domain,
    std::string name,
    std::string displayName,
    std::string category,
    EditorAssetKind assetKind) {
    EditorPropertyDescriptor descriptor = MakeDescriptor(
        domain,
        std::move(name),
        std::move(displayName),
        EditorPropertyKind::AssetRef,
        std::move(category),
        ToString(assetKind));
    descriptor.assetKind = assetKind;
    return descriptor;
}

} // namespace

void EditorPropertyRegistry::Clear() {
    if (descriptors_.empty()) {
        return;
    }
    descriptors_.clear();
    Touch();
}

bool EditorPropertyRegistry::Register(EditorPropertyDescriptor descriptor) {
    if (descriptor.name.empty()) {
        return false;
    }

    auto it = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&](const EditorPropertyDescriptor& existing) {
            return existing.domain == descriptor.domain && existing.name == descriptor.name;
        });
    if (it != descriptors_.end()) {
        *it = std::move(descriptor);
        Touch();
        return true;
    }

    descriptors_.push_back(std::move(descriptor));
    Touch();
    return true;
}

const EditorPropertyDescriptor* EditorPropertyRegistry::Find(
    EditorDomainId domain,
    std::string_view name) const {
    const auto it = std::find_if(
        descriptors_.begin(),
        descriptors_.end(),
        [&](const EditorPropertyDescriptor& descriptor) {
            return descriptor.domain == domain && descriptor.name == name;
        });
    return it != descriptors_.end() ? &*it : nullptr;
}

std::vector<const EditorPropertyDescriptor*> EditorPropertyRegistry::FindByDomain(EditorDomainId domain) const {
    std::vector<const EditorPropertyDescriptor*> results;
    for (const EditorPropertyDescriptor& descriptor : descriptors_) {
        if (descriptor.domain == domain) {
            results.push_back(&descriptor);
        }
    }
    return results;
}

void EditorPropertyRegistry::Touch() {
    ++revision_;
}

void RegisterBuiltInCourseObjectProperties(EditorPropertyRegistry& registry) {
    using Kind = EditorPropertyKind;

    registry.Register(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.id", "Id", Kind::String, "Identity", "string"));
    registry.Register(MakeAssetDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.meshId", "Mesh", "Identity", EditorAssetKind::Mesh));
    registry.Register(MakeEnumDescriptor(
        EditorDomainId::CourseTerrainPlacement,
        "CourseTerrainPlacement.layer",
        "Layer",
        "Identity",
        {"gameplay_collision", "hero_landmark", "vista_background"}));
    registry.Register(MakeEnumDescriptor(
        EditorDomainId::CourseTerrainPlacement,
        "CourseTerrainPlacement.collisionMode",
        "Collision",
        "Collision",
        {"none", "proxy", "solid"}));
    registry.Register(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.distance", "Distance", Kind::Float, "Transform", "float", 0.0f, 0.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.lateralOffset", "Lateral", Kind::Float, "Transform", "float", -500.0f, 500.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.verticalOffset", "Vertical", Kind::Float, "Transform", "float", -500.0f, 500.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.forwardOffset", "Forward", Kind::Float, "Transform", "float", -500.0f, 500.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.scale", "Scale", Kind::Vec3, "Transform", "vec3", 0.01f, 200.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.rotation", "Rotation Deg", Kind::Vec3, "Transform", "vec3", -360.0f, 360.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.renderPriority", "Render Priority", Kind::Int, "Rendering", "int", -100.0f, 100.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.cullBehindDistance", "Cull Behind", Kind::Float, "Rendering", "float", -1.0f, 2000.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseTerrainPlacement, "CourseTerrainPlacement.cullAheadDistance", "Cull Ahead", Kind::Float, "Rendering", "float", -1.0f, 3000.0f, true));

    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.id", "Id", Kind::String, "Identity", "string"));
    registry.Register(MakeAssetDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.meshId", "Mesh", "Identity", EditorAssetKind::Mesh));
    registry.Register(MakeEnumDescriptor(
        EditorDomainId::CourseRockCluster,
        "CourseRockCluster.anchor",
        "Anchor",
        "Placement",
        {"left_wall", "right_wall", "floor", "ceiling_break", "vista_wall"}));
    registry.Register(MakeEnumDescriptor(
        EditorDomainId::CourseRockCluster,
        "CourseRockCluster.type",
        "Type",
        "Placement",
        {"attached_debris", "hero_fracture", "falling_debris", "vista_silhouette"}));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.rotation", "Rotation Deg", Kind::Vec3, "Transform", "vec3", -360.0f, 360.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.distance", "Distance", Kind::Float, "Placement", "float", 0.0f, 0.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.count", "Count", Kind::UInt, "Instances", "uint", 0.0f, 32.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.minScale", "Min Scale", Kind::Float, "Instances", "float", 0.01f, 20.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.maxScale", "Max Scale", Kind::Float, "Instances", "float", 0.01f, 20.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.spread", "Spread", Kind::Vec3, "Instances", "vec3", 0.0f, 500.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.clearLaneRadius", "Clear Lane", Kind::Float, "Gameplay", "float", 0.0f, 200.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.cullBehindDistance", "Cull Behind", Kind::Float, "Rendering", "float", 0.0f, 2000.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.cullAheadDistance", "Cull Ahead", Kind::Float, "Rendering", "float", 0.0f, 3000.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.instanceOverrides.index", "Instance Index", Kind::UInt, "Instance Overrides", "uint", 0.0f, 31.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.instanceOverrides.localOffset", "Instance Offset", Kind::Vec3, "Instance Overrides", "vec3", -500.0f, 500.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.instanceOverrides.scale", "Instance Scale", Kind::Vec3, "Instance Overrides", "vec3", 0.01f, 20.0f, true));
    registry.Register(MakeDescriptor(EditorDomainId::CourseRockCluster, "CourseRockCluster.instanceOverrides.rotation", "Instance Rotation", Kind::Vec3, "Instance Overrides", "vec3", -360.0f, 360.0f, true));
}

const char* ToString(EditorPropertyKind kind) {
    switch (kind) {
    case EditorPropertyKind::Bool:
        return "Bool";
    case EditorPropertyKind::Int:
        return "Int";
    case EditorPropertyKind::UInt:
        return "UInt";
    case EditorPropertyKind::Float:
        return "Float";
    case EditorPropertyKind::String:
        return "String";
    case EditorPropertyKind::Vec2:
        return "Vec2";
    case EditorPropertyKind::Vec3:
        return "Vec3";
    case EditorPropertyKind::Vec4:
        return "Vec4";
    case EditorPropertyKind::Color:
        return "Color";
    case EditorPropertyKind::Enum:
        return "Enum";
    case EditorPropertyKind::AssetRef:
        return "AssetRef";
    case EditorPropertyKind::ObjectRef:
        return "ObjectRef";
    }
    return "Unknown";
}

} // namespace editor
