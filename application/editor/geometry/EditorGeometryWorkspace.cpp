#include "EditorGeometryWorkspace.h"

#include "../mesh/EditorProductionMeshEditableSourceMetadata.h"
#include "../scene/EditorScene.h"
#include "../world/SceneWorldObjectProvider.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace editor {
namespace {

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(const Vector3& value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

std::optional<std::string> PropertyValue(
    const EditorSceneComponent& component,
    std::string_view name) {
    const auto found = std::find_if(
        component.properties.begin(), component.properties.end(),
        [&](const EditorSceneProperty& value) { return value.name == name; });
    return found == component.properties.end()
        ? std::optional<std::string>{}
        : std::optional<std::string>{found->value};
}

Vector3 ParseVector(std::string_view text, Vector3 fallback) {
    std::istringstream stream{std::string(text)};
    Vector3 parsed{};
    if (!(stream >> parsed.x >> parsed.y >> parsed.z) ||
        !std::isfinite(parsed.x) || !std::isfinite(parsed.y) || !std::isfinite(parsed.z)) {
        return fallback;
    }
    return parsed;
}

const EditorSceneProperty* FindProperty(
    const EditorSceneComponent& component,
    std::string_view name) {
    const auto found = std::find_if(component.properties.begin(), component.properties.end(),
        [&](const EditorSceneProperty& value) { return value.name == name; });
    return found == component.properties.end() ? nullptr : &*found;
}

bool RayTriangle(
    const EditorViewportWorldRay& ray,
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    float& distance) {
    constexpr float epsilon = 1.0e-6f;
    const Vector3 edgeA = Subtract(b, a);
    const Vector3 edgeB = Subtract(c, a);
    const Vector3 p = Cross(ray.direction, edgeB);
    const float determinant = Dot(edgeA, p);
    if (std::abs(determinant) < epsilon) return false;
    const float inverse = 1.0f / determinant;
    const Vector3 t = Subtract(ray.origin, a);
    const float u = Dot(t, p) * inverse;
    if (u < 0.0f || u > 1.0f) return false;
    const Vector3 q = Cross(t, edgeA);
    const float v = Dot(ray.direction, q) * inverse;
    if (v < 0.0f || u + v > 1.0f) return false;
    const float hit = Dot(edgeB, q) * inverse;
    if (hit <= epsilon) return false;
    distance = hit;
    return true;
}

} // namespace

Vector3 EditorGeometryTransform::TransformPoint(const Vector3& local) const {
    Vector3 value{local.x * scale.x, local.y * scale.y, local.z * scale.z};
    const float cx = std::cos(rotation.x);
    const float sx = std::sin(rotation.x);
    value = {value.x, value.y * cx - value.z * sx, value.y * sx + value.z * cx};
    const float cy = std::cos(rotation.y);
    const float sy = std::sin(rotation.y);
    value = {value.x * cy + value.z * sy, value.y, -value.x * sy + value.z * cy};
    const float cz = std::cos(rotation.z);
    const float sz = std::sin(rotation.z);
    value = {value.x * cz - value.y * sz, value.x * sz + value.y * cz, value.z};
    return Add(value, translation);
}

void EditorGeometryWorkspace::Bind(
    SceneWorldObjectProvider* provider,
    const EditorSelection* selection,
    const EditorDocumentId& activeDocument) {
    EditorObjectHandle nextTarget{};
    std::string nextEntity;
    if (provider != nullptr && selection != nullptr && selection->Primary() != nullptr &&
        provider->Document() == activeDocument) {
        if (const EditorSceneEntity* entity = provider->ResolveEntity(*selection->Primary())) {
            const EditorScene* scene = provider->BoundScene();
            if (scene != nullptr && scene->FindComponent(*entity, kEditorMeshRendererComponentType) != nullptr) {
                nextTarget = *selection->Primary();
                nextEntity = entity->guid;
            }
        }
    }
    if (provider_ != provider || document_ != activeDocument || entityGuid_ != nextEntity) {
        Clear();
        provider_ = provider;
        document_ = activeDocument;
        target_ = std::move(nextTarget);
        entityGuid_ = std::move(nextEntity);
    }
    RefreshFromScene();
}

void EditorGeometryWorkspace::Clear() {
    provider_ = nullptr;
    document_ = {};
    target_ = {};
    entityGuid_.clear();
    meshAssetGuid_.clear();
    authored_.reset();
    preview_.reset();
    collisionPreview_.reset();
    authoredGeometryText_.reset();
    authoredCollisionText_.reset();
    authoredSourceAssetGuid_.reset();
    authoredSourceGeometryHash_.reset();
    transform_ = {};
    selectedFaces_.clear();
}

void EditorGeometryWorkspace::RefreshFromScene() {
    if (provider_ == nullptr || entityGuid_.empty()) return;
    const EditorScene* scene = provider_->BoundScene();
    const EditorSceneEntity* entity = scene != nullptr ? scene->FindEntity(entityGuid_) : nullptr;
    const EditorSceneComponent* meshComponent = entity != nullptr
        ? scene->FindComponent(*entity, kEditorMeshRendererComponentType) : nullptr;
    if (meshComponent == nullptr) {
        Clear();
        return;
    }
    const auto assetReference = std::find_if(
        meshComponent->references.begin(),
        meshComponent->references.end(),
        [](const EditorSceneObjectReference& reference) {
            return reference.property == "asset";
        });
    meshAssetGuid_ = assetReference == meshComponent->references.end()
        ? std::string{}
        : assetReference->assetGuid;
    const std::optional<std::string> geometry =
        PropertyValue(*meshComponent, kEditorEditableGeometryProperty);
    const std::optional<std::string> collision =
        PropertyValue(*meshComponent, kEditorGeneratedCollisionProperty);
    const std::optional<std::string> sourceAssetGuid =
        PropertyValue(
            *meshComponent,
            kEditorEditableSourceAssetGuidProperty);
    const std::optional<std::string> sourceGeometryHash =
        PropertyValue(
            *meshComponent,
            kEditorEditableSourceGeometryHashProperty);
    if (geometry != authoredGeometryText_) {
        authored_.reset();
        if (geometry.has_value()) {
            EditorGeometryMesh decoded;
            if (EditorGeometryMesh::Deserialize(*geometry, decoded, nullptr)) {
                authored_ = std::move(decoded);
            }
        }
        authoredGeometryText_ = geometry;
        preview_.reset();
        collisionPreview_.reset();
        PruneElementSelection();
    }
    authoredCollisionText_ = collision;
    authoredSourceAssetGuid_ = sourceAssetGuid;
    authoredSourceGeometryHash_ = sourceGeometryHash;
    transform_ = {};
    if (const EditorSceneComponent* transform = scene->FindComponent(*entity, kEditorTransformComponentType)) {
        if (const EditorSceneProperty* value = FindProperty(*transform, "translation")) {
            transform_.translation = ParseVector(value->value, {});
        }
        if (const EditorSceneProperty* value = FindProperty(*transform, "rotation")) {
            transform_.rotation = ParseVector(value->value, {});
        }
        if (const EditorSceneProperty* value = FindProperty(*transform, "scale")) {
            transform_.scale = ParseVector(value->value, {1.0f, 1.0f, 1.0f});
        }
    }
}

bool EditorGeometryWorkspace::CanEdit() const noexcept {
    return provider_ != nullptr && document_.IsValid() && !entityGuid_.empty() &&
        !target_.stableId.empty();
}

const EditorGeometryMesh* EditorGeometryWorkspace::AuthoredMesh() const noexcept {
    return authored_.has_value() ? &*authored_ : nullptr;
}

const EditorGeometryMesh* EditorGeometryWorkspace::DisplayMesh() const noexcept {
    if (preview_.has_value()) return &*preview_;
    return AuthoredMesh();
}

EditorGeometryPropertyState EditorGeometryWorkspace::AuthoredState() const {
    return {
        authoredGeometryText_,
        authoredCollisionText_,
        authoredSourceAssetGuid_,
        authoredSourceGeometryHash_};
}

bool EditorGeometryWorkspace::SetPreview(
    EditorGeometryMesh mesh,
    std::string* errorMessage) {
    const EditorGeometryValidationReport validation = mesh.Validate();
    if (!validation.Succeeded()) {
        if (errorMessage != nullptr) *errorMessage = validation.errors.front();
        return false;
    }
    preview_ = std::move(mesh);
    return true;
}

void EditorGeometryWorkspace::SetCollisionPreview(EditorGeneratedCollision collision) {
    collisionPreview_ = collision.Valid()
        ? std::optional<EditorGeneratedCollision>{collision}
        : std::optional<EditorGeneratedCollision>{};
}

void EditorGeometryWorkspace::ClearPreview() {
    preview_.reset();
    collisionPreview_.reset();
}

EditorGeometryPropertyState EditorGeometryWorkspace::PreviewState(
    std::string* errorMessage) const {
    EditorGeometryPropertyState state = AuthoredState();
    if (preview_.has_value()) {
        std::string serialized;
        if (!preview_->Serialize(serialized, errorMessage)) return {};
        state.geometry = std::move(serialized);
        state.collision.reset();
    }
    if (collisionPreview_.has_value()) {
        std::string serialized;
        if (!SerializeEditorGeneratedCollision(*collisionPreview_, serialized)) {
            if (errorMessage != nullptr) *errorMessage = "Generated collision preview is invalid.";
            return {};
        }
        state.collision = std::move(serialized);
    }
    return state;
}

void EditorGeometryWorkspace::SetElementMode(EditorGeometryElementMode mode) {
    if (elementMode_ == mode) return;
    elementMode_ = mode;
    ClearElementSelection();
}

void EditorGeometryWorkspace::SelectFace(std::string guid, bool additive) {
    if (!additive) selectedFaces_.clear();
    const auto found = std::find(selectedFaces_.begin(), selectedFaces_.end(), guid);
    if (found == selectedFaces_.end()) selectedFaces_.push_back(std::move(guid));
    else if (additive) selectedFaces_.erase(found);
}

void EditorGeometryWorkspace::ClearElementSelection() {
    selectedFaces_.clear();
}

void EditorGeometryWorkspace::PruneElementSelection() {
    if (!authored_.has_value()) {
        selectedFaces_.clear();
        return;
    }
    std::erase_if(selectedFaces_, [&](const std::string& guid) {
        return std::none_of(authored_->triangles.begin(), authored_->triangles.end(),
            [&](const EditorGeometryTriangle& triangle) { return triangle.guid == guid; });
    });
}

EditorGeometryFaceHit EditorGeometryWorkspace::PickFace(
    const EditorViewportCoordinateService& coordinates,
    float displayX,
    float displayY) const {
    EditorGeometryFaceHit result{};
    const EditorGeometryMesh* mesh = DisplayMesh();
    if (mesh == nullptr || !coordinates.DisplayPointInside(displayX, displayY)) return result;
    const EditorViewportWorldRay ray = coordinates.DisplayToWorldRay(displayX, displayY);
    if (!ray.valid) return result;
    float nearest = std::numeric_limits<float>::max();
    for (const EditorGeometryTriangle& triangle : mesh->triangles) {
        if (triangle.vertices[0] >= mesh->vertices.size() ||
            triangle.vertices[1] >= mesh->vertices.size() ||
            triangle.vertices[2] >= mesh->vertices.size()) continue;
        const Vector3 a = transform_.TransformPoint(mesh->vertices[triangle.vertices[0]].position);
        const Vector3 b = transform_.TransformPoint(mesh->vertices[triangle.vertices[1]].position);
        const Vector3 c = transform_.TransformPoint(mesh->vertices[triangle.vertices[2]].position);
        float distance = 0.0f;
        if (RayTriangle(ray, a, b, c, distance) && distance < nearest) {
            nearest = distance;
            result.faceGuid = triangle.guid;
            result.position = Add(ray.origin, Scale(ray.direction, distance));
            result.rayDistance = distance;
            result.valid = true;
        }
    }
    return result;
}

} // namespace editor
