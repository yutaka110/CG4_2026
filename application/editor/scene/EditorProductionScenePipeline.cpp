#include "EditorProductionScenePipeline.h"

#include "../../../externals/DirectXTex/d3dx12.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_set>

namespace editor {
namespace {

constexpr float kEpsilon = 1.0e-6f;

void SetError(std::string* errorMessage, std::string message) {
    if (errorMessage != nullptr) *errorMessage = std::move(message);
}

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Scale(const Vector3& value, float amount) {
    return {value.x * amount, value.y * amount, value.z * amount};
}

float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Cross(const Vector3& a, const Vector3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float Length(const Vector3& value) {
    return std::sqrt(Dot(value, value));
}

Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
    const float length = Length(value);
    return length > kEpsilon ? Scale(value, 1.0f / length) : fallback;
}

Vector3 TransformPoint(const Vector3& value, const Matrix4x4& matrix) {
    const float w = value.x * matrix.m[0][3] + value.y * matrix.m[1][3] +
        value.z * matrix.m[2][3] + matrix.m[3][3];
    const float inverseW = std::abs(w) > kEpsilon ? 1.0f / w : 1.0f;
    return {
        (value.x * matrix.m[0][0] + value.y * matrix.m[1][0] +
            value.z * matrix.m[2][0] + matrix.m[3][0]) * inverseW,
        (value.x * matrix.m[0][1] + value.y * matrix.m[1][1] +
            value.z * matrix.m[2][1] + matrix.m[3][1]) * inverseW,
        (value.x * matrix.m[0][2] + value.y * matrix.m[1][2] +
            value.z * matrix.m[2][2] + matrix.m[3][2]) * inverseW};
}

Vector3 TransformDirection(const Vector3& value, const Matrix4x4& matrix) {
    return {
        value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0],
        value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1],
        value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2]};
}

Vector3 ParseVector(const EditorSceneComponent* component, std::string_view name, Vector3 fallback) {
    if (component == nullptr) return fallback;
    const auto found = std::find_if(component->properties.begin(), component->properties.end(),
        [&](const EditorSceneProperty& property) { return property.name == name; });
    if (found == component->properties.end()) return fallback;
    std::istringstream stream(found->value);
    Vector3 result{};
    if (!(stream >> result.x >> result.y >> result.z) ||
        !std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z)) {
        return fallback;
    }
    return result;
}

std::string AssetReference(const EditorSceneComponent& component) {
    const auto found = std::find_if(component.references.begin(), component.references.end(),
        [](const EditorSceneObjectReference& reference) { return reference.property == "asset"; });
    return found == component.references.end() ? std::string{} : found->assetGuid;
}

void TransformBounds(
    const Vector3& localMin,
    const Vector3& localMax,
    const Matrix4x4& world,
    Vector3& worldMin,
    Vector3& worldMax) {
    worldMin = {(std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)()};
    worldMax = {-(std::numeric_limits<float>::max)(), -(std::numeric_limits<float>::max)(),
        -(std::numeric_limits<float>::max)()};
    for (uint32_t corner = 0; corner < 8; ++corner) {
        const Vector3 local{
            (corner & 1u) != 0 ? localMax.x : localMin.x,
            (corner & 2u) != 0 ? localMax.y : localMin.y,
            (corner & 4u) != 0 ? localMax.z : localMin.z};
        const Vector3 point = TransformPoint(local, world);
        worldMin.x = (std::min)(worldMin.x, point.x);
        worldMin.y = (std::min)(worldMin.y, point.y);
        worldMin.z = (std::min)(worldMin.z, point.z);
        worldMax.x = (std::max)(worldMax.x, point.x);
        worldMax.y = (std::max)(worldMax.y, point.y);
        worldMax.z = (std::max)(worldMax.z, point.z);
    }
}

bool FrustumVisible(const Vector3& boundsMin, const Vector3& boundsMax, const Matrix4x4& viewProjection) {
    std::array<std::array<float, 4>, 8> clip{};
    for (uint32_t corner = 0; corner < 8; ++corner) {
        const Vector3 point{
            (corner & 1u) != 0 ? boundsMax.x : boundsMin.x,
            (corner & 2u) != 0 ? boundsMax.y : boundsMin.y,
            (corner & 4u) != 0 ? boundsMax.z : boundsMin.z};
        clip[corner] = {
            point.x * viewProjection.m[0][0] + point.y * viewProjection.m[1][0] +
                point.z * viewProjection.m[2][0] + viewProjection.m[3][0],
            point.x * viewProjection.m[0][1] + point.y * viewProjection.m[1][1] +
                point.z * viewProjection.m[2][1] + viewProjection.m[3][1],
            point.x * viewProjection.m[0][2] + point.y * viewProjection.m[1][2] +
                point.z * viewProjection.m[2][2] + viewProjection.m[3][2],
            point.x * viewProjection.m[0][3] + point.y * viewProjection.m[1][3] +
                point.z * viewProjection.m[2][3] + viewProjection.m[3][3]};
    }
    const auto allOutside = [&](auto predicate) {
        return std::all_of(clip.begin(), clip.end(), predicate);
    };
    return !(allOutside([](const auto& p) { return p[0] < -p[3]; }) ||
        allOutside([](const auto& p) { return p[0] > p[3]; }) ||
        allOutside([](const auto& p) { return p[1] < -p[3]; }) ||
        allOutside([](const auto& p) { return p[1] > p[3]; }) ||
        allOutside([](const auto& p) { return p[2] < 0.0f; }) ||
        allOutside([](const auto& p) { return p[2] > p[3]; }));
}

bool AabbOverlap(const Vector3& aMin, const Vector3& aMax, const Vector3& bMin, const Vector3& bMax) {
    return aMin.x <= bMax.x && aMax.x >= bMin.x &&
        aMin.y <= bMax.y && aMax.y >= bMin.y &&
        aMin.z <= bMax.z && aMax.z >= bMin.z;
}

bool CollisionBounds(
    const EditorCookedCollisionArtifact& collision,
    Vector3& boundsMin,
    Vector3& boundsMax) {
    if (collision.mode == EditorMeshCollisionBuildMode::Box) {
        boundsMin = Subtract(collision.center, collision.extents);
        boundsMax = Add(collision.center, collision.extents);
        return true;
    }
    if (collision.mode != EditorMeshCollisionBuildMode::TriangleMesh ||
        collision.vertices.empty()) return false;
    boundsMin = collision.vertices.front();
    boundsMax = collision.vertices.front();
    for (const Vector3& vertex : collision.vertices) {
        boundsMin.x = (std::min)(boundsMin.x, vertex.x);
        boundsMin.y = (std::min)(boundsMin.y, vertex.y);
        boundsMin.z = (std::min)(boundsMin.z, vertex.z);
        boundsMax.x = (std::max)(boundsMax.x, vertex.x);
        boundsMax.y = (std::max)(boundsMax.y, vertex.y);
        boundsMax.z = (std::max)(boundsMax.z, vertex.z);
    }
    return true;
}

bool RayAabb(
    const Vector3& origin,
    const Vector3& direction,
    const Vector3& boundsMin,
    const Vector3& boundsMax,
    float maximumDistance,
    float& distance,
    Vector3* normal = nullptr) {
    float nearValue = 0.0f;
    float farValue = maximumDistance;
    Vector3 nearNormal{};
    const float originValues[3]{origin.x, origin.y, origin.z};
    const float directionValues[3]{direction.x, direction.y, direction.z};
    const float minValues[3]{boundsMin.x, boundsMin.y, boundsMin.z};
    const float maxValues[3]{boundsMax.x, boundsMax.y, boundsMax.z};
    for (uint32_t axis = 0; axis < 3; ++axis) {
        if (std::abs(directionValues[axis]) <= kEpsilon) {
            if (originValues[axis] < minValues[axis] || originValues[axis] > maxValues[axis]) return false;
            continue;
        }
        float a = (minValues[axis] - originValues[axis]) / directionValues[axis];
        float b = (maxValues[axis] - originValues[axis]) / directionValues[axis];
        float sign = -1.0f;
        if (a > b) {
            std::swap(a, b);
            sign = 1.0f;
        }
        if (a > nearValue) {
            nearValue = a;
            nearNormal = {};
            if (axis == 0) nearNormal.x = sign;
            if (axis == 1) nearNormal.y = sign;
            if (axis == 2) nearNormal.z = sign;
        }
        farValue = (std::min)(farValue, b);
        if (nearValue > farValue) return false;
    }
    distance = nearValue;
    if (normal != nullptr) *normal = nearNormal;
    return nearValue <= maximumDistance;
}

bool RayTriangle(
    const Vector3& origin,
    const Vector3& direction,
    const Vector3& a,
    const Vector3& b,
    const Vector3& c,
    float maximumDistance,
    float& distance,
    Vector3& normal) {
    const Vector3 edgeA = Subtract(b, a);
    const Vector3 edgeB = Subtract(c, a);
    const Vector3 p = Cross(direction, edgeB);
    const float determinant = Dot(edgeA, p);
    if (std::abs(determinant) <= kEpsilon) return false;
    const float inverse = 1.0f / determinant;
    const Vector3 t = Subtract(origin, a);
    const float u = Dot(t, p) * inverse;
    if (u < 0.0f || u > 1.0f) return false;
    const Vector3 q = Cross(t, edgeA);
    const float v = Dot(direction, q) * inverse;
    if (v < 0.0f || u + v > 1.0f) return false;
    distance = Dot(edgeB, q) * inverse;
    if (distance < 0.0f || distance > maximumDistance) return false;
    normal = NormalizeOr(Cross(edgeA, edgeB), {0.0f, 1.0f, 0.0f});
    return true;
}

D3D12_RESOURCE_DESC BufferDescription(uint64_t bytes) {
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = (std::max)(uint64_t{256}, (bytes + 255ull) & ~255ull);
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return description;
}

bool CreateBuffer(
    ID3D12Device* device,
    D3D12_HEAP_TYPE heapType,
    uint64_t bytes,
    D3D12_RESOURCE_STATES initialState,
    Microsoft::WRL::ComPtr<ID3D12Resource>& output,
    std::string* errorMessage) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = heapType;
    const D3D12_RESOURCE_DESC description = BufferDescription(bytes);
    const HRESULT result = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &description, initialState,
        nullptr, IID_PPV_ARGS(output.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        SetError(errorMessage, "E-6 failed to allocate a D3D12 buffer.");
        return false;
    }
    return true;
}

} // namespace

bool EditorProductionScenePipeline::Initialize(ID3D12Device* device, std::string* errorMessage) {
    if (device == nullptr) {
        SetError(errorMessage, "E-6 requires a D3D12 device.");
        return false;
    }
    if (device_.Get() == device) return true;
    Shutdown();
    device_ = device;
    return true;
}

void EditorProductionScenePipeline::Shutdown() {
    for (auto& [guid, transform] : gpuTransforms_) {
        (void)guid;
        if (transform.resource && transform.mapped != nullptr) transform.resource->Unmap(0, nullptr);
    }
    gpuTransforms_.clear();
    gpuAssets_.clear();
    pendingResources_.clear();
    previousLods_.clear();
    instances_.clear();
    renderPackets_.clear();
    gpuDrivenCandidates_.clear();
    physicsInstances_.clear();
    diagnostics_.clear();
    stats_ = {};
    device_.Reset();
}

uint32_t EditorProductionScenePipeline::SelectLod(
    float distance,
    float boundsRadius,
    uint32_t lodCount,
    uint32_t previousLod) noexcept {
    if (lodCount <= 1) return 0;
    const float safeRadius = (std::max)(0.05f, boundsRadius);
    uint32_t desired = 0;
    for (uint32_t lod = 1; lod < lodCount; ++lod) {
        const float threshold = safeRadius * 12.0f * std::pow(2.0f, static_cast<float>(lod - 1));
        if (distance >= threshold) desired = lod;
    }
    previousLod = (std::min)(previousLod, lodCount - 1);
    if (desired == previousLod) return desired;
    if (desired > previousLod) {
        const float boundary = safeRadius * 12.0f * std::pow(2.0f, static_cast<float>(previousLod));
        return distance >= boundary * 1.1f ? desired : previousLod;
    }
    const float boundary = safeRadius * 12.0f * std::pow(2.0f, static_cast<float>(desired));
    return distance <= boundary * 0.9f ? desired : previousLod;
}

bool EditorProductionScenePipeline::Sync(
    const EditorScene& scene,
    const EditorAssetRegistry& registry,
    EditorProductionMeshRuntimeCache& runtimeCache,
    const Vector3& cameraWorldPosition,
    const Matrix4x4& viewProjection,
    ID3D12GraphicsCommandList* uploadCommandList,
    uint64_t completedFenceValue,
    uint64_t scheduledFenceValue,
    std::string* errorMessage,
    const std::unordered_set<std::string>* sourceResidentEntities,
    const std::unordered_set<std::string>* editorTransientOverrides) {
    CollectRetired(completedFenceValue);
    instances_.clear();
    renderPackets_.clear();
    gpuDrivenCandidates_.clear();
    physicsInstances_.clear();
    diagnostics_.clear();
    const uint64_t priorUploadedBytes = stats_.uploadedGpuBytes;
    stats_ = {};
    stats_.uploadedGpuBytes = priorUploadedBytes;

    std::unordered_map<std::string, Matrix4x4> worldMatrices;
    std::unordered_map<std::string, bool> hierarchyVisibility;
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visibilityVisiting;
    const auto resolveWorld = [&](const auto& self, const EditorSceneEntity& entity) -> Matrix4x4 {
        if (const auto existing = worldMatrices.find(entity.guid); existing != worldMatrices.end()) {
            return existing->second;
        }
        if (!visiting.insert(entity.guid).second) return MakeIdentity4x4();
        const EditorSceneComponent* transform = scene.FindComponent(entity, kEditorTransformComponentType);
        const Vector3 translation = ParseVector(transform, "translation", {});
        const Vector3 rotation = ParseVector(transform, "rotation", {});
        const Vector3 scale = ParseVector(transform, "scale", {1.0f, 1.0f, 1.0f});
        Matrix4x4 world = MakeAffineMatrix(scale, rotation, translation);
        if (!entity.parentGuid.empty()) {
            if (const EditorSceneEntity* parent = scene.FindEntity(entity.parentGuid)) {
                world = Multiply(world, self(self, *parent));
            }
        }
        visiting.erase(entity.guid);
        worldMatrices.insert_or_assign(entity.guid, world);
        return world;
    };
    const auto resolveVisibility = [&](const auto& self, const EditorSceneEntity& entity) -> bool {
        if (const auto existing = hierarchyVisibility.find(entity.guid);
            existing != hierarchyVisibility.end()) return existing->second;
        if (!visibilityVisiting.insert(entity.guid).second) return false;
        bool visible = entity.visible;
        if (visible && !entity.parentGuid.empty()) {
            if (const EditorSceneEntity* parent = scene.FindEntity(entity.parentGuid)) {
                visible = self(self, *parent);
            }
        }
        visibilityVisiting.erase(entity.guid);
        hierarchyVisibility.insert_or_assign(entity.guid, visible);
        return visible;
    };

    std::unordered_set<std::string> activeEntities;
    std::unordered_set<std::string> activeAssets;
    for (const EditorSceneEntity& entity : scene.entities) {
        if (sourceResidentEntities != nullptr &&
            !sourceResidentEntities->contains(entity.guid)) continue;
        const EditorSceneComponent* component = scene.FindComponent(entity, kEditorMeshRendererComponentType);
        if (component == nullptr || !component->enabled) continue;
        ++stats_.meshEntities;
        if (editorTransientOverrides != nullptr &&
            editorTransientOverrides->contains(entity.guid)) continue;
        const std::string assetGuid = AssetReference(*component);
        const EditorAssetRecord* record = registry.FindByGuid(assetGuid);
        if (assetGuid.empty() || record == nullptr || record->kind != EditorAssetKind::Mesh || record->missing) {
            diagnostics_.push_back("Mesh Entity '" + entity.name + "' has no available durable Mesh Asset.");
            continue;
        }
        const EditorProductionMeshRuntimeResource* resource = runtimeCache.Find(assetGuid);
        if (resource == nullptr || resource->sourceTimestamp != record->sourceTimestamp) {
            std::string cacheError;
            if (!runtimeCache.Load(*record, &cacheError)) {
                diagnostics_.push_back("Mesh Asset '" + record->displayName + "' rejected: " + cacheError);
                if (resource == nullptr) continue;
            } else {
                resource = runtimeCache.Find(assetGuid);
            }
        }
        if (resource == nullptr || resource->mesh.lods.empty()) continue;
        const EditorSceneComponent* transformComponent =
            scene.FindComponent(entity, kEditorTransformComponentType);
        const Vector3 instanceScale = ParseVector(
            transformComponent, "scale", {1.0f, 1.0f, 1.0f});
        if (std::abs(instanceScale.x) <= kEpsilon ||
            std::abs(instanceScale.y) <= kEpsilon ||
            std::abs(instanceScale.z) <= kEpsilon) {
            diagnostics_.push_back(
                "Mesh Entity '" + entity.name + "' has a non-invertible Transform scale.");
            continue;
        }

        EditorProductionSceneInstance instance{};
        instance.entityGuid = entity.guid;
        instance.assetGuid = assetGuid;
        instance.world = resolveWorld(resolveWorld, entity);
        Matrix4x4 worldCopy = instance.world;
        instance.inverseWorld = Inverse(worldCopy);
        TransformBounds(resource->mesh.boundsMin, resource->mesh.boundsMax, instance.world,
            instance.boundsMin, instance.boundsMax);
        const Vector3 center = Scale(Add(instance.boundsMin, instance.boundsMax), 0.5f);
        const float radius = Length(Scale(Subtract(instance.boundsMax, instance.boundsMin), 0.5f));
        const float distance = Length(Subtract(center, cameraWorldPosition));
        const uint32_t priorLod = previousLods_.contains(entity.guid) ? previousLods_[entity.guid] : 0;
        instance.selectedLod = SelectLod(distance, radius,
            static_cast<uint32_t>(resource->mesh.lods.size()), priorLod);
        previousLods_[entity.guid] = instance.selectedLod;
        instance.frustumCulled = !FrustumVisible(instance.boundsMin, instance.boundsMax, viewProjection);
        const bool hierarchyVisible = resolveVisibility(resolveVisibility, entity);
        instance.visible = hierarchyVisible && !instance.frustumCulled;
        if (instance.visible) ++stats_.visibleInstances;
        if (hierarchyVisible && instance.frustumCulled) ++stats_.frustumCulledInstances;
        ++stats_.selectedLods[(std::min)(instance.selectedLod,
            EditorMeshBuildSettings::kMaxLods - 1)];
        activeEntities.insert(entity.guid);
        activeAssets.insert(assetGuid);

        const EditorMeshPhysicsResourceView physics = runtimeCache.ResolveForPhysics(assetGuid);
        if (physics.Valid()) {
            Vector3 physicsLocalMin{};
            Vector3 physicsLocalMax{};
            Vector3 physicsWorldMin = instance.boundsMin;
            Vector3 physicsWorldMax = instance.boundsMax;
            if (CollisionBounds(*physics.collision, physicsLocalMin, physicsLocalMax)) {
                TransformBounds(physicsLocalMin, physicsLocalMax, instance.world,
                    physicsWorldMin, physicsWorldMax);
            }
            physicsInstances_.push_back({entity.guid, assetGuid, instance.world, instance.inverseWorld,
                physicsWorldMin, physicsWorldMax, physics.collision});
        }
        instances_.push_back(instance);

        if (!hierarchyVisible || device_ == nullptr || uploadCommandList == nullptr) continue;
        std::string gpuError;
        if (!EnsureGpuAsset(*resource, uploadCommandList, scheduledFenceValue, &gpuError) ||
            !EnsureTransform(entity.guid, instance.world, viewProjection, scheduledFenceValue, &gpuError)) {
            diagnostics_.push_back("Mesh Entity '" + entity.name + "' GPU sync failed: " + gpuError);
            continue;
        }
        const auto asset = gpuAssets_.find(assetGuid);
        const auto transform = gpuTransforms_.find(entity.guid);
        if (asset == gpuAssets_.end() || transform == gpuTransforms_.end() ||
            instance.selectedLod >= asset->second.lods.size()) continue;
        const GpuLod& lod = asset->second.lods[instance.selectedLod];
        for (const GpuSubmesh& submesh : lod.submeshes) {
            EditorProductionSceneRenderPacket packet{entity.guid, assetGuid, instance.selectedLod,
                submesh.materialSlot, submesh.indexCount, lod.vertexBuffer,
                submesh.indexBuffer, transform->second.resource->GetGPUVirtualAddress(),
                center, radius, instance.visible};
            gpuDrivenCandidates_.push_back(packet);
            if (instance.visible) renderPackets_.push_back(std::move(packet));
        }
    }

    for (auto iterator = previousLods_.begin(); iterator != previousLods_.end();) {
        iterator = activeEntities.contains(iterator->first) ? std::next(iterator) : previousLods_.erase(iterator);
    }
    for (auto iterator = gpuTransforms_.begin(); iterator != gpuTransforms_.end();) {
        if (activeEntities.contains(iterator->first)) {
            ++iterator;
            continue;
        }
        if (iterator->second.resource && iterator->second.mapped != nullptr) {
            iterator->second.resource->Unmap(0, nullptr);
            pendingResources_.push_back({iterator->second.resource, scheduledFenceValue});
        }
        iterator = gpuTransforms_.erase(iterator);
    }
    for (auto iterator = gpuAssets_.begin(); iterator != gpuAssets_.end();) {
        if (activeAssets.contains(iterator->first)) {
            ++iterator;
            continue;
        }
        RetireAsset(iterator->second, scheduledFenceValue);
        iterator = gpuAssets_.erase(iterator);
    }
    stats_.renderPackets = static_cast<uint32_t>(renderPackets_.size());
    stats_.physicsInstances = static_cast<uint32_t>(physicsInstances_.size());
    stats_.residentGpuAssets = static_cast<uint32_t>(gpuAssets_.size());
    stats_.pendingGpuRetirements = static_cast<uint32_t>(pendingResources_.size());
    for (const auto& [guid, asset] : gpuAssets_) {
        (void)guid;
        for (const GpuLod& lod : asset.lods) stats_.residentGpuBytes += lod.bytes;
    }
    if (!diagnostics_.empty() && errorMessage != nullptr) *errorMessage = diagnostics_.front();
    return true;
}

bool EditorProductionScenePipeline::EnsureGpuAsset(
    const EditorProductionMeshRuntimeResource& resource,
    ID3D12GraphicsCommandList* commandList,
    uint64_t scheduledFenceValue,
    std::string* errorMessage) {
    const uint64_t buildHash = resource.mesh.buildSettingsHash;
    if (const auto found = gpuAssets_.find(resource.assetGuid); found != gpuAssets_.end() &&
        found->second.generation == resource.generation &&
        found->second.sourceTimestamp == resource.sourceTimestamp &&
        found->second.sourceGeometryHash == resource.mesh.sourceGeometryHash &&
        found->second.buildSettingsHash == buildHash) return true;

    GpuAsset built{};
    built.generation = resource.generation;
    built.sourceTimestamp = resource.sourceTimestamp;
    built.sourceGeometryHash = resource.mesh.sourceGeometryHash;
    built.buildSettingsHash = buildHash;
    built.lods.reserve(resource.mesh.lods.size());
    for (const EditorCookedMeshLod& sourceLod : resource.mesh.lods) {
        std::vector<VertexData> vertices;
        vertices.reserve(sourceLod.vertices.size());
        for (const EditorCookedMeshVertex& source : sourceLod.vertices) {
            vertices.push_back({{source.position.x, source.position.y, source.position.z, 1.0f},
                {source.u, source.v}, source.normal});
        }
        const uint64_t vertexBytes = vertices.size() * sizeof(VertexData);
        if (vertexBytes == 0 || sourceLod.indices.empty()) {
            SetError(errorMessage, "E-6 cannot upload an empty LOD.");
            return false;
        }
        std::map<uint32_t, std::vector<uint32_t>> indicesByMaterial;
        for (std::size_t triangle = 0; triangle * 3 + 2 < sourceLod.indices.size(); ++triangle) {
            const uint32_t slot = triangle < sourceLod.materialSlots.size()
                ? sourceLod.materialSlots[triangle] : 0u;
            auto& indices = indicesByMaterial[slot];
            indices.push_back(sourceLod.indices[triangle * 3]);
            indices.push_back(sourceLod.indices[triangle * 3 + 1]);
            indices.push_back(sourceLod.indices[triangle * 3 + 2]);
        }
        if (indicesByMaterial.empty()) {
            SetError(errorMessage, "E-6 LOD has no material submeshes.");
            return false;
        }
        GpuLod lod{};
        Microsoft::WRL::ComPtr<ID3D12Resource> vertexUpload;
        if (!CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_DEFAULT, vertexBytes,
                D3D12_RESOURCE_STATE_COMMON, lod.vertexResource, errorMessage) ||
            !CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_UPLOAD, vertexBytes,
                D3D12_RESOURCE_STATE_GENERIC_READ, vertexUpload, errorMessage)) return false;
        void* mapped = nullptr;
        if (FAILED(vertexUpload->Map(0, nullptr, &mapped))) {
            SetError(errorMessage, "E-6 failed to map the vertex staging buffer.");
            return false;
        }
        std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(vertexBytes));
        vertexUpload->Unmap(0, nullptr);
        D3D12_RESOURCE_BARRIER vertexToCopy = CD3DX12_RESOURCE_BARRIER::Transition(lod.vertexResource.Get(),
            D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(1, &vertexToCopy);
        commandList->CopyBufferRegion(lod.vertexResource.Get(), 0, vertexUpload.Get(), 0, vertexBytes);
        D3D12_RESOURCE_BARRIER vertexToRead = CD3DX12_RESOURCE_BARRIER::Transition(lod.vertexResource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        commandList->ResourceBarrier(1, &vertexToRead);
        lod.vertexBuffer = {lod.vertexResource->GetGPUVirtualAddress(),
            static_cast<UINT>(vertexBytes), sizeof(VertexData)};
        lod.bytes = vertexBytes;
        for (const auto& [materialSlot, indices] : indicesByMaterial) {
            const uint64_t indexBytes = indices.size() * sizeof(uint32_t);
            GpuSubmesh submesh{};
            Microsoft::WRL::ComPtr<ID3D12Resource> indexUpload;
            if (!CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_DEFAULT, indexBytes,
                    D3D12_RESOURCE_STATE_COMMON, submesh.indexResource, errorMessage) ||
                !CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_UPLOAD, indexBytes,
                    D3D12_RESOURCE_STATE_GENERIC_READ, indexUpload, errorMessage)) return false;
            if (FAILED(indexUpload->Map(0, nullptr, &mapped))) {
                SetError(errorMessage, "E-6 failed to map a submesh index staging buffer.");
                return false;
            }
            std::memcpy(mapped, indices.data(), static_cast<std::size_t>(indexBytes));
            indexUpload->Unmap(0, nullptr);
            D3D12_RESOURCE_BARRIER indexToCopy = CD3DX12_RESOURCE_BARRIER::Transition(
                submesh.indexResource.Get(), D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COPY_DEST);
            commandList->ResourceBarrier(1, &indexToCopy);
            commandList->CopyBufferRegion(
                submesh.indexResource.Get(), 0, indexUpload.Get(), 0, indexBytes);
            D3D12_RESOURCE_BARRIER indexToRead = CD3DX12_RESOURCE_BARRIER::Transition(
                submesh.indexResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_INDEX_BUFFER);
            commandList->ResourceBarrier(1, &indexToRead);
            submesh.indexBuffer = {submesh.indexResource->GetGPUVirtualAddress(),
                static_cast<UINT>(indexBytes), DXGI_FORMAT_R32_UINT};
            submesh.materialSlot = materialSlot;
            submesh.indexCount = static_cast<uint32_t>(indices.size());
            submesh.bytes = indexBytes;
            lod.bytes += indexBytes;
            pendingResources_.push_back({indexUpload, scheduledFenceValue});
            lod.submeshes.push_back(std::move(submesh));
        }
        stats_.uploadedGpuBytes += lod.bytes;
        pendingResources_.push_back({vertexUpload, scheduledFenceValue});
        built.lods.push_back(std::move(lod));
    }
    if (auto existing = gpuAssets_.find(resource.assetGuid); existing != gpuAssets_.end()) {
        RetireAsset(existing->second, scheduledFenceValue);
        existing->second = std::move(built);
    } else {
        gpuAssets_.emplace(resource.assetGuid, std::move(built));
    }
    return true;
}

bool EditorProductionScenePipeline::EnsureTransform(
    std::string_view entityGuid,
    const Matrix4x4& world,
    const Matrix4x4& viewProjection,
    uint64_t scheduledFenceValue,
    std::string* errorMessage) {
    const std::string key(entityGuid);
    auto found = gpuTransforms_.find(key);
    if (found == gpuTransforms_.end()) {
        GpuTransform transform{};
        if (!CreateBuffer(device_.Get(), D3D12_HEAP_TYPE_UPLOAD, sizeof(TransformationMatrix),
                D3D12_RESOURCE_STATE_GENERIC_READ, transform.resource, errorMessage)) return false;
        void* mapped = nullptr;
        if (FAILED(transform.resource->Map(0, nullptr, &mapped))) {
            SetError(errorMessage, "E-6 failed to map an Instance transform buffer.");
            return false;
        }
        transform.mapped = static_cast<TransformationMatrix*>(mapped);
        found = gpuTransforms_.emplace(key, std::move(transform)).first;
    }
    (void)scheduledFenceValue;
    found->second.mapped->World = world;
    found->second.mapped->WVP = Multiply(world, viewProjection);
    Matrix4x4 worldCopy = world;
    found->second.mapped->WorldInverseTranspose = Transpose(Inverse(worldCopy));
    return true;
}

void EditorProductionScenePipeline::RetireAsset(GpuAsset& asset, uint64_t fenceValue) {
    for (GpuLod& lod : asset.lods) {
        if (lod.vertexResource) pendingResources_.push_back({lod.vertexResource, fenceValue});
        for (GpuSubmesh& submesh : lod.submeshes) {
            if (submesh.indexResource) pendingResources_.push_back({submesh.indexResource, fenceValue});
        }
    }
    asset.lods.clear();
}

void EditorProductionScenePipeline::CollectRetired(uint64_t completedFenceValue) {
    std::erase_if(pendingResources_, [&](const PendingResource& pending) {
        return pending.retireFenceValue <= completedFenceValue;
    });
}

EditorProductionSceneRayHit EditorProductionScenePipeline::Raycast(
    const Vector3& origin,
    const Vector3& direction,
    float maximumDistance) const {
    EditorProductionSceneRayHit result{};
    const Vector3 normalizedDirection = NormalizeOr(direction, {});
    if (Length(normalizedDirection) <= kEpsilon || maximumDistance <= 0.0f) return result;
    float nearest = maximumDistance;
    for (const EditorProductionScenePhysicsInstance& instance : physicsInstances_) {
        float broadphaseDistance = 0.0f;
        if (!RayAabb(origin, normalizedDirection, instance.boundsMin, instance.boundsMax,
                nearest, broadphaseDistance) || instance.collision == nullptr) continue;
        const Vector3 localOrigin = TransformPoint(origin, instance.inverseWorld);
        const Vector3 localDirection = NormalizeOr(
            TransformDirection(normalizedDirection, instance.inverseWorld), normalizedDirection);
        float localHit = 0.0f;
        Vector3 localNormal{};
        uint32_t triangleIndex = 0;
        bool hit = false;
        if (instance.collision->mode == EditorMeshCollisionBuildMode::Box) {
            const Vector3 localMin = Subtract(instance.collision->center, instance.collision->extents);
            const Vector3 localMax = Add(instance.collision->center, instance.collision->extents);
            hit = RayAabb(localOrigin, localDirection, localMin, localMax,
                maximumDistance, localHit, &localNormal);
        } else if (instance.collision->mode == EditorMeshCollisionBuildMode::TriangleMesh) {
            float nearestLocal = (std::numeric_limits<float>::max)();
            for (std::size_t index = 0; index + 2 < instance.collision->indices.size(); index += 3) {
                const uint32_t ia = instance.collision->indices[index];
                const uint32_t ib = instance.collision->indices[index + 1];
                const uint32_t ic = instance.collision->indices[index + 2];
                if (ia >= instance.collision->vertices.size() || ib >= instance.collision->vertices.size() ||
                    ic >= instance.collision->vertices.size()) continue;
                float candidate = 0.0f;
                Vector3 candidateNormal{};
                if (RayTriangle(localOrigin, localDirection,
                        instance.collision->vertices[ia], instance.collision->vertices[ib],
                        instance.collision->vertices[ic], maximumDistance,
                        candidate, candidateNormal) && candidate < nearestLocal) {
                    nearestLocal = candidate;
                    localHit = candidate;
                    localNormal = candidateNormal;
                    triangleIndex = static_cast<uint32_t>(index / 3);
                    hit = true;
                }
            }
        }
        if (!hit) continue;
        const Vector3 localPosition = Add(localOrigin, Scale(localDirection, localHit));
        const Vector3 worldPosition = TransformPoint(localPosition, instance.world);
        const float worldDistance = Length(Subtract(worldPosition, origin));
        if (worldDistance > nearest) continue;
        nearest = worldDistance;
        result.entityGuid = instance.entityGuid;
        result.assetGuid = instance.assetGuid;
        result.position = worldPosition;
        const Matrix4x4 normalMatrix = Transpose(instance.inverseWorld);
        result.normal = NormalizeOr(
            TransformDirection(localNormal, normalMatrix), {0.0f, 1.0f, 0.0f});
        result.distance = worldDistance;
        result.triangleIndex = triangleIndex;
        result.valid = true;
    }
    return result;
}

std::vector<const EditorProductionScenePhysicsInstance*>
EditorProductionScenePipeline::OverlapAabb(
    const Vector3& boundsMin,
    const Vector3& boundsMax) const {
    std::vector<const EditorProductionScenePhysicsInstance*> result;
    if (boundsMin.x > boundsMax.x || boundsMin.y > boundsMax.y || boundsMin.z > boundsMax.z) return result;
    for (const EditorProductionScenePhysicsInstance& instance : physicsInstances_) {
        if (AabbOverlap(boundsMin, boundsMax, instance.boundsMin, instance.boundsMax)) {
            result.push_back(&instance);
        }
    }
    return result;
}

} // namespace editor
